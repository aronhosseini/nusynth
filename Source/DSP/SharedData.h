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
/** Free-form LFO shape: any number of points (2..64), x = 0..1 (sorted), y = 0..1. */
struct LfoCustom
{
    static constexpr int kMax = 64;
    int n = 9;
    float x[kMax] {};
    float y[kMax] {};

    LfoCustom() { setDefault(); }
    void setDefault()
    {
        n = 9;
        for (int i = 0; i < n; ++i) { x[i] = (float) i / 8.0f; y[i] = 0.5f + 0.5f * std::sin (6.28318530718f * x[i]); }
    }
    int add (float px, float py)
    {
        if (n >= kMax) return -1;
        int i = 0;
        while (i < n && x[i] < px) ++i;
        for (int k = n; k > i; --k) { x[k] = x[k - 1]; y[k] = y[k - 1]; }
        x[i] = px; y[i] = py; ++n;
        return i;
    }
    void remove (int i)
    {
        if (n <= 2 || i <= 0 || i >= n - 1) return;       // end points stay
        for (int k = i; k < n - 1; ++k) { x[k] = x[k + 1]; y[k] = y[k + 1]; }
        --n;
    }
    juce::String toString() const
    {
        juce::String s;
        for (int i = 0; i < n; ++i) s << juce::String (x[i], 4) << ":" << juce::String (y[i], 4) << ";";
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
            x[n] = p.upToFirstOccurrenceOf (":", false, false).getFloatValue();
            y[n] = p.fromFirstOccurrenceOf (":", false, false).getFloatValue();
            ++n;
        }
        x[0] = 0.0f; x[n - 1] = 1.0f;
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
    std::array<std::atomic<float>, kNumDests> v;
    LiveMod() { for (auto& a : v) a.store (-1000.0f); }      // -1000 = "not published yet"
};

struct SharedData
{
    LiveMod live;
    LfoShapeBank lfo;
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
