#pragma once
#include <cmath>
#include "SharedData.h"

namespace nusynth
{
inline float lfoWaveCustom (const LfoCustom& c, float ph)
{
    const int n = c.n;
    if (n < 2) return 0.0f;
    if (ph <= c.x[0]) return c.y[0] * 2.0f - 1.0f;
    for (int i = 0; i < n - 1; ++i)
        if (ph <= c.x[i + 1])
        {
            const float span = c.x[i + 1] - c.x[i];
            const float t = span > 1.0e-6f ? (ph - c.x[i]) / span : 0.0f;
            return (c.y[i] + (c.y[i + 1] - c.y[i]) * t) * 2.0f - 1.0f;
        }
    return c.y[n - 1] * 2.0f - 1.0f;
}

/** Shared by the audio engine and the UI so the display always matches what you hear.
    shape: 0 Custom (free points), 1 Sine, 2 Triangle, 3 Saw, 4 Square, 5 S&H. Returns -1..1 */
inline float lfoWave (int shape, float ph, const LfoCustom& custom, float sampleHold)
{
    switch (shape)
    {
        case 1:  return std::sin (6.28318530718f * ph);
        case 2:  return 1.0f - 4.0f * std::abs (std::fmod (ph + 0.25f, 1.0f) - 0.5f);
        case 3:  return 2.0f * ph - 1.0f;
        case 4:  return ph < 0.5f ? 1.0f : -1.0f;
        case 5:  return sampleHold;
        default: return lfoWaveCustom (custom, ph);
    }
}
}
