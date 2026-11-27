#pragma once
#include <cmath>

namespace nusynth
{
/** Balanced (point-symmetric) envelope segment shape.

    A quadratic Bezier from (0,0) to (1,1) whose control point sits on the anti-diagonal, so the bend is shared
    EQUALLY by both end points (the old  t^e  curve only bent near one end).
      c = 0   straight line
      c > 0   slow start / fast end  (bows below the line)
      c < 0   fast start / slow end  (bows above the line)
    envCurve() returns the progress 0..1 for a given time fraction t 0..1.  It is used by the audio engine AND by the
    envelope display, so what you see is exactly what you hear. */
inline float envCurve (float t, float c)
{
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    const float d = 0.46f * c;
    if (std::abs (d) < 1.0e-4f) return t;
    const float cx = 0.5f + d, cy = 0.5f - d;
    const float u = t / (cx + std::sqrt (cx * cx - 2.0f * d * t));       // solves x(u) = t
    return u * u + 2.0f * u * (1.0f - u) * cy;
}

/** The point exactly in the middle of the curve (equal distance from both end points) - where the drag handle sits. */
inline void envCurveCentre (float c, float& x, float& y)
{
    c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    const float d = 0.46f * c;
    x = 0.5f + 0.5f * d;
    y = 0.5f - 0.5f * d;
}

/** Inverse: curve amount from a handle position (nx = time fraction, ny = progress), both 0..1. */
inline float envCurveFromHandle (float nx, float ny)
{
    const float c = (nx - ny) / 0.46f;
    return c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
}
}
