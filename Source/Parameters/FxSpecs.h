#pragma once
#include <juce_core/juce_core.h>
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

/** Effect indices: 0 Reverb, 1 Delay, 2 Chorus, 3 Flanger, 4 Phaser, 5 Distortion, 6 Multiband EQ, 7 Compressor (OTT), 8 Limiter */
inline const std::vector<FxParamSpec>& fxSpecs (int fx)
{
    static const std::array<std::vector<FxParamSpec>, 9> all = []
    {
        std::array<std::vector<FxParamSpec>, 9> a;
        a[0] = { fxF ("Size", 0.0f, 1.0f, 0.6f), fxF ("Damping", 0.0f, 1.0f, 0.5f), fxF ("Width", 0.0f, 1.0f, 1.0f),
                 fxF ("Pre-Delay", 0.0f, 200.0f, 10.0f, 0.0f, " ms"), fxF ("Low Cut", 20.0f, 1000.0f, 20.0f, 200.0f, " Hz"),
                 fxF ("High Cut", 1000.0f, 20000.0f, 12000.0f, 5000.0f, " Hz") };
        a[1] = { fxF ("Time L", 1.0f, 2000.0f, 250.0f, 300.0f, " ms"), fxF ("Time R", 1.0f, 2000.0f, 375.0f, 300.0f, " ms"),
                 fxF ("Feedback", 0.0f, 0.95f, 0.4f), fxF ("Tone", 200.0f, 20000.0f, 8000.0f, 3000.0f, " Hz"),
                 fxB ("Ping-Pong", false) };
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
        a[7] = { fxF ("Depth", 0.0f, 1.0f, 1.0f), fxF ("Time", 0.0f, 1.0f, 0.5f), fxF ("Low Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB"),
                 fxF ("Mid Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB"), fxF ("High Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB") };
        a[8] = { fxF ("Threshold", -24.0f, 0.0f, -1.0f, 0.0f, " dB"), fxF ("Release", 1.0f, 500.0f, 50.0f, 50.0f, " ms"),
                 fxF ("Gain", -12.0f, 12.0f, 0.0f, 0.0f, " dB") };
        return a;
    }();
    return all[(size_t) fx];
}

inline bool fxHasMix (int fx) { return fx != 6 && fx != 8; }
inline float fxDefaultMix (int fx)
{
    static const float m[9] = { 0.3f, 0.3f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f };
    return m[fx];
}
}
