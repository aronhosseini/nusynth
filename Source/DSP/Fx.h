#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include "Parameters/ParameterIDs.h"

namespace nusynth
{
/** RBJ biquad (no allocation, safe to retune on the audio thread). */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void clear() { z1 = z2 = 0; }
    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }

    void lowpass (float sr, float f, float Q)
    {
        const float w = 6.28318530718f * std::min (f, 0.45f * sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (float sr, float f, float Q)
    {
        const float w = 6.28318530718f * std::min (f, 0.45f * sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (float sr, float f, float Q, float gainDb)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f), w = 6.28318530718f * std::min (f, 0.45f * sr) / sr;
        const float c = std::cos (w), al = std::sin (w) / (2 * Q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float sr, float f, float gainDb)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f), w = 6.28318530718f * std::min (f, 0.45f * sr) / sr;
        const float c = std::cos (w), al = std::sin (w) / 2.0f * 1.41421356f, sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float sr, float f, float gainDb)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f), w = 6.28318530718f * std::min (f, 0.45f * sr) / sr;
        const float c = std::cos (w), al = std::sin (w) / 2.0f * 1.41421356f, sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

struct DelayLine
{
    std::vector<float> buf; int mask = 0, wp = 0;
    void prepare (int maxSamples) { int s = 1; while (s < maxSamples + 4) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; wp = 0; }
    void push (float x) { buf[(size_t) wp] = x; wp = (wp + 1) & mask; }
    float read (float d) const
    {
        float rp = (float) wp - d;
        while (rp < 0.0f) rp += (float) (mask + 1);
        const int i = (int) rp; const float f = rp - (float) i;
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + (b - a) * f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
};

/** Serum-style FX rack: 9 re-orderable effects, each with its own parameter set (see FxSpecs.h). */
class FxChain
{
public:
    void prepare (double sr, int maxBlock);
    void bind (juce::AudioProcessorValueTreeState& apvts);
    void process (juce::AudioBuffer<float>& buffer);

private:
    struct Slot { std::atomic<float>* on; std::atomic<float>* order; std::atomic<float>* mix; std::atomic<float>* p[kMaxFxParams]; };
    using Params = std::array<float, kMaxFxParams>;

    void reverb (float* l, float* r, int n, const Params& p, float mix);
    void delay (float* l, float* r, int n, const Params& p, float mix);
    void modFx (int which, float* l, float* r, int n, const Params& p, float mix);
    void phaser (float* l, float* r, int n, const Params& p, float mix);
    void distortion (float* l, float* r, int n, const Params& p, float mix);
    void eq (float* l, float* r, int n, const Params& p);
    void ott (float* l, float* r, int n, const Params& p, float mix);
    void limiter (float* l, float* r, int n, const Params& p);

    double sampleRate = 44100.0;
    std::array<Slot, kNumFxSlots> slots {};

    // reverb
    juce::Reverb rv;
    DelayLine preL, preR;
    Biquad rvHp[2], rvLp[2];
    std::vector<float> wetL, wetR;
    // delay
    DelayLine dlyL, dlyR;
    float dlyTimeL = 11025.0f, dlyTimeR = 16537.0f, dlyLpL = 0.0f, dlyLpR = 0.0f;
    // chorus / flanger
    struct Mod { DelayLine l, r; float phase = 0.0f; float fbL = 0.0f, fbR = 0.0f; } mods[2];
    // phaser
    float phZ[2][12] {}, phFb[2] {}, phPhase = 0.0f, phCoefL = 0.0f, phCoefR = 0.0f;
    // distortion
    Biquad distLp[2];
    float crushHold[2] {}; int crushCnt[2] {};
    // eq
    Biquad eqLow[2], eqMid[2], eqHigh[2], eqHp[2], eqLp[2];
    // ott
    Biquad xo1lp[2][2], xo1hp[2][2], xo2lp[2][2], xo2hp[2][2];
    float ottEnv[3] {};
    // limiter
    juce::dsp::Limiter<float> lim;
    int order[kNumFxSlots] {};
};
}
