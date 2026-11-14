#pragma once
#include <cmath>
#include <algorithm>

namespace nusynth
{
/** Waveshapers shared by the Distortion FX and its graph. Types: 0 Soft Clip, 1 Hard Clip, 2 Tube, 3 Fold, 4 Bit Crush,
    5 Tape, 6 Diode, 7 Overdrive, 8 Fuzz, 9 Sine Shaper, 10 Chebyshev, 11 Downsample (stateful, handled by the caller). */
inline float distShape (int type, float v, float levels)
{
    const float clipped = std::max (-1.0f, std::min (1.0f, v));
    switch (type)
    {
        case 1:  return clipped;
        case 2:  return std::tanh (v + 0.2f) - std::tanh (0.2f);
        case 3:  { float m = (v + 1.0f) * 0.25f; m -= std::floor (m); return 1.0f - std::abs (m * 4.0f - 2.0f); }
        case 4:  return std::floor (clipped * levels + 0.5f) / levels;
        case 5:  return std::abs (v) < 1.0f ? 1.5f * v - 0.5f * v * v * v : (v > 0.0f ? 1.0f : -1.0f);
        case 6:  return v > 0.0f ? 1.0f - std::exp (-v) : -0.6f * (1.0f - std::exp (v));
        case 7:  return 0.63662f * std::atan (1.6f * v);
        case 8:  { const float a = std::tanh (4.0f * v + 0.15f); return std::max (-1.0f, std::min (1.0f, a + 0.25f * std::tanh (9.0f * v))) * 0.85f; }
        case 9:  return std::sin (std::max (-3.1416f, std::min (3.1416f, v)));
        case 10: { const float x = std::tanh (0.5f * v); return std::max (-1.0f, std::min (1.0f, x + 0.5f * (4.0f * x * x * x - 3.0f * x))); }
        default: return std::tanh (v);
    }
}
}
