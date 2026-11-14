#include "Wavetables.h"
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <algorithm>

namespace nusynth
{
namespace
{
constexpr int N = WavetableBank::kSize;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kPi = 3.14159265359f;
using cf = std::complex<float>;

std::array<float, (size_t) N> g_sin;

inline float sinIdx (int i) { return g_sin[(size_t) (i & (N - 1))]; }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float fractf (float x) { return x - std::floor (x); }
inline float sawW (float x) { return 2.0f * x - 1.0f; }
inline float triW (float x) { return 1.0f - 4.0f * std::abs (fractf (x + 0.25f) - 0.5f); }
inline float sqW (float x, float d) { return x < d ? 1.0f : -1.0f; }
inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float gauss (float x) { return std::exp (-x * x); }

inline float hashN (int n, int seed)
{
    uint32_t h = (uint32_t) n * 747796405u + (uint32_t) seed * 2891336453u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    return (float) (h & 0xFFFFu) / 32768.0f - 1.0f;
}

template <class F>
float additive (int n, int hmax, F&& amp)
{
    float s = 0.0f;
    for (int h = 1; h <= hmax; ++h)
    {
        const float a = amp (h);
        if (a != 0.0f) s += a * sinIdx (h * n);
    }
    return s;
}

template <class F>
float additiveScr (int n, int hmax, F&& amp, float scramble)
{
    float s = 0.0f;
    for (int h = 1; h <= hmax; ++h)
    {
        const float a = amp (h);
        if (a != 0.0f) s += a * sinIdx (h * n + (int) (scramble * (float) ((h * 7919) % N)));
    }
    return s;
}

// formants expressed as harmonic numbers (fundamental ~100 Hz): F1, F2, F3 for A E I O U
inline float vowelAmp (int vowel, float m, int h, float shiftMul = 1.0f, float bwMul = 1.0f)
{
    static const float f[5][3] = { { 8.0f, 11.5f, 28.0f }, { 4.0f, 16.0f, 27.0f }, { 2.7f, 23.0f, 29.0f }, { 4.5f, 8.0f, 26.0f }, { 3.2f, 7.0f, 25.0f } };
    const float shift = (0.8f + 0.4f * m) * shiftMul, hf = (float) h, bw = 1.3f * bwMul;
    const float a = 0.04f + gauss ((hf - f[vowel][0] * shift) / bw) + 0.7f * gauss ((hf - f[vowel][1] * shift) / (bw * 1.4f))
                  + 0.4f * gauss ((hf - f[vowel][2] * shift) / (bw * 1.8f));
    return a / std::pow (hf, 0.6f);
}

inline float vowelWave (int n, int vowel, float m, float shiftMul = 1.0f, float bwMul = 1.0f)
{
    static float amp[65]; static int cV = -1; static float cM = -1.0f, cS = -1.0f, cB = -1.0f;
    if (vowel != cV || m != cM || shiftMul != cS || bwMul != cB)
    {
        cV = vowel; cM = m; cS = shiftMul; cB = bwMul;
        for (int h = 1; h <= 64; ++h) amp[h] = vowelAmp (vowel, m, h, shiftMul, bwMul);
    }
    float s = 0.0f;
    for (int h = 1; h <= 64; ++h) s += amp[h] * sinIdx (h * n);
    return s;
}

float gen (int t, float m, int n);

float gen (int t, float m, int n)
{
    const float x = (float) n / (float) N;
    const float th = kTwoPi * x;
    const float s1 = std::sin (th);

    switch (t)
    {
        // ---------------------------------------------------------------- Classic analog
        case 0:  return lerpf (s1, sawW (x), m);
        case 1:  return lerpf (s1, sqW (x, 0.5f), m);
        case 2:  return lerpf (s1, triW (x), m);
        case 3:  return s1 + m * (0.5f * std::sin (2.0f * th) + 0.3f * std::sin (3.0f * th));
        case 4:  return sqW (x, 0.5f - 0.25f * m);
        case 5:  return sqW (x, 0.5f - 0.375f * m);
        case 6:  { float s = 0.0f; for (int k = -3; k <= 3; ++k) s += sawW (fractf (x + (float) k * m * 0.06f)); return s / 7.0f; }
        case 7:  return std::tanh ((1.0f + 4.0f * m) * sawW (x)) + 0.3f * m * std::sin (3.0f * th);
        case 8:  return 0.5f * sawW (x) + 0.5f * triW (fractf (2.0f * x + 0.25f * m));
        case 9:  return 0.6f * sawW (x) + 0.3f * m * sawW (fractf (2.0f * x)) + 0.2f * m * std::sin (5.0f * th);
        case 10: return sqW (x, 0.5f - 0.45f * m);
        case 11: return additive (n, 3 + (int) (20.0f * m), [] (int h) { return 1.0f / (float) h; });

        // ---------------------------------------------------------------- Digital / additive
        case 12: { static const int hs[9] = { 1, 2, 3, 5, 7, 11, 13, 17, 19 };
                   float s = 0.0f;
                   for (int i = 0; i < 9; ++i)
                       s += (1.0f / (0.6f + (float) hs[i] * 0.5f)) * (1.0f + ((i % 2) ? m : -0.5f * m)) * sinIdx (hs[i] * n);
                   return s; }
        case 13: { static const int hs[6] = { 1, 2, 3, 4, 6, 8 };
                   const float a[6] = { 1.0f, 0.8f, 0.2f + 0.6f * m, 0.5f, 0.3f * m, 0.4f };
                   float s = 0.0f; for (int i = 0; i < 6; ++i) s += a[i] * sinIdx (hs[i] * n);
                   return s; }
        case 14: return additive (n, 40, [m] (int h) { const float hf = (float) h;
                   return (0.15f + gauss ((hf - (4.0f + 6.0f * m)) / 1.7f) + gauss ((hf - (10.0f + 8.0f * m)) / 2.0f)
                           + 0.6f * gauss ((hf - (16.0f + 10.0f * m)) / 2.5f)) / hf; });
        case 15: return additive (n, 48, [m] (int h) { return gauss (((float) h - (2.0f + 40.0f * m)) / 4.0f); });
        case 16: return additive (n, 40, [m] (int h) { return ((h % 2) ? 1.0f : m) / (float) h; });
        case 17: return additive (n, 40, [m] (int h) { return ((h % 2 == 0 || h == 1) ? 1.0f : 0.3f * (1.0f - m)) / (float) h; });
        case 18: return additive (n, 48, [m] (int h) { return 0.5f * (1.0f + std::cos ((float) h * (0.3f + 2.0f * m))) / (float) h; });
        case 19: return additive (n, 48, [m] (int h) { return (0.5f + 0.5f * std::sin ((float) h * 1.7f + m * 6.0f)) / std::pow ((float) h, 1.2f); });
        case 20: return additive (n, 24 + (int) (24.0f * m), [] (int h) { return 1.0f / std::pow ((float) h, 0.7f); });
        case 21: return additive (n, 40, [m] (int h) { const float hf = (float) h;
                   return (0.2f + gauss ((hf - (4.0f + 2.0f * m)) / 1.5f) + 0.7f * gauss ((hf - (10.0f + 3.0f * m)) / 2.0f)
                           + 0.4f * gauss ((hf - 18.0f) / 3.0f)) / hf; });
        case 22: return additive (n, 40, [m] (int h) { const float hf = (float) h;
                   return (0.2f + gauss ((hf - (2.5f + 3.0f * m)) / 1.2f) + 0.7f * gauss ((hf - (12.0f - 3.0f * m)) / 2.0f)
                           + 0.4f * gauss ((hf - 17.0f) / 3.0f)) / hf; });
        case 23: return additive (n, 48, [m] (int h) { return ((h % 7 == 0) ? 0.2f * m : 1.0f) / std::pow ((float) h, 1.1f); });

        // ---------------------------------------------------------------- FM
        case 24: return std::sin (th + (1.0f + 5.0f * m) * std::sin (3.0f * th));
        case 25: return std::sin (th + 2.5f * m * std::sin (14.0f * th));
        case 26: return std::sin (th + 4.0f * m * s1);
        case 27: return std::sin (th + (3.0f + 4.0f * m) * std::sin (7.0f * th));
        case 28: return std::sin (th + (2.0f + 8.0f * m) * std::sin (5.0f * th));
        case 29: return std::sin (th + 10.0f * m * std::sin (2.0f * th));
        case 30: return std::sin (th + 6.0f * m * std::sin (2.0f * th));
        case 31: return std::sin (th + 6.0f * m * std::sin (3.0f * th));
        case 32: return std::sin (th + 3.0f * m * std::sin (th + 3.0f * m * s1));
        case 33: return std::sin (th + (1.0f + 3.0f * m) * std::sin (4.0f * th));
        case 34: return std::sin (th + 2.0f * m * std::sin (2.0f * th) + 2.0f * m * std::sin (3.0f * th));
        case 35: return std::sin (th + 8.0f * m * std::sin (11.0f * th));

        // ---------------------------------------------------------------- Harsh / spectral
        case 36: return additiveScr (n, 48, [m] (int h) { return h < 6 ? 0.0f : (0.5f + 0.5f * std::sin ((float) h * m * 3.0f)) / std::pow ((float) h, 0.4f); }, 1.0f);
        case 37: return std::sin (kPi * (1.0f + 6.0f * m) * sawW (x));
        case 38: { const float L = 2.0f + 30.0f * (1.0f - m); return std::floor (sawW (x) * L + 0.5f) / L; }
        case 39: { const float s = sawW (x); return std::copysign (std::pow (std::abs (s), 0.2f + 0.8f * (1.0f - m)), s); }
        case 40: return sqW (x, 0.5f) + 0.5f * m * hashN (n / 8, 40);
        case 41: return sawW (x) - sawW (fractf (x + 0.02f + 0.3f * m));
        case 42: return additiveScr (n, 40, [m] (int h) { return (0.3f + 0.7f * std::abs (std::sin ((float) h * 12.9898f)))
                                                          * (h <= 8 + (int) (30.0f * m) ? 1.0f : 0.0f) / std::sqrt ((float) h); }, 1.0f);
        case 43: { const int cell = 1 << (2 + (int) (5.0f * (1.0f - m)));
                   const int cells = N / cell; const int i0 = n / cell; const float fr = (float) (n % cell) / (float) cell;
                   const float sm = fr * fr * (3.0f - 2.0f * fr);
                   return lerpf (hashN (i0 % cells, 43), hashN ((i0 + 1) % cells, 43), sm); }
        case 44: { static const int ps[8] = { 7, 11, 13, 17, 19, 23, 29, 31 };
                   float s = 0.0f; for (int i = 0; i < 8; ++i) s += sinIdx (ps[i] * n) / (1.0f + (float) ps[i] * 0.1f);
                   return s * (0.5f + 0.5f * m) + 0.3f * s1; }
        case 45: return clampf ((1.0f + 9.0f * m) * s1, -1.0f, 1.0f);

        // ---------------------------------------------------------------- Morphs
        case 46: return lerpf (s1, sawW (x), m);
        case 47: return lerpf (sawW (x), sqW (x, 0.5f), m);
        case 48: return sqW (x, 0.5f - 0.4f * m);
        case 49: return lerpf (triW (x), sawW (x), m);
        case 50: return lerpf (gen (13, 0.6f, n), gen (12, 0.6f, n), m);
        case 51: return lerpf (gen (21, 0.5f, n), gen (22, 0.5f, n), m);
        case 52: return lerpf (gen (0, 1.0f, n), gen (20, 0.7f, n), m);
        case 53: return lerpf (gen (18, 0.5f, n), gen (44, 1.0f, n), m);
        case 54: return lerpf (gen (8, m, n), gen (19, m, n), m);
        case 56: return vowelWave (n, 0, m);
        case 57: return vowelWave (n, 1, m);
        case 58: return vowelWave (n, 2, m);
        case 59: return vowelWave (n, 3, m);
        case 60: return vowelWave (n, 4, m);
        case 61: { const float v = m * 4.0f; const int i0 = std::min (3, (int) v); return lerpf (vowelWave (n, i0, 0.5f), vowelWave (n, i0 + 1, 0.5f), v - (float) i0); }
        case 62: return vowelWave (n, 0, 0.5f + 0.5f * m, 1.3f, 0.7f);
        case 63: return additiveScr (n, 64, [m] (int h) { return vowelAmp (3, 0.3f + 0.4f * m, h, 1.0f, 1.0f) * (1.0f + 0.5f * m * (float) (h % 3 == 0)); }, 0.35f * m);
        case 64: return 0.6f * vowelWave (n, 3, m) + 0.4f * vowelWave (n, 4, 1.0f - m);
        default: return s1 + 0.5f * std::sin (2.0f * th + 1.0f + m * 3.0f) + 0.25f * std::sin (3.0f * th + 2.0f - m * 2.0f)
                        + 0.15f * std::sin (5.0f * th + m * 5.0f);
    }
}

struct Fft
{
    int n;
    std::vector<cf> tw;
    std::vector<int> rev;

