#pragma once
// Procedural generators for the 144 extra "Serum-style" wavetables (see WavetableCatalog.h for the names).
// Every table is described by a family + 7 numbers; the 8 morph frames (WT Pos) sweep those numbers.
// These are original approximations that follow each name's character (PWM sweeps, resonant bass, FM, spectral monsters, vowels ...),
// they are NOT copies of Serum's wavetable data.
#include <cmath>
#include <complex>
#include <cstdint>
#include <algorithm>

namespace nusynth
{
namespace extras
{
enum Family { F_SAWLP, F_SHAPES, F_PULSE, F_RESO, F_COMB, F_SERIES, F_FORMANT, F_RAND, F_METAL, F_FM, F_DIST, F_SYNC };

struct Spec { Family fam; float p[7]; };

#define X(F, a, b, c, d, e, f, g) { F, { a, b, c, d, e, f, g } }
// SAWLP : cut0, cutDelta, order, exponent, odd0, triangleSigns, oddDelta
// SHAPES: start, delta  (0 sine, 1 triangle, 2 saw, 3 square)
// PULSE : duty0, dutyDelta, cut0, cutDelta, extraRolloff, sawBlend
// RESO  : fc0, fcDelta, Q, width, exponent, cut
// COMB  : phase0, phaseDelta, exponent, cut, thirdLayer      (saw + shifted saw = pulse / reese-like phase combing)
// SERIES: step (0 = primes), -, exponent, leak0, leakDelta, cut
// FORMANT: vowel0, vowel1, shift0, bandwidth, tilt, shiftDelta
// RAND  : seed, hmax, exponent, sparsity, phaseRandom
// METAL : ratio0, ratioDelta, width, exponent, hmax, floor
// FM    : index0, indexDelta, ratio, ratio2, index2
// DIST  : base(0 sine 1 saw 2 tri 3 square), drive0, driveDelta, shape(0 tanh 1 fold 2 bits 3 clip 4 asym), bits, offset
// SYNC  : ratio0, ratioDelta, base
inline const Spec kSpecs[144] = {
    // ---------------- Analog (42)
    X (F_SAWLP, 30, 60, 2, 1.0f, 0, 0, 0),            // 4088
    X (F_RESO, 3, 11, 6, 1.5f, 1.0f, 60, 0),          // Acid
    X (F_DIST, 0, 1.0f, 2.0f, 0, 0, 0, 0),            // Analog_BD_Sin
    X (F_RESO, 2, 4, 1.5f, 3.0f, 1.0f, 40, 0),            // Basic Mg
    X (F_SAWLP, 20, 20, 1.5f, 1.0f, 0, 0, 0.6f),         // Basic Mini
    X (F_SHAPES, 0, 3, 0, 0, 0, 0, 0),                // Basic Shapes
    X (F_SHAPES, 0, 1.2f, 0, 0, 0, 0, 0),             // Basic_Cjw
    X (F_SHAPES, 0.5f, 2.0f, 0, 0, 0, 0, 0),          // Basic_McB
    X (F_SHAPES, 2, 1, 0, 0, 0, 0, 0),                // Basic_Mdc
    X (F_SHAPES, 0.2f, 1.5f, 0, 0, 0, 0, 0),          // Basic_Wrd
    X (F_RESO, 2, 16, 8, 1.2f, 1.0f, 70, 0),          // BS2 - Acid
    X (F_DIST, 1, 2.0f, 6.0f, 1, 0, 0, 0),            // BS2 - Filthy
    X (F_SAWLP, 4, 10, 2.5f, 1.0f, 0, 0, 0),          // BS2 - Subby Saw
    X (F_PULSE, 0.5f, 0, 40, 360, 0, 0, 0),            // BSOD_Square
    X (F_SHAPES, 1, 1, 0, 0, 0, 0, 0),                // DS Saw and Tri
    X (F_PULSE, 0.5f, -0.35f, 40, 60, 0, 0.6f, 0),       // Jno
    X (F_DIST, 3, 1.0f, 0, 2, 5, 0, 0.4f),               // MATRIXY C64
    X (F_SAWLP, 6, 200, 1, 1.1f, 0, 0, 0),        // MB Saw
    X (F_METAL, 2.7f, 0.5f, 0.25f, 1.5f, 40, 0.1f, 0),// MB Triangle (Metal)
    X (F_COMB, 0.01f, 0.05f, 1.3f, 12, 0, 0, 0),      // Mellow But Instable
    X (F_SAWLP, 8, 24, 2, 1.0f, 0.3f, 0, 0),             // MiniBass
    X (F_SAWLP, 60, 200, 1, 1.0f, 0.5f, 0, -0.5f),            // MsAw
    X (F_PULSE, 0.5f, -0.4f, 600, 0, 0, 0, 0),        // MsPw
    X (F_PULSE, 0.12f, 0.38f, 500, 0, 0, 0, 0),       // PWM C64
    X (F_PULSE, 0.5f, -0.3f, 60, 140, 0, 0, 0),       // PWM DS
    X (F_PULSE, 0.3f, -0.25f, 600, 0, 0.3f, 0, 0),    // PWM Genji
    X (F_PULSE, 0.05f, 0.45f, 600, 0, 0.6f, 0.3f, 0), // PWM Inception
    X (F_PULSE, 0.5f, -0.25f, 25, 0, 0, 0, 0),        // PWM Juno
    X (F_PULSE, 0.5f, -0.4f, 12, 28, 0, 0, 0),        // PWM LPF
    X (F_PULSE, 0.35f, -0.2f, 80, 0, 0.2f, 0, 0),     // PWM Maths 2
    X (F_PULSE, 0.5f, -0.48f, 300, 0, 0, 0, 0),       // PWM Maths
    X (F_PULSE, 0.25f, 0.2f, 30, 0, 0, 0.5f, 0),      // PWM MedicineHat
    X (F_PULSE, 0.5f, -0.2f, 200, 0, 0.5f, 0, 0),     // PWM MG
    X (F_PULSE, 0.5f, -0.35f, 20, 0, 0, 0, 0),        // PWM Mini
    X (F_COMB, 0.45f, -0.37f, 1.0f, 18, 0, 0, 0),     // PWM Reso
    X (F_PULSE, 0.5f, -0.1f, 15, 0, 0, 0, 0),         // PWM Restricted
    X (F_PULSE, 0.45f, -0.4f, 100, 0, 0.8f, 0, 0),    // PWM Trench
    X (F_PULSE, 0.3f, -0.2f, 40, 0, 0.3f, 0.2f, 0),         // PWM_Mdc
    X (F_SAWLP, 14, -6, 3, 1.0f, 0, 0, 0),             // SawRounded
    X (F_SAWLP, 10, 20, 2, 1.0f, 0, 0, 1),            // SawRoundedToSquare
    X (F_SHAPES, 3, -1, 0, 0, 0, 0, 0),               // SQR_SAW_Wrd
    X (F_RESO, 1, 19, 5, 1.0f, 1.0f, 60, 0),          // Sweep20
    // ---------------- Digital (56)
    X (F_RAND, 11, 40, 0.8f, 0.3f, 0.2f, 0, 0),       // Anti-Stasis
    X (F_SERIES, 3, 0, 1.0f, 0.2f, 0.6f, 0, 0),       // bipole_harmonic
    X (F_RESO, 3, 2, 12, 0.8f, 2.0f, 0, 0),           // BottleBlow
    X (F_DIST, 1, 1.5f, 2.5f, 2, 3, 0, 0),            // CrushWub
    X (F_RAND, 3, 24, 1.4f, 0.2f, 0.1f, 0, 0),        // Debussy
    X (F_DIST, 1, 1.0f, 2.0f, 3, 0, 0.1f, 0),         // DirtySaw
    X (F_DIST, 3, 1.0f, 3.0f, 2, 3, 0, 1.6f),            // Dist 8bit Fwap
    X (F_DIST, 0, 2.0f, 8.0f, 1, 0, 0, 0),            // Dist Bass Dropper
    X (F_DIST, 2, 2.0f, 4.0f, 0, 0, 0.3f, 0),         // Dist C2
    X (F_DIST, 1, 3.0f, 9.0f, 1, 0, 0, 0),            // Dist d00t
    X (F_DIST, 3, 1.5f, 4.0f, 4, 0, 0, 2.4f),            // Dist Fwapper SQ
    X (F_DIST, 0, 3.0f, 3.0f, 3, 0, 0.2f, 0),         // Dist Sub Dskperc
    X (F_DIST, 1, 1.0f, 4.0f, 4, 0, 0, 0),            // Dist WaTech
    X (F_SAWLP, 5, 3, 4, 1.5f, 0, 0, 0),              // Dull_toy
    X (F_FORMANT, 0, 3, 1.0f, 1.2f, 0.6f, 0, 0),      // Ethos
    X (F_COMB, 0.02f, 0.28f, 1.0f, 40, 0, 0, 0),      // Evol Longreece
    X (F_RESO, 2, 38, 4, 3.0f, 1.0f, 0, 0),           // Evol Sweep
    X (F_RAND, 21, 32, 1.0f, 0.2f, 0.5f, 0, 0),       // Evolution
    X (F_SERIES, 2, 0, 1.0f, 0, 0.25f, 0, 0),             // FFT_Add_2nds
    X (F_SERIES, 3, 0, 1.0f, 0, 0.25f, 0, 0),             // FFT_Add_3rds
    X (F_SERIES, 4, 0, 1.0f, 0, 0.25f, 0, 0),             // FFT_Add_4ths
    X (F_RAND, 7, 60, 0.6f, 0.6f, 1.0f, 0, 0),        // FFT_Animal 1
    X (F_RESO, 20, 30, 10, 2.0f, 1.2f, 90, 0),        // FFT_SQUEAL
    X (F_FM, 1, 3, 2, 3, 1.5f, 0, 0),                 // FMFM
    X (F_FM, 2, 6, 3, 5, 2.0f, 0, 0),                 // FM_Freak
    X (F_FM, 0, 6, 7, 1, 3.0f, 0, 0),                 // FM_Splat
    X (F_DIST, 1, 1.0f, 1.0f, 2, 6, 0, 0),            // Gritty
    X (F_RESO, 4, 3, 5, 2.5f, 1.3f, 0, 0),            // GS-xello
    X (F_SERIES, 2, 0, 1.0f, 0, 1.0f, 0, 0),          // Harmonic Morph
    X (F_SERIES, 2, 0, 1.2f, 0.6f, 0.4f, 0, 0),       // Harmonic Subtle
    X (F_SAWLP, 10, 60, 2, 0.9f, 0, 0, 0),             // HarmonicSeries
    X (F_FORMANT, 3, 4, 0.8f, 1.5f, 0.8f, 0, 0),      // Hoo
    X (F_FM, 3, -2, 4, 1, 0.5f, 0, 0),                // Hypa
    X (F_DIST, 0, 1.5f, 4.5f, 1, 0, 0, 0),            // ICanHasKick
    X (F_DIST, 1, 2.0f, 6.0f, 0, 0, 0.15f, 0),        // Kream
    X (F_DIST, 3, 1.0f, 0, 2, 4, 0, 0.9f),               // Mario
    X (F_RAND, 33, 28, 0.9f, 0.4f, 0.8f, 0, 0),       // Misfits
    X (F_DIST, 2, 1.0f, 0, 2, 6, 0, 0.5f),               // PerfectSQSteps
    X (F_SERIES, 0, 0, 1.0f, 0, 0.3f, 0, 0),             // Prime
    X (F_PULSE, 0.5f, -0.4f, 30, 370, 0, 0.2f, 0),    // PWM Electro
    X (F_PULSE, 0.3f, 0.4f, 50, 0, 0.4f, -0.4f, 0),   // PWM Weird
    X (F_SAWLP, 80, 100, 1, 0.7f, 0, 0, 0),           // Razor
    X (F_COMB, 0.01f, 0.11f, 1.0f, 50, 0, 0, 0),      // Reese
    X (F_FM, 1, 4, 5, 1, 0, 0, 0),                    // Ring
    X (F_RESO, 12, 16, 8, 2.0f, 0.8f, 70, 0),         // Scream
    X (F_SYNC, 1.5f, 3.5f, 1, 0, 0, 0, 0),            // Shrats
    X (F_DIST, 1, 4.0f, 6.0f, 3, 0, 0.3f, 0),         // Sludgecrank
    X (F_SYNC, 1, 1.0f, 3, 0, 0, 0, 0),               // SSQ
    X (F_SAWLP, 2, 3, 4, 1.0f, 0, 0, 0),              // SubBass_1
    X (F_FORMANT, 0, 1, 1.2f, 0.8f, 0.5f, 0, 0),      // Tigra
    X (F_PULSE, 0.5f, -0.2f, 25, 0, 0, 0.5f, 0),      // VintageSynth
    X (F_FORMANT, 4, 0, 1.0f, 1.1f, 0.7f, 0, 0),      // Voxu
    X (F_SYNC, 1, 7, 0, 0, 0, 0, 0),                  // Waveterminal
    X (F_FM, 0.5f, 4.5f, 1, 2, 2.0f, 0, 0),           // Weow
    X (F_RAND, 55, 20, 1.2f, 0.7f, 1.0f, 0, 0),       // Wraith
    X (F_SYNC, 2, 1, 2, 0, 0, 0, 0),                  // Yoy64
    // ---------------- Spectral (35)
    X (F_RAND, 101, 80, 0.7f, 0.5f, 1.0f, 0, 0),      // AlienSpectral
    X (F_METAL, 2.76f, 0.34f, 0.15f, 1.0f, 60, 0.05f, 0), // Bowed Metal
    X (F_RAND, 103, 30, 1.2f, 0.3f, 0.6f, 0, 0),      // Creeper
    X (F_COMB, 0.08f, 0.12f, 1.1f, 28, 0.4f, 0, 0),   // Electric Guitar
    X (F_RAND, 105, 70, 0.5f, 0.6f, 1.0f, 0, 0),      // Gremlin
    X (F_FORMANT, 0, 2, 1.5f, 0.6f, 0.4f, 0, 0),      // Grimace
    X (F_METAL, 1.59f, 0.31f, 0.2f, 1.2f, 50, 0.08f, 0), // Light Metal 1
    X (F_RAND, 111, 90, 0.45f, 0.00f, 1.0f, 0, 0),    // Monster 1
    X (F_RAND, 112, 90, 0.52f, 0.05f, 1.0f, 0, 0),    // Monster 2
    X (F_RAND, 113, 90, 0.59f, 0.10f, 1.0f, 0, 0),    // Monster 3
    X (F_RAND, 114, 90, 0.66f, 0.15f, 1.0f, 0, 0),    // Monster 4
    X (F_RAND, 115, 90, 0.73f, 0.20f, 1.0f, 0, 0),    // Monster 5
    X (F_RAND, 116, 90, 0.80f, 0.25f, 1.0f, 0, 0),    // Monster 6
    X (F_RAND, 117, 90, 0.87f, 0.30f, 1.0f, 0, 0),    // Monster 7
    X (F_RAND, 118, 90, 0.94f, 0.35f, 1.0f, 0, 0),    // Monster 8
    X (F_RAND, 119, 90, 1.01f, 0.40f, 1.0f, 0, 0),    // Monster 9
    X (F_SAWLP, 6, 54, 2, 0.6f, 1, 0, 0),            // Oddpass
    X (F_COMB, 0.15f, 0.75f, 1.0f, 80, 0, 0, 0),      // Phase Werb
    X (F_COMB, 0.020f, 0.050f, 0.9f, 40, 0.3f, 0, 0), // Reesey Mess 1
    X (F_COMB, 0.030f, 0.090f, 1.0f, 50, 0.5f, 0, 0), // Reesey Mess 2
    X (F_COMB, 0.040f, 0.130f, 1.1f, 60, 0.7f, 0, 0), // Reesey Mess 3
    X (F_COMB, 0.050f, 0.170f, 1.2f, 70, 0.9f, 0, 0), // Reesey Mess 4
    X (F_COMB, 0.060f, 0.210f, 1.3f, 80, 1.1f, 0, 0), // Reesey Mess 5
    X (F_RESO, 6, 4, 3, 4.0f, 1.4f, 0, 0),            // Scout
    X (F_COMB, 0.25f, 0.25f, 1.3f, 30, 0, 0, 0),      // Solid Phase 1
    X (F_COMB, 0.40f, -0.30f, 1.0f, 50, 0, 0, 0),     // Solid Phase 2
    X (F_RESO, 2, 22, 10, 1.2f, 1.0f, 70, 0),         // Squelchy 1
    X (F_RESO, 3, 27, 12, 1.0f, 1.0f, 70, 0),         // Squelchy 2
    X (F_RESO, 1, 17, 9, 1.6f, 1.0f, 70, 0),          // Squelchy 3
    X (F_FM, 0.5f, 5.5f, 2, 1, 1.0f, 0, 0),           // Squelchy FM 1
    X (F_FM, 1, 7, 3, 2, 0.5f, 0, 0),                 // Squelchy FM 2
    X (F_COMB, 0.02f, 0.48f, 1.0f, 60, 0, 0, 0),             // Thin Thick
    X (F_RAND, 131, 24, 1.0f, 0.8f, 0.3f, 0, 0),      // Trilobyte 1
    X (F_RAND, 132, 32, 0.9f, 0.7f, 0.5f, 0, 0),      // Trilobyte 2
    X (F_RAND, 133, 40, 0.8f, 0.6f, 0.7f, 0, 0),      // Trilobyte 3
    // ---------------- Vowel (11)
    X (F_FORMANT, 0, 4, 1.0f, 1.0f, 0.6f, 0, 0),      // Allophones
    X (F_FORMANT, 3, 0, 0.9f, 1.6f, 0.8f, 0, 0),      // DudaChoir
    X (F_FORMANT, 1, 0, 1.1f, 1.0f, 0.6f, 0, 0),      // E_Yeah
    X (F_FORMANT, 4, 3, 0.6f, 1.4f, 0.5f, 0, 0),      // Fred_Likes_Bass
    X (F_FORMANT, 2, 1, 1.0f, 0.7f, 0.4f, 0, 0),      // ML Addifunk
    X (F_FORMANT, 0, 3, 1.3f, 1.3f, 0.7f, 0, 0),      // Native Curse
    X (F_FORMANT, 4, 0, 1.0f, 1.2f, 0.6f, 0, 0),      // OOH_YAH_OO
    X (F_FORMANT, 1, 2, 1.2f, 0.8f, 0.3f, 0, 0),      // Retro-Speaking
    X (F_FORMANT, 2, 4, 1.4f, 0.9f, 0.5f, 0, 0),      // SAM_SERUM
    X (F_FORMANT, 3, 1, 1.5f, 0.7f, 0.5f, 0, 0),      // SAM_Speaks
    X (F_FORMANT, 3, 0, 0.6f, 1.2f, 0.6f, 0.8f, 0),   // WahWah
};
#undef X

namespace detail
{
constexpr float kPi = 3.14159265359f;
inline float lp (float h, float cut, float order)
{
    if (cut <= 0.0f) return 1.0f;
    return 1.0f / std::sqrt (1.0f + std::pow (h / cut, 2.0f * order));
}
inline float hashU (int n, int seed)           // 0..1
{
    uint32_t h = (uint32_t) n * 747796405u + (uint32_t) seed * 2891336453u + 12345u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    return (float) (h & 0xFFFFu) / 65535.0f;
}
inline bool isPrime (int n) { if (n < 2) return false; for (int i = 2; i * i <= n; ++i) if (n % i == 0) return false; return true; }
inline float baseWave (int kind, float x)
{
    x -= std::floor (x);
    switch (kind)
    {
        case 0:  return std::sin (2.0f * kPi * x);
        case 1:  return 2.0f * x - 1.0f;
        case 2:  return 1.0f - 4.0f * std::abs (x - 0.5f) ;
        default: return x < 0.5f ? 1.0f : -1.0f;
    }
}
inline float foldF (float x) { float m = (x + 1.0f) * 0.25f; m -= std::floor (m); return 1.0f - std::abs (m * 4.0f - 2.0f); }
}

inline bool isHarmonicFamily (int x)
{
    const Family f = kSpecs[x].fam;
    return f != F_FM && f != F_DIST && f != F_SYNC;
}

/** Harmonic h (1..1023) of table x at morph m: returns a signed amplitude and a phase (radians), value = a * sin(2*pi*h*t + ph). */
inline void harmonic (int x, float m, int h, float& a, float& ph)
{
    using namespace detail;
    const Spec& s = kSpecs[x];
    const float* p = s.p;
    const float hf = (float) h;
    a = 0.0f; ph = 0.0f;
    switch (s.fam)
    {
        case F_SAWLP:
        {
            const float odd = std::min (1.0f, std::max (0.0f, p[4] + p[6] * m));
            const float w = (h % 2 == 1) ? 1.0f : (1.0f - odd);
            a = w * std::pow (hf, -p[3]) * lp (hf, p[0] + p[1] * m, p[2]);
            if (p[5] > 0.5f && (h % 4 == 3)) a = -a;
            break;
        }
        case F_SHAPES:
        {
            const float pos = std::min (3.0f, std::max (0.0f, p[0] + p[1] * m));
            const int i0 = std::min (2, (int) pos); const float fr = pos - (float) i0;
            auto coef = [h, hf] (int shape)
            {
                switch (shape)
                {
                    case 0:  return h == 1 ? 1.0f : 0.0f;
                    case 1:  return (h % 2 == 1) ? ((h % 4 == 1) ? 1.0f : -1.0f) * 8.0f / (kPi * kPi * hf * hf) : 0.0f;
                    case 2:  return ((h % 2 == 1) ? 1.0f : -1.0f) * 2.0f / (kPi * hf);
                    default: return (h % 2 == 1) ? 4.0f / (kPi * hf) : 0.0f;
                }
            };
            a = coef (i0) * (1.0f - fr) + coef (i0 + 1) * fr;
            break;
        }
        case F_PULSE:
        {
            const float d = std::min (0.97f, std::max (0.03f, p[0] + p[1] * m));
            const float cut = p[2] + p[3] * m;
            const float amp = (2.0f / (kPi * hf)) * std::sin (kPi * hf * d) * lp (hf, cut, 2.0f) * std::pow (hf, -p[4]);
            std::complex<float> c = std::polar (amp, 0.5f * kPi - kPi * hf * d);
            c += std::polar (p[5] / hf * lp (hf, cut, 2.0f), 0.0f);
            a = std::abs (c); ph = std::arg (c);
            break;
        }
        case F_RESO:
        {
            const float fc = p[0] + p[1] * m, g = (hf - fc) / std::max (0.2f, p[3]);
            a = std::pow (hf, -p[4]) * (1.0f + p[2] * std::exp (-g * g)) * lp (hf, p[5], 3.0f);
            break;
        }
        case F_COMB:
        {
            const float phi = p[0] + p[1] * m, ang = 2.0f * kPi * hf * phi;
            std::complex<float> c (1.0f, 0.0f);
            c += std::polar (1.0f, ang);
            c += std::polar (p[4], 2.0f * ang);
            a = std::abs (c) * std::pow (hf, -p[2]) * lp (hf, p[3], 2.0f); ph = std::arg (c);
            break;
        }
        case F_SERIES:
        {
            const int step = (int) std::lround (p[0]);
            const bool in = h == 1 || (step <= 0 ? isPrime (h) : (h % step == 0));
            const float leak = std::max (0.0f, p[3] + p[4] * m);
            a = (in ? 1.0f : leak) * std::pow (hf, -p[2]) * lp (hf, p[5], 2.0f);
            break;
        }
        case F_FORMANT:
        {
            static const float f[5][3] = { { 8.0f, 11.5f, 28.0f }, { 4.0f, 16.0f, 27.0f }, { 2.7f, 23.0f, 29.0f }, { 4.5f, 8.0f, 26.0f }, { 3.2f, 7.0f, 25.0f } };
            const int v0 = std::min (4, std::max (0, (int) std::lround (p[0]))), v1 = std::min (4, std::max (0, (int) std::lround (p[1])));
            const float shift = std::max (0.2f, p[2] + p[6] * m), bw = 1.3f * p[3];
            float F[3];
            for (int k = 0; k < 3; ++k) F[k] = (f[v0][k] + (f[v1][k] - f[v0][k]) * m) * shift;
            const float g0 = (hf - F[0]) / bw, g1 = (hf - F[1]) / (bw * 1.4f), g2 = (hf - F[2]) / (bw * 1.8f);
            a = (0.04f + std::exp (-g0 * g0) + 0.7f * std::exp (-g1 * g1) + 0.4f * std::exp (-g2 * g2)) * std::pow (hf, -p[4]);
            if (h > 80) a = 0.0f;
            break;
        }
        case F_RAND:
        {
            const int seed = (int) p[0];
            if (h > (int) p[1]) break;
            auto amp = [&] (int sd)
            {
                float u = hashU (h, sd);
                u = u < p[3] ? 0.0f : (u - p[3]) / (1.0f - p[3] + 1.0e-4f);
                return u;
            };
            a = (amp (seed) * (1.0f - m) + amp (seed + 1) * m) * std::pow (hf, -p[2]);
            ph = (hashU (h, seed + 100) * 2.0f - 1.0f) * kPi * p[4];
            break;
        }
        case F_METAL:
        {
            if (h > (int) p[4]) break;
            const float r = p[0] + p[1] * m, x = hf / r, d = std::abs (x - std::round (x)) / std::max (0.02f, p[2]);
            a = (p[5] + std::exp (-d * d)) * std::pow (hf, -p[3]);
            ph = hashU (h, 77) * 2.0f * kPi;
            break;
        }
        default: break;
    }
}

/** Time-domain families (FM / distortion / sync); t in [0,1). */
inline float timeDomain (int x, float m, float t)
{
    using namespace detail;
    const Spec& s = kSpecs[x];
    const float* p = s.p;
    switch (s.fam)
    {
        case F_FM:
        {
            const float I = p[0] + p[1] * m, r = std::round (p[2]), r2 = std::round (p[3]);
            return std::sin (2.0f * kPi * t + I * std::sin (2.0f * kPi * r * t + p[4] * std::sin (2.0f * kPi * r2 * t)));
        }
        case F_DIST:
        {
            const float tb = t + 0.5f * p[6] * m * std::cos (2.0f * kPi * t) / kPi;
            const float v = baseWave ((int) p[0], tb) * 0.9f + p[5], d = p[1] + p[2] * m;
            switch ((int) p[3])
            {
                case 0:  return std::tanh (d * v);
                case 1:  return foldF (d * v * 0.7f);
                case 2:  { const float q = std::pow (2.0f, std::max (1.0f, p[4])); return std::round (std::tanh (d * v) * q) / q; }
                case 3:  return std::max (-1.0f, std::min (1.0f, d * v));
                default: return std::tanh (d * std::max (0.0f, v)) + 0.3f * v;
            }
        }
        case F_SYNC:
        {
            const float ratio = p[0] + p[1] * m;
            return baseWave ((int) p[2], t * ratio);
        }
        default: return 0.0f;
    }
}
}
}
