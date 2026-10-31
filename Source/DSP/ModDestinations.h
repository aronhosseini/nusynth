#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include "Parameters/ParameterIDs.h"

namespace nusynth
{
/** Every parameter that can receive modulation (drag a LFO/ENV onto its knob, or use the MATRIX tab). */
enum Dest : int
{
    D_OscA = 0, D_OscB = 8, D_OscC = 16,           // 8 entries each: see OscDest
    D_SubLevel = 24, D_SubPan = 25,
    D_NoiseLevel = 26, D_NoiseCutoff = 27, D_NoisePitch = 28, D_NoiseFine = 29, D_NoisePan = 30,
    D_F1 = 31, D_F2 = 35,                          // 4 entries each: see FilterDest
    D_Lfo1Rate = 39,                               // 6 entries
    kNumDests = 45
};
enum OscDest    { OD_Pitch, OD_Fine, OD_Position, OD_Spread, OD_Detune, OD_Warp, OD_Level, OD_Pan };
enum FilterDest { FD_Cutoff, FD_Res, FD_Drive, FD_Mix };

struct DestInfo { juce::String id, name; };

inline const std::array<DestInfo, kNumDests>& destTable()
{
    static const std::array<DestInfo, kNumDests> t = []
    {
        std::array<DestInfo, kNumDests> a;
        const char* on[]  = { "Pitch", "Fine", "Position", "Spread", "Detune", "Warp", "Level", "Pan" };
        const char* oid[] = { "pitch", "fine", "position", "spread", "detune", "warp", "level", "pan" };
        const char* oname[] = { "Osc A ", "Osc B ", "Osc C " };
        for (int o = 0; o < 3; ++o)
            for (int k = 0; k < 8; ++k)
                a[(size_t) (o * 8 + k)] = DestInfo { ids::osc (o, oid[k]), juce::String (oname[o]) + on[k] };
        a[D_SubLevel]    = DestInfo { ids::subLevel, "Sub Level" };
        a[D_SubPan]      = DestInfo { ids::subPan, "Sub Pan" };
        a[D_NoiseLevel]  = DestInfo { ids::noiseLevel, "Noise Level" };
        a[D_NoiseCutoff] = DestInfo { ids::noiseCutoff, "Noise Filter" };
        a[D_NoisePitch]  = DestInfo { ids::noisePitch, "Noise Pitch" };
        a[D_NoiseFine]   = DestInfo { ids::noiseFine, "Noise Fine" };
        a[D_NoisePan]    = DestInfo { ids::noisePan, "Noise Pan" };
        const char* fn[]  = { "Cutoff", "Resonance", "Drive", "Mix" };
        const char* fid[] = { "cutoff", "res", "drive", "mix" };
        for (int f = 0; f < 2; ++f)
            for (int k = 0; k < 4; ++k)
                a[(size_t) (D_F1 + f * 4 + k)] = DestInfo { ids::filter (f, fid[k]), "Filter " + juce::String (f + 1) + " " + fn[k] };
        for (int l = 0; l < 6; ++l)
            a[(size_t) (D_Lfo1Rate + l)] = DestInfo { ids::lfo (l, "rate"), "LFO " + juce::String (l + 1) + " Rate" };
        return a;
    }();
    return t;
}

inline int destForParam (const juce::String& paramId)
{
    const auto& t = destTable();
    for (int i = 0; i < kNumDests; ++i)
        if (t[(size_t) i].id == paramId) return i;
    return -1;
}

// Modulation source indices (value of the "Source" choice parameter)
enum ModSrc { MS_None = 0, MS_Lfo1, MS_Lfo2, MS_Lfo3, MS_Lfo4, MS_Lfo5, MS_Lfo6, MS_Env1, MS_Env2, MS_Env3, MS_Env4,
              MS_Velocity, MS_Key, MS_ModWheel, MS_Aftertouch, MS_Random,
              MS_Macro1, MS_Macro2, MS_Macro3, MS_Macro4, MS_Macro5, MS_Macro6, MS_Macro7, MS_Macro8, kNumModSources };
}