    explicit Fft (int n_) : n (n_), tw ((size_t) (n_ / 2)), rev ((size_t) n_)
    {
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int k = 0; k < n / 2; ++k) tw[(size_t) k] = std::polar (1.0f, -kTwoPi * (float) k / (float) n);
        rev[0] = 0;
        for (int i = 1; i < n; ++i) rev[(size_t) i] = (rev[(size_t) (i >> 1)] >> 1) | ((i & 1) << (bits - 1));
    }

    void run (std::vector<cf>& a, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (a[(size_t) i], a[(size_t) rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    cf w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const cf u = a[(size_t) (i + j)], v = a[(size_t) (i + j + half)] * w;
                    a[(size_t) (i + j)] = u + v;
                    a[(size_t) (i + j + half)] = u - v;
                }
        }
    }
};
}

const WavetableBank& WavetableBank::get()
{
    static WavetableBank instance;
    return instance;
}

WavetableBank::WavetableBank()
{
    for (int i = 0; i < N; ++i) g_sin[(size_t) i] = std::sin (kTwoPi * (float) i / (float) N);

    data.assign ((size_t) kProcTables * kLevels * kFrames * (size_t) (kSize + 1), 0.0f);

    Fft fft (N);
    std::vector<cf> buf ((size_t) N), spec ((size_t) (N / 2 + 1));
    std::vector<float> td ((size_t) N);

    for (int t = 0; t < kProcTables; ++t)
        for (int f = 0; f < kFrames; ++f)
        {
            const float m = (float) f / (float) (kFrames - 1);
            double mean = 0.0;
            for (int n = 0; n < N; ++n) { td[(size_t) n] = gen (t, m, n); mean += td[(size_t) n]; }
            mean /= N;

            double sq = 0.0; float peak = 1.0e-6f;
            for (auto& v : td) { v -= (float) mean; sq += (double) v * v; peak = std::max (peak, std::abs (v)); }
            const float rms = (float) std::sqrt (sq / N);
            float scale = rms > 1.0e-6f ? 0.35f / rms : 1.0f;
            if (peak * scale > 1.2f) scale = 1.2f / peak;

            for (int n = 0; n < N; ++n) buf[(size_t) n] = cf (td[(size_t) n] * scale, 0.0f);
            fft.run (buf, false);
            for (int k = 0; k <= N / 2; ++k) spec[(size_t) k] = buf[(size_t) k];

            for (int L = 0; L < kLevels; ++L)
            {
                const int maxH = (L == 0) ? (N / 2 - 1) : ((N / 2) >> L);
                std::fill (buf.begin(), buf.end(), cf (0.0f, 0.0f));
                for (int k = 1; k <= maxH; ++k)
                {
                    float g = 1.0f;
                    const float edge = 0.85f * (float) maxH;
                    if ((float) k > edge) g = 0.5f * (1.0f + std::cos (kPi * ((float) k - edge) / ((float) maxH - edge + 1.0f)));
                    const cf v = spec[(size_t) k] * g;
                    buf[(size_t) k] = v;
                    buf[(size_t) (N - k)] = std::conj (v);
                }
                fft.run (buf, true);
                float* dst = const_cast<float*> (frame (t, L, f));
                for (int n = 0; n < N; ++n) dst[n] = buf[(size_t) n].real() / (float) N;
                dst[N] = dst[0];
            }
        }

    for (int u = 0; u < kUserSlots; ++u)
    {
        userActive[(size_t) u].store (0);
        std::vector<float> pts ((size_t) kUserPoints);
        for (int i = 0; i < kUserPoints; ++i)
        {
            const float ph = (float) i / (float) kUserPoints;
            pts[(size_t) i] = 0.8f * std::sin (kTwoPi * ph) + 0.25f * std::sin (kTwoPi * (float) (u + 2) * ph + (float) u);
        }
        userSrc[(size_t) u] = { pts };
        buildUser (u, userSrc[(size_t) u]);
    }
}

