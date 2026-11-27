#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace nusynth
{
/** Oscillator warp modes. The first 16 keep the indices of earlier versions (saved presets stay valid); new modes are appended. */
enum WarpMode : int
{
    WM_Off = 0, WM_Sync, WM_Remap, WM_BendP, WM_BendM, WM_BendPM, WM_PWM, WM_AsymP, WM_AsymM, WM_AsymPM,
    WM_Quant, WM_Mirror, WM_Fold, WM_FM, WM_AM, WM_RM,
    // ---- added later
    WM_Flip, WM_Remap2, WM_Remap3, WM_Remap4, WM_OddEven,
    WM_FiltLP, WM_FiltHP,
    WM_Tube, WM_SoftClip, WM_HardClip, WM_Diode1, WM_Diode2, WM_SineFold, WM_ZeroSq, WM_DistAsym, WM_Rectify, WM_SineShaper, WM_Stomp, WM_TapeSat, WM_SoftSat,
    WM_PD,
    kNumWarpModes
};

/** Warp sources for FM / PD / AM / RM (index of the "Warp Source" choice parameter). */
inline constexpr int kNumWarpSources = 7;
inline juce::StringArray warpSourceNames() { return { "Osc A", "Osc B", "Osc C", "Sub", "Noise", "Filter 1", "Filter 2" }; }

inline bool warpUsesSource (int m) noexcept { return m == WM_FM || m == WM_AM || m == WM_RM || m == WM_PD; }

/** Names of the choice parameter (the first 16 must never change: SoundDesigner / presets refer to them). */
inline juce::StringArray warpModeNameList()
{
    return { "Off", "Sync", "Remap", "Bend +", "Bend -", "Bend +/-", "PWM", "Asym +", "Asym -", "Asym +/-", "Quantize", "Mirror", "Wavefold", "FM", "AM", "RM",
             "Flip", "Remap 2", "Remap 3", "Remap 4", "Odd/Even", "Filter LP", "Filter HP",
             "Tube", "Soft Clip", "Hard Clip", "Diode 1", "Diode 2", "Sine Fold", "Zero-Square", "Dist Asym", "Rectify", "Sine Shaper", "Stomp Box", "Tape Sat.", "Soft Sat.",
             "PD" };
}

/** One entry of the warp menu (grouped like Serum 2: Alt Warp / Filter / Distortion submenus, FM / PD / AM / RM with a source submenu). */
struct WarpMenuItem { const char* group; const char* label; int mode; };

inline const std::vector<WarpMenuItem>& warpMenuItems()
{
    static const std::vector<WarpMenuItem> items = {
        { "Alt Warp", "Bend +", WM_BendP }, { "Alt Warp", "Bend -", WM_BendM }, { "Alt Warp", "Bend +/-", WM_BendPM }, { "Alt Warp", "PWM", WM_PWM },
        { "Alt Warp", "Asym +", WM_AsymP }, { "Alt Warp", "Asym -", WM_AsymM }, { "Alt Warp", "Asym +/-", WM_AsymPM },
        { "Alt Warp", "Flip", WM_Flip }, { "Alt Warp", "Mirror", WM_Mirror },
        { "Alt Warp", "Remap 1", WM_Remap }, { "Alt Warp", "Remap 2", WM_Remap2 }, { "Alt Warp", "Remap 3", WM_Remap3 }, { "Alt Warp", "Remap 4", WM_Remap4 },
        { "Alt Warp", "Quantize", WM_Quant }, { "Alt Warp", "Odd/Even", WM_OddEven },
        { "Filter", "LPF", WM_FiltLP }, { "Filter", "HPF", WM_FiltHP },
        { "Distortion", "Tube", WM_Tube }, { "Distortion", "Soft Clip", WM_SoftClip }, { "Distortion", "Hard Clip", WM_HardClip },
        { "Distortion", "Diode 1", WM_Diode1 }, { "Distortion", "Diode 2", WM_Diode2 }, { "Distortion", "Linear Fold", WM_Fold }, { "Distortion", "Sine Fold", WM_SineFold },
        { "Distortion", "Zero-Square", WM_ZeroSq }, { "Distortion", "Asym", WM_DistAsym }, { "Distortion", "Rectify", WM_Rectify }, { "Distortion", "Sine Shaper", WM_SineShaper },
        { "Distortion", "Stomp Box", WM_Stomp }, { "Distortion", "Tape Sat.", WM_TapeSat }, { "Distortion", "Soft Sat.", WM_SoftSat },
    };
    return items;
}

/** Short text for the warp button: "Off", "Sync", "Alt Warp: Bend +", "FM: Osc B" ... */
inline juce::String warpDisplayText (int mode, int src)
{
    if (mode == WM_Off) return "Off";
    if (mode == WM_Sync) return "Sync";
    if (mode == WM_FM || mode == WM_AM || mode == WM_RM || mode == WM_PD)
    {
        const char* n = mode == WM_FM ? "FM" : (mode == WM_AM ? "AM" : (mode == WM_RM ? "RM" : "PD"));
        const auto sn = warpSourceNames();
        return juce::String (n) + ": " + sn[juce::jlimit (0, sn.size() - 1, src)];
    }
    for (const auto& it : warpMenuItems())
        if (it.mode == mode) return juce::String (it.group) + ": " + it.label;
    return "Off";
}
inline float warpTriFold (float x) { float m = (x + 1.0f) * 0.25f; m -= std::floor (m); return 1.0f - std::abs (m * 4.0f - 2.0f); }

/** Value-domain part of a warp (the part applied to the sample, after the phase has been warped). w = warp amount 0..1, x = oscillator phase,
    ms = modulation source sample (AM / RM). Shared by the engine and the oscillator display. */
inline float warpValue (int mode, float val, float w, float x, float ms)
{
    // dry / wet crossfade so that a warp amount of 0 is always transparent
    const float amt = std::min (1.0f, w * 3.0f);
    auto mixd = [val, amt] (float y) { return val + (y - val) * amt; };
    const float g1 = 1.0f + w * 8.0f;
    switch (mode)
    {
        case WM_AsymP:  return val < 0.0f ? val * (1.0f - 0.9f * w) : val;
        case WM_AsymM:  return val > 0.0f ? val * (1.0f - 0.9f * w) : val;
        case WM_AsymPM: return std::max (-1.2f, std::min (1.2f, val + 0.5f * w * val * std::abs (val)));
        case WM_Quant:  { const float L = (2.0f + (1.0f - w) * 62.0f) * 0.5f; return std::floor (val * L + 0.5f) / L; }
        case WM_Fold:   return warpTriFold (val * (1.0f + w * 5.0f));
        case WM_AM:     return val * ((1.0f - w) + w * (0.5f + 0.5f * ms));
        case WM_RM:     return val * (1.0f - w) + w * val * ms;
        case WM_Flip:   return x >= 0.5f ? val * (1.0f - 2.0f * w) : val;
        case WM_Tube:   { const float b = 0.2f * w; return mixd (std::tanh (val * g1 + b) - std::tanh (b)); }
        case WM_SoftClip: return mixd (std::tanh (val * g1) / std::tanh (g1));
        case WM_HardClip: return mixd (std::max (-1.0f, std::min (1.0f, val * g1)));
        case WM_Diode1: { const float xx = val * g1; return mixd (xx >= 0.0f ? 1.0f - std::exp (-xx) : -0.5f * (1.0f - std::exp (xx))); }
        case WM_Diode2: { const float xx = val * g1; return mixd (xx >= 0.0f ? std::tanh (xx) : std::max (xx * 0.35f, -0.45f)); }
        case WM_SineFold: return mixd (std::sin (val * (1.0f + w * 4.0f) * 3.14159265f));
        case WM_ZeroSq: return std::copysign (std::pow (std::abs (val) + 1.0e-9f, 1.0f - 0.92f * w), val);
        case WM_DistAsym: { const float xx = val * g1; return mixd (xx >= 0.0f ? std::tanh (xx) : 1.4f * std::tanh (xx * 0.4f)); }
        case WM_Rectify: return mixd (std::abs (val) * 2.0f - 0.64f);
        case WM_SineShaper: return (val + w * 0.6f * std::sin (val * 6.28318531f * (1.0f + w))) / (1.0f + 0.6f * w);
        case WM_Stomp:  { const float xx = val * (1.0f + w * 18.0f); return mixd (0.9f * std::tanh (xx) + 0.1f * std::tanh (xx * 3.0f)); }
        case WM_TapeSat: { const float xx = val * (1.0f + w * 3.0f); return mixd (std::tanh (xx + 0.15f * xx * std::abs (xx))); }
        case WM_SoftSat: { const float xx = std::max (-1.0f, std::min (1.0f, val * (1.0f + w * 3.0f))); return mixd (1.5f * (xx - xx * xx * xx / 3.0f)); }
        default: return val;
    }
}

}
