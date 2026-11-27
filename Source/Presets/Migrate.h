#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/ParameterIDs.h"

namespace nusynth
{
/** Brings the state of an older NuSynth version up to date (call before replaceState).
    - The old "Compressor (Analog)" (effect 10, ids fx10_*) is now the SINGLE mode of the Compressor (effect 8, fx8_*):
      it goes into the compressor slot if that one is free, otherwise into the second compressor.
    - Effects that are on are always shown in the rack (the "added" flag is new).
    - Missing oscillator / sub / noise on-switches are set to ON (see below). */
inline void migrateLegacyState (juce::ValueTree& st)
{
    auto node = [&st] (const juce::String& id) { return st.getChildWithProperty ("id", id); };
    auto get = [&node] (const juce::String& id, float def)
    {
        auto n = node (id);
        return n.isValid() ? (float) (double) n.getProperty ("value") : def;
    };
    auto setv = [&st, &node] (const juce::String& id, float v)
    {
        auto n = node (id);
        if (! n.isValid()) { n = juce::ValueTree ("PARAM"); n.setProperty ("id", id, nullptr); st.appendChild (n, nullptr); }
        n.setProperty ("value", v, nullptr);
    };

    // Older states / factory presets have no on-switch for the oscillators, sub and noise (those sources were always on). The start-up default is
    // now "only Osc A on", so a state that does not store the switch gets it set to ON: old presets keep sounding exactly as they did.
    for (const auto& id : { ids::osc (0, "on"), ids::osc (1, "on"), ids::osc (2, "on"), ids::subOn, ids::noiseOn })
        if (! node (id).isValid()) setv (id, 1.0f);

    const auto oldOn = ids::fx (9, "on");
    if (! node (oldOn).isValid()) return;                       // new-style state (or the analog compressor was never touched)

    if (get (oldOn, 0.0f) > 0.5f)
    {
        const bool ottOn = get (ids::fx (7, "on"), 0.0f) > 0.5f;
        const int target = ottOn ? 7 + kNumFxTypes : 7;
        for (int k = 0; k < 6; ++k) setv (ids::fxp (target, 7 + k), get (ids::fxp (9, k), 0.0f));
        setv (ids::fxp (target, 6), 0.0f);                                           // mode = SINGLE
        setv (ids::fx (target, "mix"), get (ids::fx (9, "mix"), 1.0f));
        setv (ids::fx (target, "order"), get (ids::fx (9, "order"), 9.0f));
        setv (ids::fx (target, "on"), 1.0f);
        setv (ids::fx (target, "added"), 1.0f);
    }
    st.removeChild (node (oldOn), nullptr);
}
}