void WavetableBank::buildUser (int slot, const std::vector<std::vector<float>>& framesIn)
{
    std::vector<std::vector<float>> src = framesIn;
    if (src.empty()) src.push_back (std::vector<float> (8, 0.0f));
    for (auto& f : src) if (f.size() < 8) f.assign (8, 0.0f);

    static Fft fft (N);
    std::vector<cf> buf ((size_t) N), spec ((size_t) (N / 2 + 1));
    std::vector<float> td ((size_t) N);

    auto sample = [] (const std::vector<float>& f, int n)
    {
        const int P = (int) f.size();
        const float pos = (float) n * (float) P / (float) N;
        const int i0 = (int) pos; const float fr = pos - (float) i0;
        return lerpf (f[(size_t) (i0 % P)], f[(size_t) ((i0 + 1) % P)], fr);
    };

    const int target = (userActive[(size_t) slot].load() + 1) % 3;
    auto& dstVec = userData[(size_t) slot][(size_t) target];
    dstVec.assign ((size_t) kLevels * (size_t) kFrames * (size_t) (kSize + 1), 0.0f);

    for (int fr = 0; fr < kFrames; ++fr)
    {
        const float sp = src.size() > 1 ? (float) fr * (float) (src.size() - 1) / (float) (kFrames - 1) : 0.0f;
        const int s0 = std::min ((int) sp, (int) src.size() - 1), s1 = std::min (s0 + 1, (int) src.size() - 1);
        const float sw = sp - (float) s0;

        double mean = 0.0;
        for (int n = 0; n < N; ++n)
        {
            td[(size_t) n] = lerpf (sample (src[(size_t) s0], n), sample (src[(size_t) s1], n), sw);
            mean += td[(size_t) n];
        }
        mean /= N;
        double sq = 0.0; float peak = 1.0e-6f;
        for (auto& v : td) { v -= (float) mean; sq += (double) v * v; peak = std::max (peak, std::abs (v)); }
        const float rms = (float) std::sqrt (sq / N);
        float scale = rms > 1.0e-6f ? 0.35f / rms : 1.0f;
        if (peak * scale > 1.2f) scale = 1.2f / peak;

        for (int n = 0; n < N; ++n) buf[(size_t) n] = cf (td[(size_t) n] * scale, 0.0f);
        fft.run (buf, false);
        for (int k = 0; k <= N / 2; ++k) spec[(size_t) k] = buf[(size_t) k];

        for (int L = 0; L < kLevels; ++L)
        {
            const int maxH = (L == 0) ? (N / 2 - 1) : ((N / 2) >> L);
            std::fill (buf.begin(), buf.end(), cf (0.0f, 0.0f));
            for (int k = 1; k <= maxH; ++k)
            {
                float g = 1.0f;
                const float edge = 0.85f * (float) maxH;
                if ((float) k > edge) g = 0.5f * (1.0f + std::cos (kPi * ((float) k - edge) / ((float) maxH - edge + 1.0f)));
                const cf v = spec[(size_t) k] * g;
                buf[(size_t) k] = v;
                buf[(size_t) (N - k)] = std::conj (v);
            }
            fft.run (buf, true);
            float* dst = dstVec.data() + ((size_t) L * kFrames + (size_t) fr) * (size_t) (kSize + 1);
            for (int n = 0; n < N; ++n) dst[n] = buf[(size_t) n].real() / (float) N;
            dst[N] = dst[0];
        }
    }
    userActive[(size_t) slot].store (target, std::memory_order_release);
}

void WavetableBank::setUserFrames (int slot, const std::vector<std::vector<float>>& frames)
{
    if (slot < 0 || slot >= kUserSlots) return;
    auto& inst = const_cast<WavetableBank&> (get());
    std::lock_guard<std::mutex> lock (inst.userMutex);
    inst.userSrc[(size_t) slot] = frames;
    if (inst.userSrc[(size_t) slot].size() > (size_t) kFrames) inst.userSrc[(size_t) slot].resize ((size_t) kFrames);
    inst.buildUser (slot, inst.userSrc[(size_t) slot]);
}

std::vector<std::vector<float>> WavetableBank::getUserFrames (int slot)
{
    if (slot < 0 || slot >= kUserSlots) return {};
    auto& inst = const_cast<WavetableBank&> (get());
    std::lock_guard<std::mutex> lock (inst.userMutex);
    return inst.userSrc[(size_t) slot];
}
}
