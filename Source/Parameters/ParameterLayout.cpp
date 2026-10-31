#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include "FxSpecs.h"
#include "DSP/ModDestinations.h"
#include <cmath>

namespace nusynth
{
namespace
{
    constexpr int kVersion = 1;
    using Params = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;

    /** All floats display with at most two decimals (e.g. 0.01), plus an optional unit suffix. */
    void addFloat (Params& p, const juce::String& id, const juce::String& name,
                   float lo, float hi, float def, float skewCentre = 0.0f, float interval = 0.0f,
                   const juce::String& unit = {})
    {
        juce::NormalisableRange<float> r (lo, hi, interval);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        auto attrs = juce::AudioParameterFloatAttributes()
                         .withStringFromValueFunction ([unit] (float v, int) { return juce::String (v, 2) + unit; })
                         .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, kVersion }, name, r, def, attrs));
    }

    void addInt (Params& p, const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, kVersion }, name, lo, hi, def));
    }

    void addChoice (Params& p, const juce::String& id, const juce::String& name,
                    const juce::StringArray& choices, int def = 0)
    {
        p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, kVersion }, name, choices, def));
    }

    void addBool (Params& p, const juce::String& id, const juce::String& name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, kVersion }, name, def));
    }
}

// ------------------------------------------------------------------ name tables
juce::StringArray wavetableNames()
{
    return {
        // Classic Analog (0-11)
        "Basic Saw", "Basic Square", "Basic Triangle", "Basic Sine", "Pulse 25", "Pulse 12",
        "Super Saw Stack", "Analog Brass", "Analog Pad", "Analog Strings", "Vintage PWM", "Soft Saw",
        // Digital / Additive (12-23)
        "Digital Bell", "Additive Organ", "Additive Choir", "Harmonic Sweep", "Odd Harmonics", "Even Harmonics",
        "Glass Harmonic", "Spectral Pad", "Digital Edge", "Vocal Formant A", "Vocal Formant B", "Piano Partials",
        // FM (24-35)
        "FM Bell", "FM EP", "FM Bass", "FM Metallic", "FM Clang", "FM Sweep",
        "FM Ratio 1:2", "FM Ratio 1:3", "FM Growl", "FM Pluck", "FM Stack", "FM Chaos",
        // Harsh / Spectral (36-45)
        "Spectral Shred", "Harsh Fold", "Bit Crush", "Aliased Edge", "Rasp",
        "Comb Tear", "Spectral Freeze", "Noise Burst", "Metal Scrape", "Distorted Digital",
        // Wavetable morphs (46-55)
        "Sine to Saw", "Saw to Square", "Square to Pulse", "Tri to Saw", "Organ to Bell",
        "Vowel Morph", "Analog to Digital", "Glass to Metal", "Pad Evolve", "Custom Wave",
        // Vowels (56-64)
        "Vowel A", "Vowel E", "Vowel I", "Vowel O", "Vowel U", "Vowel AEIOU", "Vowel Female", "Vowel Growl", "Vowel Choir Oh",
        // User slots (65-72)
        "User 1", "User 2", "User 3", "User 4", "User 5", "User 6", "User 7", "User 8" };
}

/** 0 Analog, 1 Digital, 2 Vowel, 3 User */
int wavetableCategory (int idx)
{
    if (idx >= 65) return 3;
    if (idx <= 11) return 0;
    if (idx == 14 || idx == 21 || idx == 22 || idx == 51 || (idx >= 56 && idx <= 64)) return 2;
    return 1;
}

juce::StringArray filterTypeNames()
{
    return { "Low Pass 12", "Low Pass 24", "Ladder LP 24", "Ladder LP 12", "MG Low", "SVF Low Pass",
             "High Pass 12", "High Pass 24", "Ladder HP", "SVF High Pass",
             "Band Pass 12", "Band Pass 24", "SVF Band Pass", "Notch 12", "Notch 24", "Peak",
             "Low Shelf", "High Shelf", "Formant I", "Formant II", "Formant III",
             "Comb +", "Comb -", "Flanger Comb", "Phaser 4", "Phaser 8", "Diode Ladder",
             "Dist Comb", "Ring Mod Filter", "Vowel Morph" };
}

