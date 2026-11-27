#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cmath>
#include "ModDestinations.h"

namespace nusynth
{
/** Free-form LFO shape: any number of points (2..64), x = 0..1 (sorted), y = 0..1.
    curve[i] bends the line from point i to point i+1: 0 = straight, > 0 = eases in, < 0 = eases out (-1..1). */
struct LfoCustom
{
    static constexpr int kMax = 64;
    int n = 3;
    float x[kMax] {};
    float y[kMax] {};
    float curve[kMax] {};

    LfoCustom() { setDefault(); }
    /** The starting shape of every LFO: three points, bottom-left -> top-centre -> bottom-right, straight lines (a triangle). */
    void setDefault()
    {
        n = 3;
        x[0] = 0.0f; y[0] = 0.0f; curve[0] = 0.0f;
        x[1] = 0.5f; y[1] = 1.0f; curve[1] = 0.0f;
        x[2] = 1.0f; y[2] = 0.0f; curve[2] = 0.0f;
    }
    int add (float px, float py)
    {
        if (n >= kMax) return -1;
        int i = 0;
        while (i < n && x[i] < px) ++i;
        for (int k = n; k > i; --k) { x[k] = x[k - 1]; y[k] = y[k - 1]; curve[k] = curve[k - 1]; }
        x[i] = px; y[i] = py; curve[i] = 0.0f; ++n;
        return i;
    }
    void remove (int i)
    {
        if (n <= 2 || i <= 0 || i >= n - 1) return;       // end points stay
        for (int k = i; k < n - 1; ++k) { x[k] = x[k + 1]; y[k] = y[k + 1]; curve[k] = curve[k + 1]; }
        --n;
    }
    juce::String toString() const
    {
        juce::String s;
        for (int i = 0; i < n; ++i)
        {
            s << juce::String (x[i], 4) << ":" << juce::String (y[i], 4);
            if (std::abs (curve[i]) > 0.001f) s << ":" << juce::String (curve[i], 3);
            s << ";";
        }
        return s;
    }
    bool fromString (const juce::String& str)
    {
        juce::StringArray parts;
        parts.addTokens (str, ";", "");
        parts.removeEmptyStrings();
        if (parts.size() < 2 || parts.size() > kMax) return false;
        n = 0;
        for (auto& p : parts)
        {
            juce::StringArray f;
            f.addTokens (p, ":", "");
            x[n] = f.size() > 0 ? f[0].getFloatValue() : 0.0f;
            y[n] = f.size() > 1 ? f[1].getFloatValue() : 0.5f;     // (older presets have only x:y)
            curve[n] = f.size() > 2 ? juce::jlimit (-1.0f, 1.0f, f[2].getFloatValue()) : 0.0f;
            ++n;
        }
        x[0] = 0.0f; x[n - 1] = 1.0f; curve[n - 1] = 0.0f;
        return true;
    }
};

/** Triple-buffered so the audio thread never reads a half-written shape. */
class LfoShapeBank
{
public:
    LfoShapeBank() { for (auto& a : active) a.store (0); }
    void set (int l, const LfoCustom& c)
    {
        const int t = (active[(size_t) l].load() + 1) % 3;
        buf[(size_t) l][(size_t) t] = c;
        active[(size_t) l].store (t, std::memory_order_release);
    }
    const LfoCustom& get (int l) const noexcept { return buf[(size_t) l][(size_t) active[(size_t) l].load (std::memory_order_acquire)]; }
private:
    std::array<std::array<LfoCustom, 3>, kNumLfos> buf;
    std::array<std::atomic<int>, kNumLfos> active;
};

/** Values the UI shows "live": the modulated value of every destination for the newest voice. */
struct LiveMod
{
    std::array<std::atomic<float>, kNumDests> v;       // modulated value (UI display)
    std::array<std::atomic<float>, kNumDests> mod;     // normalised modulation offset for effect parameters
    LiveMod() { for (auto& a : v) a.store (-1000.0f); for (auto& a : mod) a.store (0.0f); }      // -1000 = "not published yet"
};

/** What the dynamics effects publish for their level displays. The audio thread keeps the MAX since the UI last looked
    (the UI reads with exchange (0)), so no short peak is ever missed.  Index: 0 = Compressor (OTT), 1 = Limiter, 2 = Compressor (Analog). */
struct FxMeters
{
    static constexpr int kCount = 3;
    std::array<std::atomic<float>, kCount> inPeak, outPeak;     // linear peak level
    std::array<std::atomic<float>, kCount> reduction, boost;    // gain change in dB (both stored as positive numbers)
    FxMeters() { for (int i = 0; i < kCount; ++i) { inPeak[(size_t) i].store (0.0f); outPeak[(size_t) i].store (0.0f); reduction[(size_t) i].store (0.0f); boost[(size_t) i].store (0.0f); } }
};

struct SharedData
{
    LiveMod live;
    FxMeters fxMeter;
    LfoShapeBank lfo;
    std::atomic<float> bpm { 120.0f };     // host tempo (for the tempo-synced displays)
    std::function<void()> onUiEdit;        // set by the processor (marks the undo history dirty)
};

inline std::mutex& sharedMutex() { static std::mutex m; return m; }
inline std::unordered_map<const void*, SharedData*>& sharedMap() { static std::unordered_map<const void*, SharedData*> m; return m; }
inline void registerShared (const void* key, SharedData* d) { std::lock_guard<std::mutex> l (sharedMutex()); sharedMap()[key] = d; }
inline void unregisterShared (const void* key) { std::lock_guard<std::mutex> l (sharedMutex()); sharedMap().erase (key); }
inline SharedData* sharedFor (const void* key)
{
    std::lock_guard<std::mutex> l (sharedMutex());
    auto it = sharedMap().find (key);
    return it == sharedMap().end() ? nullptr : it->second;
}
}
