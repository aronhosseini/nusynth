#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include "Parameters/ParameterIDs.h"
#include "Parameters/FxSpecs.h"
#include "Parameters/ParameterLayout.h"

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
    kNumVoiceDests = 45                            // 45.. = effect parameters (global modulation, see destTable)
};
inline constexpr int kMaxFxDests = 800;                 // effect destinations (all copies of all effects) + the extra voice destinations below
inline constexpr int kNumDests = (int) kNumVoiceDests + kMaxFxDests;
enum OscDest    { OD_Pitch, OD_Fine, OD_Position, OD_Spread, OD_Detune, OD_Warp, OD_Level, OD_Pan };
enum FilterDest { FD_Cutoff, FD_Res, FD_Drive, FD_Mix };

struct DestInfo { juce::String id, name; };

/** Voice destinations added after the effect destinations (so no older saved routing shifts): Osc A/B/C Coarse, Osc A/B/C Blend. */
inline constexpr int kNumExtraDests = 6;
inline int& extraDestBaseRef() { static int v = 0; return v; }

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

        // effect parameters (every continuous knob of every effect, plus each effect's mix)
        int n = (int) kNumVoiceDests;
        const int fxLimit = kNumDests - kNumExtraDests;
        const auto fxn = fxNames();
        auto addFx = [&] (int slot, int from, const juce::String& prefix)
        {
            const auto& specs = fxSpecs (slot);
            for (int k = from; k < (int) specs.size(); ++k)
                if (specs[(size_t) k].kind == FxParamSpec::Float && n < fxLimit)
                    a[(size_t) n++] = DestInfo { ids::fxp (slot, k), prefix + " " + specs[(size_t) k].name };
        };
        for (int i = 0; i < kNumLegacyFx; ++i)              // original destinations first: same order as before
        {
            if (i == 9)
            {   // the retired analog compressor: its 6 destinations + mix now point at the "Single" parameters of the Compressor (p7..p12)
                const auto& sp = fxSpecs (7);
                for (int k = 0; k < 6; ++k) a[(size_t) n++] = DestInfo { ids::fxp (7, 7 + k), "Compressor " + sp[(size_t) (7 + k)].name };
                a[(size_t) n++] = DestInfo { ids::fx (7, "mix"), "Compressor Mix (Single)" };
                continue;
            }
            const auto& specs = fxSpecs (i);
            for (int k = 0; k < fxLegacyParams (i); ++k)
                if (specs[(size_t) k].kind == FxParamSpec::Float && n < fxLimit)
                    a[(size_t) n++] = DestInfo { ids::fxp (i, k), fxn[i] + " " + specs[(size_t) k].name };
            if (fxHasMix (i) && n < fxLimit) a[(size_t) n++] = DestInfo { ids::fx (i, "mix"), fxn[i] + " Mix" };
        }
        auto addLater = [&] (int i)
        {
            if (i != 7) addFx (i, fxLegacyParams (i), fxn[i]);                          // (the Compressor's Single parameters are already listed above)
            if (i >= kNumLegacyFx && fxHasMix (i) && n < fxLimit) a[(size_t) n++] = DestInfo { ids::fx (i, "mix"), fxn[i] + " Mix" };
        };
        // then everything added later, in the order it was added (so older saved routings never shift):
        for (int i = 0; i < kNumFxTypes; ++i) if (i != 8 && i < 11) addLater (i);       // later reverb / delay extras, the Filter effect
        addLater (8);                                                                   // Limiter: Input, Ceiling, Soft Clip, Width
        for (int i = 11; i < kNumFxTypes; ++i) addLater (i);                            // Hyper / Dimension
        // the extra copies of every effect (second reverb, third distortion, ...)
        for (int c = 1; c < kFxCopies; ++c)
            for (int t : fxMenuTypes())
            {
                const int slot = t + kNumFxTypes * c;
                const juce::String prefix = fxn[t] + " #" + juce::String (c + 1);       // ("Filter #2 ..." must not look like the voice filter 2)
                addFx (slot, 0, prefix);
                if (fxHasMix (t) && n < fxLimit) a[(size_t) n++] = DestInfo { ids::fx (slot, "mix"), prefix + " Mix" };
            }
        // extra voice destinations (always at the very end of the used table)
        extraDestBaseRef() = n;
        for (int o = 0; o < 3; ++o) if (n < kNumDests) a[(size_t) n++] = DestInfo { ids::osc (o, "coarse"), juce::String (oname[o]) + "Coarse" };
        for (int o = 0; o < 3; ++o) if (n < kNumDests) a[(size_t) n++] = DestInfo { ids::osc (o, "blend"),  juce::String (oname[o]) + "Blend" };
        return a;
    }();
    return t;
}

/** Number of real destinations (the table is padded with empty entries). */
inline int usedDests()
{
    static const int n = [] { int c = 0; for (const auto& d : destTable()) if (d.id.isNotEmpty()) ++c; return c; }();
    return n;
}

inline int extraDestBase() { (void) destTable(); return extraDestBaseRef(); }
inline int destCoarse (int o) { return extraDestBase() + o; }
inline int destBlend (int o)  { return extraDestBase() + 3 + o; }
/** True for the destinations a voice modulates (oscillators, sub, noise, filters, LFO rates + the extras); the rest are effect parameters. */
inline bool isVoiceDest (int d) { const int b = extraDestBase(); return d < kNumVoiceDests || (d >= b && d < b + kNumExtraDests); }

inline int destForParam (const juce::String& paramId)
{
    const auto& t = destTable();
    for (int i = 0; i < kNumDests; ++i)
        if (t[(size_t) i].id.isNotEmpty() && t[(size_t) i].id == paramId) return i;
    return -1;
}

// Modulation source indices (value of the "Source" choice parameter)
enum ModSrc { MS_None = 0, MS_Lfo1, MS_Lfo2, MS_Lfo3, MS_Lfo4, MS_Lfo5, MS_Lfo6, MS_Env1, MS_Env2, MS_Env3, MS_Env4,
              MS_Velocity, MS_Key, MS_ModWheel, MS_Aftertouch, MS_Random,
              MS_Macro1, MS_Macro2, MS_Macro3, MS_Macro4, MS_Macro5, MS_Macro6, MS_Macro7, MS_Macro8, kNumModSources };
}