juce::StringArray warpModeNames()
{
    return { "Off", "Sync", "Remap", "Bend +", "Bend -", "Bend +/-", "PWM", "Asym +", "Asym -", "Asym +/-",
             "Quantize", "Mirror", "Wavefold", "FM", "AM", "RM" };
}

juce::StringArray routeNames()       { return { "Bypass", "Filter 1", "Filter 2", "Parallel (1+2)", "Series (1 > 2)" }; }
juce::StringArray noiseTypeNames()   { return { "White", "Pink", "Brown", "Analog Tape", "Circuit Noise", "Sample" }; }

juce::StringArray modSourceNames()
{
    return { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "Env 1", "Env 2", "Env 3", "Env 4",
             "Velocity", "Key", "Mod Wheel", "Aftertouch", "Random",
             "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8" };
}

juce::StringArray modDestinationNames()
{
    juce::StringArray s;
    s.add ("None");
    for (const auto& d : destTable()) s.add (d.name);
    return s;
}

juce::StringArray fxNames()
{
    return { "Reverb", "Delay", "Chorus", "Flanger", "Phaser", "Distortion",
             "Multiband EQ", "Compressor (OTT)", "Limiter" };
}

// ------------------------------------------------------------------ layout
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Params p;

    // ---- Global / play modes ----
    addFloat  (p, ids::masterGainDb, "Master", -60.0f, 6.0f, 0.0f, -12.0f, 0.0f, " dB");
    addChoice (p, ids::playMode,  "Play Mode", { "Poly", "Mono", "Legato" });
    addFloat  (p, ids::glideTime, "Glide Time", 0.0f, 5000.0f, 0.0f, 250.0f, 0.0f, " ms");
    addFloat  (p, ids::glideCurve, "Glide Curve", -1.0f, 1.0f, 0.0f);
    addFloat  (p, ids::masterTune, "Master Tune", 400.0f, 480.0f, 440.0f, 0.0f, 0.01f, " Hz");
    addInt    (p, ids::maxVoices, "Polyphony", 1, 16, 16);
    addInt    (p, ids::octaveShift, "Octave", -3, 3, 0);
    addInt    (p, ids::bendUp,   "Pitch Bend Up", 0, 24, 2);
    addInt    (p, ids::bendDown, "Pitch Bend Down", 0, 24, 2);
    for (int m = 0; m < kNumMacros; ++m) addFloat (p, ids::macro (m), "Macro " + juce::String (m + 1), 0.0f, 1.0f, 0.0f);

    // ---- Oscillators A, B & C ----
    for (int o = 0; o < 3; ++o)
    {
        const juce::String n = (o == 0 ? "Osc A " : (o == 1 ? "Osc B " : "Osc C "));
        addInt    (p, ids::osc (o, "pitch"),    n + "Semitones", -48, 48, 0);
        addFloat  (p, ids::osc (o, "fine"),     n + "Fine",     -100.0f, 100.0f, 0.0f, 0.0f, 0.0f, " ct");
        addInt    (p, ids::osc (o, "unison"),   n + "Unison Voices", 1, 16, 1);
        addFloat  (p, ids::osc (o, "spread"),   n + "Unison Spread", 0.0f, 1.0f, 0.5f);
        addFloat  (p, ids::osc (o, "detune"),   n + "Unison Detune", 0.0f, 1.0f, 0.25f);
        addChoice (p, ids::osc (o, "wavetable"),n + "Wavetable", wavetableNames(), 0);
        addFloat  (p, ids::osc (o, "position"), n + "WT Position", 0.0f, 1.0f, 1.0f);
        addChoice (p, ids::osc (o, "warpMode"), n + "Warp Mode", warpModeNames());
        addFloat  (p, ids::osc (o, "warp"),     n + "Warp",  0.0f, 1.0f, 0.0f);
        addChoice (p, ids::osc (o, "warpSrc"),  n + "Warp Source", { "Osc A", "Osc B", "Osc C", "Sub", "Noise" }, o == 0 ? 1 : 0);
        addFloat  (p, ids::osc (o, "phase"),    n + "Phase", 0.0f, 360.0f, 0.0f, 0.0f, 0.0f, " deg");
        addBool   (p, ids::osc (o, "on"),       n + "On", true);
        addFloat  (p, ids::osc (o, "level"),    n + "Level", 0.0f, 1.0f, o == 0 ? 0.8f : 0.0f);
        addFloat  (p, ids::osc (o, "pan"),      n + "Pan", -1.0f, 1.0f, 0.0f);
    }

    // ---- Sub oscillator ----
    addChoice (p, ids::subWave,   "Sub Waveform", { "SIN", "TRI", "SQ", "SAW", "PULSE" });
    addInt    (p, ids::subOctave, "Sub Octave", -4, 4, -1);
    addBool   (p, ids::subOn,     "Sub On", true);
    addFloat  (p, ids::subLevel,  "Sub Level", 0.0f, 1.0f, 0.0f);
    addFloat  (p, ids::subPan,    "Sub Pan", -1.0f, 1.0f, 0.0f);

    // ---- Noise oscillator ----
    addChoice (p, ids::noiseType,   "Noise Type", noiseTypeNames(), 0);
    addFloat  (p, ids::noiseCutoff, "Noise Filter", 20.0f, 20000.0f, 8000.0f, 1000.0f, 0.0f, " Hz");
    addBool   (p, ids::noiseHP,     "Noise HP", false);
    addBool   (p, ids::noiseLP,     "Noise LP", false);
    addFloat  (p, ids::noiseLevel,  "Noise Level", 0.0f, 1.0f, 0.0f);
    addBool   (p, ids::noiseOn,   "Noise On", true);
    addBool   (p, ids::noiseLoop, "Noise Sample Loop", true);
    addInt    (p, ids::noisePitch,  "Noise Pitch", -48, 48, 0);
    addFloat  (p, ids::noiseFine,   "Noise Fine", -100.0f, 100.0f, 0.0f, 0.0f, 0.0f, " ct");
    addFloat  (p, ids::noisePan,    "Noise Pan", -1.0f, 1.0f, 0.0f);

    // ---- Per-source routing matrix ----
    addChoice (p, ids::route ("oscA"),  "Route Osc A", routeNames(), 1);
    addChoice (p, ids::route ("oscB"),  "Route Osc B", routeNames(), 1);
    addChoice (p, ids::route ("oscC"),  "Route Osc C", routeNames(), 1);
    addChoice (p, ids::route ("sub"),   "Route Sub",   routeNames(), 1);
    addChoice (p, ids::route ("noise"), "Route Noise", routeNames(), 1);

    // ---- Dual DSP filters ----
    for (int f = 0; f < 2; ++f)
    {
        const juce::String n = "Filter " + juce::String (f + 1) + " ";
        addBool   (p, ids::filter (f, "on"),   n + "On", f == 0);
        addBool   (p, ids::filter (f, "keytrack"), n + "Key Track", false);
        addChoice (p, ids::filter (f, "type"), n + "Type", filterTypeNames(), 0);
        addFloat  (p, ids::filter (f, "cutoff"), n + "Cutoff", 20.0f, 20000.0f, 20000.0f, 1000.0f, 0.0f, " Hz");
        addFloat  (p, ids::filter (f, "res"),    n + "Resonance", 0.0f, 1.0f, 0.1f);
        addFloat  (p, ids::filter (f, "drive"),  n + "Drive", 0.0f, 1.0f, 0.0f);
        addFloat  (p, ids::filter (f, "mix"),    n + "Mix", 0.0f, 1.0f, 1.0f);
    }

    // ---- LFOs ----
    for (int i = 0; i < kNumLfos; ++i)
    {
        const juce::String n = "LFO " + juce::String (i + 1) + " ";
        addChoice (p, ids::lfo (i, "shape"), n + "Shape", { "Custom", "Sine", "Triangle", "Saw", "Square", "S&H" }, 1);
        addFloat  (p, ids::lfo (i, "rate"),  n + "Rate", 0.01f, 50.0f, 1.0f, 2.0f, 0.0f, " Hz");
        addFloat  (p, ids::lfo (i, "depth"), n + "Depth", 0.0f, 1.0f, 1.0f);
        addFloat  (p, ids::lfo (i, "phase"), n + "Phase", 0.0f, 1.0f, 0.0f);
        addBool   (p, ids::lfo (i, "sync"),  n + "Sync", false);
        addChoice (p, ids::lfo (i, "div"),   n + "Division", { "32 bar", "16 bar", "8 bar", "4 bar", "2 bar", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" }, 7);
        addFloat  (p, ids::lfo (i, "smooth"),n + "Smooth", 0.0f, 1.0f, 0.0f);
    }

    // ---- Envelopes ----
    for (int i = 0; i < kNumEnvs; ++i)
    {
        const juce::String n = "Env " + juce::String (i + 1) + " ";
        addFloat (p, ids::env (i, "attack"),  n + "Attack",  0.0005f, 10.0f, 0.005f, 0.1f, 0.0f, " s");
        addFloat (p, ids::env (i, "decay"),   n + "Decay",   0.0005f, 10.0f, 0.3f,   0.3f, 0.0f, " s");
        addFloat (p, ids::env (i, "sustain"), n + "Sustain", 0.0f, 1.0f, 0.8f);
        addFloat (p, ids::env (i, "release"), n + "Release", 0.0005f, 10.0f, 0.2f,   0.3f, 0.0f, " s");
        addFloat (p, ids::env (i, "aCurve"),  n + "Attack Curve",  -1.0f, 1.0f, 0.0f);
        addFloat (p, ids::env (i, "dCurve"),  n + "Decay Curve",   -1.0f, 1.0f, 0.0f);
        addFloat (p, ids::env (i, "rCurve"),  n + "Release Curve", -1.0f, 1.0f, 0.0f);
    }

    // ---- Arpeggiator ----
    addBool   (p, ids::arpOn,      "Arp On", false);
    addChoice (p, ids::arpPattern, "Arp Pattern", { "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord", "Converge", "Diverge", "Pinky Up", "Thumb Up", "Double Up", "Double Down", "Zigzag", "Random Walk", "Octave Jump", "Cascade" });
    addInt    (p, ids::arpOctaves, "Arp Octaves", 1, 4, 1);
    addFloat  (p, ids::arpGate,    "Arp Gate",  0.05f, 1.0f, 0.8f);
    addFloat  (p, ids::arpSwing,   "Arp Swing", 0.0f, 1.0f, 0.0f);
    addChoice (p, ids::arpRate,    "Arp Rate", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T" }, 4);

    // ---- Mod matrix (16 slots) ----
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const juce::String n = "Mod " + juce::String (i + 1) + " ";
        addChoice (p, ids::mod (i, "src"),    n + "Source", modSourceNames(), 0);
        addChoice (p, ids::mod (i, "dst"),    n + "Destination", modDestinationNames(), 0);
        addFloat  (p, ids::mod (i, "amount"), n + "Amount", -1.0f, 1.0f, 0.0f);
        addFloat  (p, ids::mod (i, "curve"),  n + "Curve",  -1.0f, 1.0f, 0.0f);
        addChoice (p, ids::mod (i, "type"),   n + "Type", { "Unipolar", "Bipolar" }, 1);
    }

    // ---- FX rack (re-orderable, per-effect parameters) ----
    const auto fxn = fxNames();
    for (int i = 0; i < kNumFxSlots; ++i)
    {
        addBool  (p, ids::fx (i, "on"),    fxn[i] + " On", false);
        addFloat (p, ids::fx (i, "mix"),   fxn[i] + " Mix", 0.0f, 1.0f, fxDefaultMix (i));
        addInt   (p, ids::fx (i, "order"), fxn[i] + " Order", 0, kNumFxSlots - 1, i);

        const auto& specs = fxSpecs (i);
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            const auto& s = specs[(size_t) k];
            const juce::String id = ids::fxp (i, k), nm = fxn[i] + " " + s.name;
            if (s.kind == FxParamSpec::Float)       addFloat (p, id, nm, s.lo, s.hi, s.def, s.skew, 0.0f, s.unit);
            else if (s.kind == FxParamSpec::Choice) addChoice (p, id, nm, s.choices, (int) s.def);
            else                                    addBool (p, id, nm, s.def > 0.5f);
        }
    }

    // ---- Per-key microtuning (one octave, propagated to all octaves) ----
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < kNumMicroKeys; ++i)
        addFloat (p, ids::micro (i), juce::String ("Tune ") + noteNames[i], -100.0f, 100.0f, 0.0f, 0.0f, 1.0f, " ct");

    return { p.begin(), p.end() };
}

} // namespace nusynth
