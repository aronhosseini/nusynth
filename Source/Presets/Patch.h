#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <initializer_list>
#include <utility>
#include <vector>
#include "Parameters/ParameterIDs.h"
#include "Parameters/FxSpecs.h"

namespace nusynth
{
/** A sound described as a list of parameter assignments (later entries override earlier ones).
    Used by the factory presets AND the offline "AI" sound designer. */
class Patch
{
public:
    void n (const juce::String& id, float v) { nums.push_back ({ id, v }); }
    void c (const juce::String& id, const juce::String& choice) { choices.push_back ({ id, choice }); }

    float get (const juce::String& id, float fallback) const
    {
        for (auto it = nums.rbegin(); it != nums.rend(); ++it) if (it->first == id) return it->second;
        return fallback;
    }

    // ---------------------------------------------------------------- oscillators
    void osc (int o, const char* wavetable, float level, int semis = 0, float fine = 0.0f, int unison = 1,
              float detune = 0.25f, float spread = 0.5f, float position = 0.7f, float pan = 0.0f)
    {
        c (ids::osc (o, "wavetable"), wavetable);
        n (ids::osc (o, "level"), level);       n (ids::osc (o, "pitch"), (float) semis);   n (ids::osc (o, "fine"), fine);
        n (ids::osc (o, "unison"), (float) unison); n (ids::osc (o, "detune"), detune);    n (ids::osc (o, "spread"), spread);
        n (ids::osc (o, "position"), position); n (ids::osc (o, "pan"), pan);
    }
    void warp (int o, const char* mode, float amount) { c (ids::osc (o, "warpMode"), mode); n (ids::osc (o, "warp"), amount); }
    void oscOff (int o) { n (ids::osc (o, "level"), 0.0f); }

    void sub (const char* wave, float level, int octave = -1, float pan = 0.0f)
    { c (ids::subWave, wave); n (ids::subLevel, level); n (ids::subOctave, (float) octave); n (ids::subPan, pan); }

    void noise (const char* type, float level, float cutoff = 8000.0f, bool lp = true, bool hp = false, int pitch = 0, float pan = 0.0f)
    {
        c (ids::noiseType, type); n (ids::noiseLevel, level); n (ids::noiseCutoff, cutoff);
        n (ids::noiseLP, lp ? 1.0f : 0.0f); n (ids::noiseHP, hp ? 1.0f : 0.0f); n (ids::noisePitch, (float) pitch); n (ids::noisePan, pan);
    }

    // ---------------------------------------------------------------- filters / routing
    void filter (int f, const char* type, float cutoff, float res, float drive = 0.0f, float mix = 1.0f, bool on = true)
    {
        c (ids::filter (f, "type"), type); n (ids::filter (f, "cutoff"), cutoff); n (ids::filter (f, "res"), res);
        n (ids::filter (f, "drive"), drive); n (ids::filter (f, "mix"), mix); n (ids::filter (f, "on"), on ? 1.0f : 0.0f);
    }
    /** source: oscA, oscB, oscC, sub, noise ; routing: "Bypass", "Filter 1", "Filter 2", "Parallel (1+2)", "Series (1 > 2)" */
    void route (const char* source, const char* routing) { c (ids::route (source), routing); }
    void routeAll (const char* routing) { for (auto* s : { "oscA", "oscB", "oscC", "sub", "noise" }) route (s, routing); }

    // ---------------------------------------------------------------- modulators
    void env (int e, float a, float d, float s, float r)
    { n (ids::env (e, "attack"), a); n (ids::env (e, "decay"), d); n (ids::env (e, "sustain"), s); n (ids::env (e, "release"), r); }
    void envCurve (int e, float ac, float dc, float rc) { n (ids::env (e, "aCurve"), ac); n (ids::env (e, "dCurve"), dc); n (ids::env (e, "rCurve"), rc); }

    void lfo (int l, const char* shape, float rate, float depth = 1.0f, bool sync = false, const char* div = "1/4", float smooth = 0.0f)
    {
        c (ids::lfo (l, "shape"), shape); n (ids::lfo (l, "rate"), rate); n (ids::lfo (l, "depth"), depth);
        n (ids::lfo (l, "sync"), sync ? 1.0f : 0.0f); c (ids::lfo (l, "div"), div); n (ids::lfo (l, "smooth"), smooth);
    }

