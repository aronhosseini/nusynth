#pragma once
// Generators for the 59 extra noise types (see WavetableCatalog.h for the names; parameter indices 6..48 = index + 6).
// Names follow the Serum 2 noise menus ("Analog" and "S2 Noises"); the sounds are synthesised here (filtered / sampled / LFSR noise,
// crackle and ping generators, short transient bursts) and are NOT Serum's samples.
// extraNoiseTick() is called once per generator tick (= one sample at the nominal rate; the Noise Pitch knob re-clocks it). Time constants assume 48 kHz.
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace nusynth
{
namespace noisex
{
constexpr float kRate = 48000.0f, kTwoPi = 6.28318530718f;

struct Ping { float f = 0.0f, ph = 0.0f, env = 0.0f, dec = 0.99f; };

struct State
{
    uint32_t rng = 0x9E3779B9u, lfsr = 0x7FFFF8u, age = 0, ctr = 0;
    bool loop = true;
    float pk[7] {};          // pink filter
    float z[8] {};           // one-pole / smoothing states
    float held = 0.0f, gate = 1.0f, ph = 0.0f, env = 0.0f;
    float lf[3] {};          // slow modulators
    Ping ping[4];
    int nextPing = 0;
};

inline float rnd (State& s)                                  // 0..1
{
    s.rng ^= s.rng << 13; s.rng ^= s.rng >> 17; s.rng ^= s.rng << 5;
    return (float) (s.rng >> 8) / 16777216.0f;
}
inline float coef (float fc) { return 1.0f - std::exp (-kTwoPi * std::min (fc, 0.45f * kRate) / kRate); }
inline float pinkOf (State& s, float w)
{
    s.pk[0] = 0.99886f * s.pk[0] + w * 0.0555179f;  s.pk[1] = 0.99332f * s.pk[1] + w * 0.0750759f;
    s.pk[2] = 0.96900f * s.pk[2] + w * 0.1538520f;  s.pk[3] = 0.86650f * s.pk[3] + w * 0.3104856f;
    s.pk[4] = 0.55000f * s.pk[4] + w * 0.5329522f;  s.pk[5] = -0.7616f * s.pk[5] - w * 0.0168980f;
    const float p = s.pk[0] + s.pk[1] + s.pk[2] + s.pk[3] + s.pk[4] + s.pk[5] + s.pk[6] + w * 0.5362f;
    s.pk[6] = w * 0.115926f;
    return p * 0.11f;
}
inline float onePole (float& z, float x, float a) { z += a * (x - z); return z; }
inline float tSec (const State& s) { return (float) s.age / kRate; }
/** time inside the current cycle (looping) or -1 once a one-shot has finished */
inline float cycle (const State& s, float period)
{
    const float t = tSec (s);
    if (s.loop) return std::fmod (t, period);
    return t < period ? t : -1.0f;
}
inline void trigger (State& s, float f, float decaySec, float level = 1.0f)
{
    Ping& p = s.ping[s.nextPing]; s.nextPing = (s.nextPing + 1) & 3;
    p.f = f; p.ph = 0.0f; p.env = level; p.dec = std::exp (-1.0f / (decaySec * kRate));
}
inline float pings (State& s)
{
    float o = 0.0f;
    for (auto& p : s.ping)
    {
        if (p.env < 1.0e-4f) continue;
        p.ph += p.f / kRate; p.ph -= std::floor (p.ph);
        o += std::sin (kTwoPi * p.ph) * p.env;
        p.env *= p.dec;
    }
    return o;
}
inline float lfsrStep (State& s, int every)
{
    if (s.ctr++ % (uint32_t) every == 0)
    {
        const uint32_t bit = ((s.lfsr >> 22) ^ (s.lfsr >> 17)) & 1u;
        s.lfsr = ((s.lfsr << 1) | bit) & 0x7FFFFFu;
        s.held = (float) ((s.lfsr >> 15) & 0xFFu) / 128.0f - 1.0f;      // 8 bit output like the classic chip
    }
    return s.held;
}

/** idx 0..58 (0..16 Analog, 17..42 S2 Noises, 43..58 Organic), w = white noise -1..1 */
inline float tick (State& s, int idx, float w)
{
    ++s.age;
    float o = 0.0f;
    switch (idx)
    {
        // ---------------------------------------------------------------- "Analog"
        case 0:  { const float m = 0.8f + 0.2f * std::sin (kTwoPi * 0.3f * tSec (s)); o = m * (onePole (s.z[0], w, 0.45f) * 1.2f + 0.3f * pinkOf (s, w)); break; }          // AlphaNz
        case 1:  { if (s.ctr++ % 3 == 0) s.held = w; o = onePole (s.z[0], s.held, 0.55f); break; }                                                                         // ARP circuit
        case 2:  o = pinkOf (s, w) * 1.6f; break;                                                                                                                          // ARP pink
        case 3:  o = onePole (s.z[0], w, 0.8f); break;                                                                                                                     // ARP white
        case 4:  { o = (w - 0.55f * s.z[0]) * 0.8f; s.z[0] = w; break; }                                                                                                    // BrightWhite
        case 5:  { const float l = onePole (s.z[0], w, 0.5f); o = 0.6f * l + 0.5f * pinkOf (s, w); break; }                                                                  // J 60
        case 6:  { const float l = onePole (s.z[0], w, 0.6f); o = l - onePole (s.z[1], l, 0.004f); break; }                                                                  // J8
        case 7:  { const float t = tSec (s); const float mod = 0.78f + 0.14f * std::sin (kTwoPi * 0.6f * t) + 0.08f * std::sin (kTwoPi * 3.1f * t); o = mod * onePole (s.z[0], w, 0.55f); break; }   // J106 Chorus
        case 8:  { o = (w - onePole (s.z[0], w, 0.3f)) * 1.1f; break; }                                                                                                     // J106 HP
        case 9:  { const float t = tSec (s); const float mod = 0.78f + 0.14f * std::sin (kTwoPi * 0.6f * t) + 0.08f * std::sin (kTwoPi * 3.1f * t); o = mod * (w - onePole (s.z[0], w, 0.3f)) * 1.1f; break; }   // J106 HP_Cho
        case 10: o = onePole (s.z[0], w, 0.5f) * 1.1f; break;                                                                                                              // J106
        case 11: { if (s.ctr++ % 2 == 0) s.held = std::round (w * 32.0f) / 32.0f; o = s.held; break; }                                                                     // MicrKrg Noise
        case 12: o = onePole (s.z[0], w, 0.12f) * 2.0f + 0.3f * onePole (s.z[1], w, 0.03f) * 4.0f + 0.12f * w; break;                                                        // OrganNoise
        case 13: o = onePole (s.z[0], w, 0.38f) * 1.2f + 0.15f * pinkOf (s, w); break;                                                                                      // SH1 Noise
        case 14: o = onePole (s.z[0], lfsrStep (s, 40), 0.5f); break;                                                                                                      // SID drone
        case 15: o = lfsrStep (s, 6); break;                                                                                                                               // SID noise
        case 16: { s.ph += 3200.0f / kRate; s.ph -= std::floor (s.ph); o = 0.55f * lfsrStep (s, 3) * 0.6f + 0.35f * std::sin (kTwoPi * s.ph) + 0.15f * (w - onePole (s.z[0], w, 0.2f)); break; }   // SID whine

        // ---------------------------------------------------------------- "S2 Noises"
        case 17: { const float t = cycle (s, 0.5f); if (t < 0.0f) break; o = w * std::exp (-t / 0.004f) * 1.8f + 0.8f * std::sin (kTwoPi * 140.0f * t) * std::exp (-t / 0.03f); break; }   // 808 Transient
        case 18: { const float t = tSec (s); const float gust = 0.6f + 0.4f * std::sin (kTwoPi * 0.17f * t) * std::sin (kTwoPi * 0.05f * t + 1.0f);
                   const float low = onePole (s.z[0], w, 0.03f) * 3.0f; const float band = onePole (s.z[1], w, 0.30f) - onePole (s.z[2], w, 0.22f);
                   o = gust * low + 1.6f * band * (0.5f + 0.5f * gust); break; }                                                                                              // Arctic Visit
        case 19: { const float t = cycle (s, 0.4f); if (t < 0.0f || t > 0.06f) break; const float f = 1800.0f + 800.0f * (t / 0.025f); s.ph += f / kRate; s.ph -= std::floor (s.ph);
                   o = std::sin (kTwoPi * s.ph) * std::exp (-t / 0.012f) * (t < 0.001f ? t / 0.001f : 1.0f) + 0.05f * w; break; }                                           // Blip
        case 20: { const float t = tSec (s); const float burst = std::fmod (t * 6.0f, 1.0f) < 0.7f ? 1.0f : 0.15f; const float pulse = std::fmod (t * 90.0f, 1.0f) < 0.5f ? 1.0f : 0.25f;
                   o = (onePole (s.z[0], w, 0.55f) - onePole (s.z[1], w, 0.2f)) * 2.4f * burst * pulse; break; }                                                          // Cicadas
        case 21: { if (rnd (s) < 0.0009f) s.env = (rnd (s) * 2.0f - 1.0f) * (0.5f + 0.5f * rnd (s)) * 1.6f; o = s.env + 0.01f * w; s.env *= 0.55f; break; }                     // Crackle
        case 22: { const float t = cycle (s, 0.8f); if (t < 0.0f) break; o = std::sin (kTwoPi * 108.0f * t) * std::exp (-t / 0.03f) * 1.2f + onePole (s.z[0], w, 0.18f) * std::exp (-t / 0.012f); break; }   // Egtr Mute
        case 23: { const float t = cycle (s, 0.6f); if (t < 0.0f || t > 0.35f) break; const float k = t / 0.3f; const float fc = 3200.0f - 2400.0f * std::min (1.0f, k);
                   o = (onePole (s.z[0], w, coef (fc)) - onePole (s.z[1], w, coef (fc * 0.5f))) * 2.5f * std::exp (-t / 0.2f); break; }                                         // Egtr Neck Down
        case 24: { const float t = cycle (s, 0.6f); if (t < 0.0f || t > 0.35f) break; const float k = t / 0.3f; const float fc = 800.0f + 2400.0f * std::min (1.0f, k);
                   o = (onePole (s.z[0], w, coef (fc)) - onePole (s.z[1], w, coef (fc * 0.5f))) * 2.5f * std::exp (-t / 0.2f); break; }                                         // Egtr Neck Up
        case 25: { if (rnd (s) < 0.06f) s.env = (rnd (s) - 0.5f) * 1.4f; o = (w - onePole (s.z[0], w, 0.5f)) * 0.5f + s.env; s.env *= 0.4f; break; }                           // Fizz
        case 26: { const float t = cycle (s, 0.9f); if (t < 0.0f || t > 0.12f) break; const float e = std::sin (3.14159f * t / 0.06f) * (t < 0.06f ? 1.0f : 0.0f);
                   o = (onePole (s.z[0], w, coef (2600.0f)) - onePole (s.z[1], w, coef (1900.0f))) * 5.0f * e; break; }                                                       // FretNoise A
        case 27: { const float t = cycle (s, 0.9f); if (t < 0.0f || t > 0.08f) break; const float e = std::sin (3.14159f * t / 0.08f); const float fc = 1500.0f + 3000.0f * (t / 0.08f);
                   o = (onePole (s.z[0], w, coef (fc * 1.2f)) - onePole (s.z[1], w, coef (fc * 0.8f))) * 5.0f * e; break; }                                                   // FretNoise B
        case 28: { const float t = cycle (s, 0.9f); if (t < 0.0f || t > 0.2f) break; const float e = std::sin (3.14159f * t / 0.2f); const float fc = 1300.0f - 400.0f * (t / 0.2f);
                   o = (onePole (s.z[0], w, coef (fc * 1.25f)) - onePole (s.z[1], w, coef (fc * 0.8f))) * 5.5f * e; break; }                                                   // FretNoise C
        case 29: { const float p = pinkOf (s, w) * 1.6f; const float h1 = p - onePole (s.z[0], p, coef (100.0f)); o = h1 - onePole (s.z[1], h1, coef (100.0f)); break; }           // HP12 Pink Noise (Mono)
        case 30: { const float p = pinkOf (s, w) * 1.6f; const float h1 = p - onePole (s.z[0], p, coef (180.0f)); o = h1 - onePole (s.z[1], h1, coef (180.0f)); break; }           // HP12 Pink Noise (Stereo)
        case 31: { const float h1 = w - onePole (s.z[0], w, coef (150.0f)); o = h1 - onePole (s.z[1], h1, coef (150.0f)); break; }                                              // HP12 White Noise (Stereo)
        case 32: { if (rnd (s) < 0.0003f) trigger (s, 2800.0f + 3200.0f * rnd (s), 0.02f + 0.04f * rnd (s), 0.7f); o = pings (s) + 0.4f * (w - onePole (s.z[0], w, 0.4f)) * s.env; s.env *= 0.99f; break; }   // Keys and Ball
        case 33: { if (rnd (s) < 0.0007f) trigger (s, 700.0f + 900.0f * rnd (s), 0.015f + 0.02f * rnd (s), 0.7f); o = pings (s) + 0.03f * onePole (s.z[0], w, 0.1f); break; }       // Pebbles in Tube
        case 34: { if (s.ctr++ % (uint32_t) (1800 + (uint32_t) (rnd (s) * 6000.0f)) == 0) { s.gate = (float) (int) (rnd (s) * 4.0f); }
                   const int g = (int) s.gate; if (g == 0) o = 0.0f; else if (g == 1) { if ((s.age & 63u) == 0) s.held = w; o = s.held; } else if (g == 2) o = std::round (w * 6.0f) / 6.0f * 0.6f; else o = w - onePole (s.z[0], w, 0.3f);
                   break; }                                                                                                                                                  // Radio Glitch
        case 35: { if (rnd (s) < 0.0025f) trigger (s, 3000.0f + 5000.0f * rnd (s), 0.004f, 0.5f); o = 0.22f * (w - onePole (s.z[0], w, 0.45f)) + pings (s) * 0.7f; break; }       // Rain 100 High
        case 36: { const float t = std::fmod (tSec (s), 4.0f); const float k = t / 4.0f; const float fc = 200.0f + 2800.0f * k * k; const float g = 0.35f + 0.65f * k;
                   o = g * (onePole (s.z[0], w, coef (fc)) * 2.2f + 0.25f * (onePole (s.z[1], w, coef (fc * 2.2f)) - onePole (s.z[2], w, coef (fc * 1.2f)))); break; }          // Rising Wind
        case 37: { s.ph += 220.0f / kRate; s.ph -= std::floor (s.ph); o = onePole (s.z[0], 2.0f * s.ph - 1.0f, 0.7f); break; }                                                // Saw
        case 38: { if (rnd (s) < 0.0006f) trigger (s, 1800.0f + 3500.0f * rnd (s), 0.035f + 0.04f * rnd (s), 0.55f); o = pings (s) + 0.25f * onePole (s.z[0], w, 0.015f) * (0.5f + 0.5f * std::sin (kTwoPi * 7.0f * tSec (s))) * 3.0f; break; }   // Screws in Bowl
        case 39: { s.ph += 261.63f / kRate; s.ph -= std::floor (s.ph); o = std::sin (kTwoPi * s.ph); break; }                                                                  // Sine Wave C
        case 40: { if (rnd (s) < 0.00025f) s.env = (rnd (s) > 0.5f ? 1.0f : -1.0f) * (0.5f + rnd (s)); o = onePole (s.z[0], s.env, 0.08f) * 1.8f + onePole (s.z[1], w, 0.004f) * 5.0f * (0.7f + 0.3f * std::sin (kTwoPi * 0.55f * tSec (s)))
                   + 0.03f * (w - onePole (s.z[2], w, 0.2f)); s.env *= 0.9f; break; }                                                                                        // Stretched Vinyl
        case 41: { if (rnd (s) < 0.0004f) s.env = (rnd (s) > 0.5f ? 1.0f : -1.0f) * (0.6f + 0.8f * rnd (s)); else if (rnd (s) < 0.01f) s.env = (rnd (s) - 0.5f) * 0.5f;
                   o = s.env + 0.035f * (w - onePole (s.z[0], w, 0.3f)) + 0.1f * onePole (s.z[1], w, 0.005f) * 4.0f; s.env *= 0.5f; break; }                                    // Vinyl Crackle
        case 42: { const float t = cycle (s, 1.0f); if (t < 0.0f || t > 0.5f) break; const float e = (t < 0.02f ? t / 0.02f : 1.0f) * std::exp (-(t - 0.02f) / 0.09f);
                   o = (onePole (s.z[0], w, coef (3500.0f)) - onePole (s.z[1], w, coef (350.0f))) * 3.0f * e; break; }                                                         // Whiff
        // ---------------------------------------------------------------- "Organic"
        case 43: { s.ph += 50.0f / kRate; s.ph -= std::floor (s.ph); const float p = kTwoPi * s.ph;                                                                           // AC hum1 (50 Hz mains)
                   o = 0.5f * std::sin (p) + 0.26f * std::sin (2.0f * p) + 0.14f * std::sin (3.0f * p) + 0.07f * std::sin (5.0f * p) + 0.04f * std::sin (7.0f * p) + 0.5f * onePole (s.z[0], w, 0.01f); break; }
        case 44: { s.ph += 60.0f / kRate; s.ph -= std::floor (s.ph); const float p = kTwoPi * s.ph; const float wob = 0.85f + 0.15f * std::sin (kTwoPi * 0.4f * tSec (s));       // AC hum2 (60 Hz, buzzier, with fan noise)
                   o = wob * (0.38f * std::sin (p) + 0.3f * std::sin (3.0f * p) + 0.2f * std::sin (5.0f * p) + 0.12f * std::sin (9.0f * p)) + 0.35f * (onePole (s.z[0], w, 0.04f) - onePole (s.z[1], w, 0.006f)) * 3.0f; break; }
        case 45: { const float t = cycle (s, 1.2f); if (t < 0.0f || t > 0.7f) break; const float e = (t < 0.02f ? t / 0.02f : 1.0f) * std::exp (-t / 0.35f);                // Air Can 1 (spray burst)
                   o = (onePole (s.z[0], w, coef (7500.0f)) - onePole (s.z[1], w, coef (1800.0f))) * 1.9f * e; break; }
        case 46: { const float t = cycle (s, 1.6f); if (t < 0.0f || t > 1.0f) break; const float e = (t < 0.05f ? t / 0.05f : 1.0f) * std::exp (-t / 0.5f) * (0.75f + 0.25f * std::sin (kTwoPi * 28.0f * t));   // Air Can 2 (lower, sputtering)
                   o = (onePole (s.z[0], w, coef (4200.0f)) - onePole (s.z[1], w, coef (900.0f))) * 2.0f * e; break; }
        case 47: { const float t = cycle (s, 0.35f); if (t < 0.0f || t > 0.15f) break; const float e = std::sin (3.14159f * t / 0.15f);                                       // Air Can 3 (short puffs)
                   o = (onePole (s.z[0], w, coef (6500.0f)) - onePole (s.z[1], w, coef (1400.0f))) * 1.8f * e; break; }
        case 48: { if (s.ctr++ % (uint32_t) (900 + (uint32_t) (rnd (s) * 3000.0f)) == 0) s.gate = rnd (s) > 0.4f ? 1.0f : 0.0f;                                         // Air Can 4 (random sputter)
                   s.env += 0.003f * (s.gate - s.env); o = (onePole (s.z[0], w, coef (8000.0f)) - onePole (s.z[1], w, coef (2200.0f))) * 1.7f * s.env; break; }
        case 49: { const float t = cycle (s, 2.0f); if (t < 0.0f || t > 1.6f) break; const float e = (t < 0.03f ? t / 0.03f : 1.0f) * std::exp (-t / 0.9f);                  // Air Can 5 (long release, falling filter)
                   const float fc = 9000.0f * std::exp (-t * 1.2f) + 1500.0f; o = (onePole (s.z[0], w, coef (fc)) - onePole (s.z[1], w, coef (fc * 0.22f))) * 2.0f * e; break; }
        case 50: { const float t = cycle (s, 1.0f); if (t < 0.0f) break; const float e = (t < 0.002f ? t / 0.002f : 1.0f) * std::exp (-t / 0.32f);                         // CymLoop (cymbal wash)
                   static const float ratio[6] = { 1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f };
                   float m = 0.0f; for (int k = 0; k < 6; ++k) m += std::sin (kTwoPi * (3100.0f * ratio[k]) * t + (float) k) * (k & 1 ? 1.0f : -1.0f);
                   const float sh = w - onePole (s.z[0], w, coef (5200.0f)); o = (0.9f * sh + 0.07f * m * (m > 0.0f ? 1.0f : -1.0f) * 0.5f + 0.05f * m) * e * 1.4f; break; }
        case 51: { if (rnd (s) < 0.00008f) trigger (s, 5200.0f + 3800.0f * rnd (s), 0.12f + 0.2f * rnd (s), 0.35f);                                                       // CymMicBleed (faint cymbal in the mic)
                   const float sw = 0.5f + 0.5f * std::sin (kTwoPi * 0.23f * tSec (s)); o = 0.3f * (onePole (s.z[0], w, coef (9000.0f)) - onePole (s.z[1], w, coef (3500.0f))) * (0.4f + sw) * 2.5f + 0.5f * pings (s); break; }
        case 52: { const float t = cycle (s, 3.2f); if (t < 0.0f || t > 2.8f) break; const float k = t / 2.8f; const float e = std::pow (std::sin (3.14159f * std::min (1.0f, k)), 2.0f);   // H-Breath (inhale / exhale)
                   const float fc = 1100.0f + 1500.0f * (k < 0.5f ? k * 2.0f : 2.0f - 2.0f * k); o = (onePole (s.z[0], w, coef (fc * 1.6f)) - onePole (s.z[1], w, coef (fc * 0.45f))) * 2.2f * e; break; }
        case 53: { if (rnd (s) < 0.0018f) s.env = std::max (s.env, 0.35f + 0.65f * rnd (s)); if (rnd (s) < 0.012f) s.env = std::max (s.env, 0.1f * rnd (s));                   // Paper Bag (crinkle)
                   o = (onePole (s.z[0], w, coef (6500.0f)) - onePole (s.z[1], w, coef (1200.0f))) * 1.7f * s.env + 0.05f * onePole (s.z[2], w, 0.02f); s.env *= 0.9985f; break; }
        case 54: { const float t = cycle (s, 1.5f); if (t < 0.0f || t > 1.2f) break; const float e = std::pow (std::sin (3.14159f * t / 1.2f), 1.5f);                        // S-Sound ("ssss")
                   o = (w - onePole (s.z[0], w, coef (5500.0f))) * 0.9f * e + 0.4f * (onePole (s.z[1], w, coef (10500.0f)) - onePole (s.z[2], w, coef (7000.0f))) * 3.0f * e; break; }
        case 55: { const float t = cycle (s, 2.0f); if (t < 0.0f || t > 1.7f) break; const float e = std::pow (std::sin (3.14159f * t / 1.7f), 1.3f);                          // Shush 1 ("shhh")
                   o = (onePole (s.z[0], w, coef (4600.0f)) - onePole (s.z[1], w, coef (1500.0f))) * 2.2f * e; break; }
        case 56: { const float t = cycle (s, 2.4f); if (t < 0.0f || t > 2.0f) break; const float e = std::pow (std::sin (3.14159f * t / 2.0f), 1.2f) * (0.85f + 0.15f * std::sin (kTwoPi * 5.0f * t));   // Shush 2 (breathier, lower)
                   const float fc = 2800.0f - 900.0f * (t / 2.0f); o = (onePole (s.z[0], w, coef (fc)) - onePole (s.z[1], w, coef (fc * 0.3f))) * 2.4f * e; break; }
        case 57: { const float t = cycle (s, 0.8f); if (t < 0.0f || t > 0.3f) break; const float k = t / 0.3f; const float e = std::sin (3.14159f * k);                         // Squeaky
                   const float f = 1500.0f + 1700.0f * k + 90.0f * std::sin (kTwoPi * 38.0f * t); s.ph += f / kRate; s.ph -= std::floor (s.ph);
                   o = (0.6f * std::sin (kTwoPi * s.ph) + 0.25f * std::sin (kTwoPi * 2.0f * s.ph) + 0.08f * std::sin (kTwoPi * 3.0f * s.ph)) * e + 0.03f * w * e; break; }
        case 58: { static const float notes[8] = { 1568.0f, 1760.0f, 2093.0f, 2349.0f, 2637.0f, 3136.0f, 3520.0f, 4186.0f };                                                   // WindChimes
                   if (rnd (s) < 0.00012f) { const float f = notes[(int) (rnd (s) * 7.99f)]; const float d = 0.7f + 0.9f * rnd (s); trigger (s, f, d, 0.55f); trigger (s, f * 2.76f, d * 0.35f, 0.12f); }
                   o = pings (s) * 0.7f + 0.02f * onePole (s.z[0], w, 0.01f); break; }
        default: o = w; break;
    }
    if (! std::isfinite (o)) o = 0.0f;
    return std::max (-1.5f, std::min (1.5f, o));
}
}
}
