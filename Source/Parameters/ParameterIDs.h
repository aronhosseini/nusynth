#pragma once
#include <juce_core/juce_core.h>

namespace nusynth
{
    inline constexpr int kNumOsc = 3;
    inline constexpr int kNumLfos = 6;
    inline constexpr int kNumEnvs = 4;
    inline constexpr int kNumModSlots = 16;
    inline constexpr int kNumFxSlots = 9;
    inline constexpr int kNumMicroKeys = 12;
    inline constexpr int kNumVoices = 16;
    inline constexpr int kLfoPoints = 16;
    inline constexpr int kMaxFxParams = 9;
    inline constexpr int kNumMacros = 8;
}

namespace nusynth::ids
{
    // ---- Global ----
    inline const juce::String masterGainDb  { "master_gain_db" };
    inline const juce::String playMode      { "play_mode" };
    inline const juce::String glideTime     { "glide_time" };
    inline const juce::String glideCurve    { "glide_curve" };
    inline const juce::String masterTune    { "master_tune" };
    inline const juce::String maxVoices     { "max_voices" };
    inline const juce::String octaveShift   { "octave_shift" };
    inline const juce::String subOn         { "sub_on" };
    inline const juce::String noiseOn       { "noise_on" };
    inline const juce::String noiseLoop     { "noise_loop" };
    inline const juce::String bendUp        { "bend_up" };
    inline const juce::String bendDown      { "bend_down" };

    // ---- Sub oscillator ----
    inline const juce::String subWave       { "sub_wave" };
    inline const juce::String subOctave     { "sub_octave" };
    inline const juce::String subLevel      { "sub_level" };
    inline const juce::String subPan        { "sub_pan" };

    // ---- Noise oscillator ----
    inline const juce::String noiseType     { "noise_type" };
    inline const juce::String noiseCutoff   { "noise_cutoff" };
    inline const juce::String noiseHP       { "noise_hp" };
    inline const juce::String noiseLP       { "noise_lp" };
    inline const juce::String noiseLevel    { "noise_level" };
    inline const juce::String noisePitch    { "noise_pitch" };
    inline const juce::String noiseFine     { "noise_fine" };
    inline const juce::String noisePan      { "noise_pan" };

    // ---- Arpeggiator ----
    inline const juce::String arpOn         { "arp_on" };
    inline const juce::String arpPattern    { "arp_pattern" };
    inline const juce::String arpOctaves    { "arp_octaves" };
    inline const juce::String arpGate       { "arp_gate" };
    inline const juce::String arpSwing      { "arp_swing" };
    inline const juce::String arpRate       { "arp_rate" };

    // ---- Parametrised IDs ----
    inline juce::String osc (int i, const juce::String& p)    { static const char* pre[3] = { "oscA_", "oscB_", "oscC_" }; return juce::String (pre[i]) + p; }
    inline juce::String filter (int i, const juce::String& p) { return "filt" + juce::String (i + 1) + "_" + p; }
    inline juce::String route (const juce::String& source)    { return "route_" + source; }
    inline juce::String lfo (int i, const juce::String& p)    { return "lfo" + juce::String (i + 1) + "_" + p; }
    inline juce::String lfoPt (int i, int k)                  { return "lfo" + juce::String (i + 1) + "_pt" + juce::String (k); }
    inline juce::String env (int i, const juce::String& p)    { return "env" + juce::String (i + 1) + "_" + p; }
    inline juce::String mod (int i, const juce::String& p)    { return "mod" + juce::String (i + 1) + "_" + p; }
    inline juce::String fx (int i, const juce::String& p)     { return "fx" + juce::String (i + 1) + "_" + p; }
    inline juce::String fxp (int i, int k)                    { return "fx" + juce::String (i + 1) + "_p" + juce::String (k); }
    inline juce::String macro (int i)                         { return "macro_" + juce::String (i + 1); }
    inline juce::String micro (int i)                         { return "micro_" + juce::String (i); }
}
