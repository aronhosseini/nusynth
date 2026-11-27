#pragma once
#include <juce_core/juce_core.h>
#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include <array>
#include <vector>

namespace nusynth
{
/** Describes one effect parameter. Used by BOTH the parameter layout and the FX rack UI. */
struct FxParamSpec
{
    enum Kind { Float, Choice, Bool };
    Kind kind = Float;
    juce::String name, unit;
    float lo = 0.0f, hi = 1.0f, def = 0.0f, skew = 0.0f;   // skew = centre value (0 = linear)
    juce::StringArray choices;
};

inline FxParamSpec fxF (const char* n, float lo, float hi, float def, float skew = 0.0f, const char* unit = "")
{
    FxParamSpec s; s.kind = FxParamSpec::Float; s.name = n; s.lo = lo; s.hi = hi; s.def = def; s.skew = skew; s.unit = unit;
    return s;
}
inline FxParamSpec fxC (const char* n, const juce::StringArray& ch, int def)
{
    FxParamSpec s; s.kind = FxParamSpec::Choice; s.name = n; s.choices = ch; s.def = (float) def; s.hi = (float) (ch.size() - 1);
    return s;
}
inline FxParamSpec fxB (const char* n, bool def)
{
    FxParamSpec s; s.kind = FxParamSpec::Bool; s.name = n; s.def = def ? 1.0f : 0.0f;
    return s;
}

/** Tempo-synced delay times (in beats; a bar = 4 beats). Order matches the "Sync L / Sync R" choices of the delay. */
inline const juce::StringArray& delaySyncNames()
{
    static const juce::StringArray n { "1/1", "1/2", "1/2 D", "1/2 T", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 D", "1/16 T", "1/32" };
    return n;
}
inline float delaySyncBeats (int i)
{
    static const float b[14] = { 4.0f, 2.0f, 3.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f, 1.0f / 6.0f, 0.125f };
    return b[i < 0 ? 0 : (i > 13 ? 13 : i)];
}

/** Effect types: 0 Reverb, 1 Delay, 2 Chorus, 3 Flanger, 4 Phaser, 5 Distortion, 6 Multiband EQ, 7 Compressor (Single / OTT), 8 Limiter,
    9 (retired), 10 Filter, 11 Hyper / Dimension.   Rack slot = type + 12 * copy. */
inline const std::vector<FxParamSpec>& fxSpecs (int slotOrType)
{
    const int fx = slotOrType % kNumFxTypes;            // accepts a rack slot too (slot = type + 12 * copy)
    static const std::array<std::vector<FxParamSpec>, 12> all = []
    {
        std::array<std::vector<FxParamSpec>, 12> a;
        a[0] = { fxF ("Size", 0.0f, 1.0f, 0.6f), fxF ("Damping", 0.0f, 1.0f, 0.5f), fxF ("Width", 0.0f, 1.0f, 1.0f),
                 fxF ("Pre-Delay", 0.0f, 200.0f, 10.0f, 0.0f, " ms"), fxF ("Low Cut", 20.0f, 1000.0f, 20.0f, 200.0f, " Hz"),
                 fxF ("High Cut", 1000.0f, 20000.0f, 12000.0f, 5000.0f, " Hz"),
                 fxC ("Type", { "Hall", "Room", "Plate", "Cathedral", "Shimmer", "Reverse" }, 0),
                 fxF ("Decay", 0.1f, 12.0f, 12.0f, 2.0f, " s") };
        a[1] = { fxF ("Time L", 1.0f, 2000.0f, 250.0f, 300.0f, " ms"), fxF ("Time R", 1.0f, 2000.0f, 375.0f, 300.0f, " ms"),
                 fxF ("Feedback", 0.0f, 0.95f, 0.4f), fxF ("Tone", 200.0f, 20000.0f, 8000.0f, 3000.0f, " Hz"),
                 fxC ("Type", { "Stereo", "Ping-Pong", "Tape Echo", "Reverse" }, 0),
                 fxB ("Sync", false), fxC ("Sync L", delaySyncNames(), 7), fxC ("Sync R", delaySyncNames(), 8),
                 fxF ("Low Cut", 20.0f, 1000.0f, 20.0f, 200.0f, " Hz"), fxF ("High Cut", 1000.0f, 20000.0f, 20000.0f, 5000.0f, " Hz") };
        a[2] = { fxF ("Rate", 0.05f, 10.0f, 0.8f, 1.0f, " Hz"), fxF ("Depth", 0.0f, 1.0f, 0.5f), fxF ("Delay", 1.0f, 30.0f, 12.0f, 0.0f, " ms"),
                 fxF ("Feedback", 0.0f, 0.7f, 0.1f), fxF ("Width", 0.0f, 1.0f, 0.8f) };
        a[3] = { fxF ("Rate", 0.05f, 5.0f, 0.3f, 0.8f, " Hz"), fxF ("Depth", 0.0f, 1.0f, 0.7f), fxF ("Delay", 0.1f, 10.0f, 2.0f, 2.0f, " ms"),
                 fxF ("Feedback", -0.95f, 0.95f, 0.5f), fxF ("Width", 0.0f, 1.0f, 0.5f) };
        a[4] = { fxF ("Rate", 0.05f, 10.0f, 0.5f, 1.0f, " Hz"), fxF ("Depth", 0.0f, 1.0f, 0.7f), fxF ("Centre", 100.0f, 8000.0f, 1000.0f, 1000.0f, " Hz"),
                 fxF ("Feedback", 0.0f, 0.95f, 0.4f), fxC ("Stages", { "2", "4", "6", "8", "12" }, 1) };
        a[5] = { fxC ("Type", { "Soft Clip", "Hard Clip", "Tube", "Fold", "Bit Crush", "Tape", "Diode", "Overdrive", "Fuzz", "Sine Shaper", "Chebyshev", "Downsample" }, 0), fxF ("Drive", 0.0f, 40.0f, 12.0f, 0.0f, " dB"),
                 fxF ("Tone", 500.0f, 20000.0f, 12000.0f, 4000.0f, " Hz"), fxF ("Bits", 2.0f, 16.0f, 8.0f),
                 fxF ("Output", -24.0f, 6.0f, -3.0f, 0.0f, " dB") };
        a[6] = { fxF ("Low Freq", 40.0f, 400.0f, 120.0f, 120.0f, " Hz"), fxF ("Low Gain", -18.0f, 18.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Mid Freq", 200.0f, 8000.0f, 1000.0f, 1500.0f, " Hz"), fxF ("Mid Gain", -18.0f, 18.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Mid Q", 0.2f, 10.0f, 1.0f, 1.0f), fxF ("High Freq", 2000.0f, 16000.0f, 6000.0f, 6000.0f, " Hz"),
                 fxF ("High Gain", -18.0f, 18.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Low Cut", 20.0f, 1000.0f, 20.0f, 150.0f, " Hz"), fxF ("High Cut", 2000.0f, 20000.0f, 20000.0f, 8000.0f, " Hz") };
        // 7: Compressor.  p0..p5 = OTT (multiband up / down) - same positions as the old "Compressor (OTT)", p6 = mode, p7..p12 = single-band analog compressor
        a[7] = { fxF ("Depth", 0.0f, 1.0f, 1.0f), fxF ("Time", 0.0f, 1.0f, 0.5f), fxF ("Low Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Mid Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB"), fxF ("High Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Gain", -12.0f, 18.0f, 0.0f, 0.0f, " dB"),
                 fxC ("Mode", { "Single", "OTT" }, 1),
                 fxF ("Threshold", -48.0f, 0.0f, -18.0f, 0.0f, " dB"), fxF ("Ratio", 1.0f, 20.0f, 4.0f, 4.0f, " :1"),
                 fxF ("Attack", 0.1f, 100.0f, 10.0f, 10.0f, " ms"), fxF ("Release", 10.0f, 1000.0f, 120.0f, 120.0f, " ms"),
                 fxF ("Knee", 0.0f, 24.0f, 6.0f, 0.0f, " dB"), fxF ("Makeup", -12.0f, 24.0f, 0.0f, 0.0f, " dB") };
        a[8] = { fxF ("Threshold", -24.0f, 0.0f, -1.0f, 0.0f, " dB"), fxF ("Release", 1.0f, 500.0f, 50.0f, 50.0f, " ms"),
                 fxF ("Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB"),
                 // added later (appended so every older preset keeps working):
                 fxF ("Input", -12.0f, 24.0f, 0.0f, 0.0f, " dB"), fxF ("Ceiling", -12.0f, 0.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Soft Clip", 0.0f, 1.0f, 0.0f), fxF ("Width", 0.0f, 2.0f, 1.0f) };
        // 9: retired (the analog compressor is now the "Single" mode of the Compressor, type 7)
        {   // 10: Filter - the same 30 filter types as the main filter
            const auto names = filterTypeNames();
            const int def = juce::jmax (0, names.indexOf ("Low Pass 24"));
            a[10] = { fxC ("Type", names, def), fxF ("Cutoff", 20.0f, 20000.0f, 2000.0f, 1000.0f, " Hz"),
                      fxF ("Resonance", 0.0f, 1.0f, 0.25f), fxF ("Drive", 0.0f, 1.0f, 0.0f) };
        }
        // 11: Hyper / Dimension (Serum 2 style): Hyper = detuned unison voices spread over the stereo field, Dimension = wide mid/side chorus
        a[11] = { fxF ("Rate", 0.05f, 8.0f, 0.6f, 1.0f, " Hz"), fxF ("Detune", 0.0f, 1.0f, 0.5f),
                  fxC ("Unison", { "2", "3", "4", "5", "6", "7" }, 2), fxF ("Width", 0.0f, 1.0f, 0.8f),
                  fxF ("Hyper", 0.0f, 1.0f, 0.6f), fxF ("Size", 0.0f, 1.0f, 0.5f), fxF ("Dimension", 0.0f, 1.0f, 0.5f) };
        return a;
    }();
    return all[(size_t) fx];
}

/** Effects 0..9 existed before the later additions; their ORIGINAL parameter counts. New parameters (and the Filter effect)
    are appended to the end of the modulation destination list so every saved modulation routing keeps working. */
inline constexpr int kNumLegacyFx = 10;
inline int fxLegacyParams (int fx)
{
    static const int n[kNumLegacyFx] = { 7, 8, 5, 5, 5, 5, 9, 6, 3, 0 };
    return fx >= 0 && fx < kNumLegacyFx ? n[fx] : 0;
}

inline bool fxHasMix (int slotOrType) { const int fx = slotOrType % kNumFxTypes; return fx != 6 && fx != 8 && fx != 9; }

inline float fxDefaultMix (int slotOrType)
{
    const int fx = slotOrType % kNumFxTypes;
    static const float m[12] = { 0.3f, 0.3f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    return m[fx];
}
}

namespace nusynth
{
/** Effect types the "+ FX" menu offers (in menu order). */
inline const std::vector<int>& fxMenuTypes() { static const std::vector<int> t = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11 }; return t; }
}