    /** source names: "LFO 1".."LFO 3", "Env 1".."Env 3", "Velocity", "Key", "Mod Wheel", "Aftertouch", "Random".
        destination names: as listed in the MATRIX tab (e.g. "Filter 1 Cutoff", "Osc A Position"). */
    void mod (const char* source, const char* dest, float amount, bool bipolar, float curve = 0.0f)
    {
        if (slot >= kNumModSlots) return;
        c (ids::mod (slot, "src"), source); c (ids::mod (slot, "dst"), dest);
        n (ids::mod (slot, "amount"), amount); n (ids::mod (slot, "curve"), curve); n (ids::mod (slot, "type"), bipolar ? 1.0f : 0.0f);
        ++slot;
    }
    int freeSlots() const { return kNumModSlots - slot; }
    int slotsUsed() const { return slot; }

    // ---------------------------------------------------------------- effects
    static int fxParamIndex (int fxIdx, const char* name)
    {
        const auto& sp = fxSpecs (fxIdx);
        for (int k = 0; k < (int) sp.size(); ++k) if (sp[(size_t) k].name == name) return k;
        return -1;
    }
    void fx (int idx, float mix, std::initializer_list<std::pair<const char*, float>> params = {})
    {
        n (ids::fx (idx, "on"), 1.0f); n (ids::fx (idx, "mix"), mix);
        for (const auto& pr : params)
        {
            const int k = fxParamIndex (idx, pr.first);
            if (k >= 0) n (ids::fxp (idx, k), pr.second);
        }
    }
    void fxChoice (int idx, const char* param, const char* choice)
    {
        const int k = fxParamIndex (idx, param);
        if (k >= 0) c (ids::fxp (idx, k), choice);
    }
    /** dist -> eq -> ott -> phaser -> flanger -> chorus -> delay -> reverb -> limiter */
    void standardChain()
    {
        static const int order[9] = { 7, 6, 5, 4, 3, 0, 1, 2, 8 };   // fx index -> position
        for (int i = 0; i < 9; ++i) n (ids::fx (i, "order"), (float) order[i]);
    }

    // ---------------------------------------------------------------- arp / play
    void arp (bool on, const char* pattern, const char* rate, int octaves, float gate, float swing)
    {
        n (ids::arpOn, on ? 1.0f : 0.0f); c (ids::arpPattern, pattern); c (ids::arpRate, rate);
        n (ids::arpOctaves, (float) octaves); n (ids::arpGate, gate); n (ids::arpSwing, swing);
    }
    void play (const char* mode, float glideMs = 0.0f, int voices = 16)
    { c (ids::playMode, mode); n (ids::glideTime, glideMs); n (ids::maxVoices, (float) voices); }

    // ---------------------------------------------------------------- export
    juce::var toVar() const
    {
        auto* params = new juce::DynamicObject();
        for (const auto& e : nums)    params->setProperty (juce::Identifier (e.first), e.second);
        for (const auto& e : choices) params->setProperty (juce::Identifier (e.first), e.second);
        auto* root = new juce::DynamicObject();
        root->setProperty ("params", juce::var (params));
        return juce::var (root);
    }

private:
    std::vector<std::pair<juce::String, float>> nums;
    std::vector<std::pair<juce::String, juce::String>> choices;
    int slot = 0;
};

/** Resets the synth to defaults (keeps master volume, master tune and microtuning), then applies the payload. Message thread only. */
inline void applyPatchVar (juce::AudioProcessorValueTreeState& apvts, const juce::var& payload)
{
    auto* root = payload.getDynamicObject();
    if (root == nullptr) return;
    auto* params = root->getProperty ("params").getDynamicObject();
    if (params == nullptr) return;

    for (int l = 0; l < kNumLfos; ++l) apvts.state.removeProperty ("lfoShape" + juce::String (l + 1), nullptr);

    for (auto* ap : apvts.processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (ap))
        {
            const auto& pid = withId->paramID;
            if (pid == ids::masterGainDb || pid == ids::masterTune || pid.startsWith ("micro_")) continue;
            ap->setValueNotifyingHost (ap->getDefaultValue());
        }

    for (const auto& nv : params->getProperties())
    {
        auto* param = apvts.getParameter (nv.name.toString());
        if (param == nullptr) continue;

        float real;
        if (nv.value.isString())
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
            if (choice == nullptr) continue;
            const int idx = choice->choices.indexOf (nv.value.toString());
            if (idx < 0) continue;
            real = (float) idx;
        }
        else real = (float) nv.value;

        param->setValueNotifyingHost (param->convertTo0to1 (real));
    }
}
}
