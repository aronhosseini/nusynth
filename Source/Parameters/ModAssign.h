#pragma once
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterIDs.h"
#include "DSP/ModDestinations.h"

namespace nusynth
{
inline void setParamReal (juce::AudioProcessorValueTreeState& a, const juce::String& id, float real)
{
    if (auto* p = a.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (real));
        p->endChangeGesture();
    }
}

inline float getParamReal (juce::AudioProcessorValueTreeState& a, const juce::String& id)
{
    auto* v = a.getRawParameterValue (id);
    return v != nullptr ? v->load() : 0.0f;
}

/** Drag a modulation source (1..11) onto a destination (index into destTable()). */
inline bool assignModulation (juce::AudioProcessorValueTreeState& a, int src, int dst)
{
    int freeSlot = -1;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int s = (int) getParamReal (a, ids::mod (i, "src"));
        const int d = (int) getParamReal (a, ids::mod (i, "dst"));
        if (s == src && d == dst + 1)
        {
            if (std::abs (getParamReal (a, ids::mod (i, "amount"))) < 0.01f) setParamReal (a, ids::mod (i, "amount"), 0.5f);
            return true;
        }
        if (freeSlot < 0 && s == 0) freeSlot = i;
    }
    if (freeSlot < 0) return false;

    setParamReal (a, ids::mod (freeSlot, "src"), (float) src);
    setParamReal (a, ids::mod (freeSlot, "dst"), (float) (dst + 1));
    setParamReal (a, ids::mod (freeSlot, "amount"), 0.5f);
    setParamReal (a, ids::mod (freeSlot, "curve"), 0.0f);
    const bool bipolarNative = (src >= MS_Lfo1 && src <= MS_Lfo6);
    setParamReal (a, ids::mod (freeSlot, "type"), bipolarNative ? 1.0f : 0.0f);
    return true;
}

inline void clearModSlot (juce::AudioProcessorValueTreeState& a, int slot)
{
    setParamReal (a, ids::mod (slot, "src"), 0.0f);
    setParamReal (a, ids::mod (slot, "dst"), 0.0f);
    setParamReal (a, ids::mod (slot, "amount"), 0.0f);
}
}
