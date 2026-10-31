#include "Fx.h"
#include "Parameters/FxSpecs.h"
#include "Shapers.h"
#include <cmath>
#include <algorithm>

namespace nusynth
{
namespace
{
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float tanhf_ (float x) { return std::tanh (x); }
}

void FxChain::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    const int maxB = std::max (maxBlock, 64);
    wetL.assign ((size_t) maxB, 0.0f);
    wetR.assign ((size_t) maxB, 0.0f);

    rv.reset();
    preL.prepare ((int) (0.25 * sr)); preR.prepare ((int) (0.25 * sr));
    dlyL.prepare ((int) (2.1 * sr)); dlyR.prepare ((int) (2.1 * sr));
    for (auto& m : mods) { m.l.prepare ((int) (0.06 * sr)); m.r.prepare ((int) (0.06 * sr)); m.phase = 0.0f; m.fbL = m.fbR = 0.0f; }
    for (auto& z : phZ) for (auto& v : z) v = 0.0f;
    phFb[0] = phFb[1] = 0.0f;
    for (auto& b : rvHp) b.clear();
    for (auto& b : rvLp) b.clear();
    for (auto& b : distLp) b.clear();
    for (int c = 0; c < 2; ++c)
    {
        eqLow[c].clear(); eqMid[c].clear(); eqHigh[c].clear(); eqHp[c].clear(); eqLp[c].clear(); crushHold[c] = 0.0f; crushCnt[c] = 0;
        for (int k = 0; k < 2; ++k) { xo1lp[c][k].clear(); xo1hp[c][k].clear(); xo2lp[c][k].clear(); xo2hp[c][k].clear(); }
    }
    for (auto& e : ottEnv) e = 0.0f;
    dlyLpL = dlyLpR = 0.0f;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxB, 2 };
    lim.prepare (spec);
    lim.reset();
}

void FxChain::bind (juce::AudioProcessorValueTreeState& a)
{
    for (int i = 0; i < kNumFxSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        s.on = a.getRawParameterValue (ids::fx (i, "on"));
        s.order = a.getRawParameterValue (ids::fx (i, "order"));
        s.mix = a.getRawParameterValue (ids::fx (i, "mix"));
        for (int k = 0; k < kMaxFxParams; ++k) s.p[k] = nullptr;
        const int n = (int) fxSpecs (i).size();
        for (int k = 0; k < n && k < kMaxFxParams; ++k) s.p[k] = a.getRawParameterValue (ids::fxp (i, k));
    }
}

void FxChain::process (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    if (n > (int) wetL.size()) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    // stable sort of slot indices by their "order" parameter
    for (int i = 0; i < kNumFxSlots; ++i) order[i] = i;
    std::stable_sort (order, order + kNumFxSlots, [this] (int a, int b)
    { return slots[(size_t) a].order->load() < slots[(size_t) b].order->load(); });

    for (int oi = 0; oi < kNumFxSlots; ++oi)
    {
        const int fx = order[oi];
        const auto& s = slots[(size_t) fx];
        if (s.on->load() < 0.5f) continue;

        Params p {};
        for (int k = 0; k < kMaxFxParams; ++k) p[(size_t) k] = s.p[k] != nullptr ? s.p[k]->load() : 0.0f;
        const float mix = s.mix->load();

        switch (fx)
        {
            case 0: reverb (L, R, n, p, mix); break;
            case 1: delay (L, R, n, p, mix); break;
            case 2: modFx (0, L, R, n, p, mix); break;
            case 3: modFx (1, L, R, n, p, mix); break;
            case 4: phaser (L, R, n, p, mix); break;
            case 5: distortion (L, R, n, p, mix); break;
            case 6: eq (L, R, n, p); break;
            case 7: ott (L, R, n, p, mix); break;
            default: limiter (L, R, n, p); break;
        }
    }
}

// ------------------------------------------------------------------ reverb: size, damping, width, predelay, lowcut, highcut
void FxChain::reverb (float* l, float* r, int n, const Params& p, float mix)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = p[0]; rp.damping = p[1]; rp.width = p[2];
    rp.wetLevel = 0.33f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
    rv.setParameters (rp);

    const float pd = p[3] * 0.001f * (float) sampleRate;
    for (int i = 0; i < n; ++i)
    {
        preL.push (l[i]); preR.push (r[i]);
        wetL[(size_t) i] = preL.read (pd + 1.0f);
        wetR[(size_t) i] = preR.read (pd + 1.0f);
    }
    rv.processStereo (wetL.data(), wetR.data(), n);

    const float sr = (float) sampleRate;
    for (int c = 0; c < 2; ++c) { rvHp[c].highpass (sr, p[4], 0.707f); rvLp[c].lowpass (sr, p[5], 0.707f); }
    for (int i = 0; i < n; ++i)
    {
        const float wl = rvLp[0].tick (rvHp[0].tick (wetL[(size_t) i]));
        const float wr = rvLp[1].tick (rvHp[1].tick (wetR[(size_t) i]));
        l[i] = l[i] * (1.0f - mix) + wl * mix;
        r[i] = r[i] * (1.0f - mix) + wr * mix;
    }
}

// ------------------------------------------------------------------ delay: timeL, timeR, feedback, tone, pingpong
void FxChain::delay (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    const float tgtL = std::max (1.0f, p[0] * 0.001f * sr), tgtR = std::max (1.0f, p[1] * 0.001f * sr);
    const float fb = p[2];
    const float lpA = 1.0f - std::exp (-6.28318530718f * std::min (p[3], 0.45f * sr) / sr);
    const bool ping = p[4] > 0.5f;

    for (int i = 0; i < n; ++i)
    {
        dlyTimeL += (tgtL - dlyTimeL) * 0.002f;
        dlyTimeR += (tgtR - dlyTimeR) * 0.002f;
        const float dl = dlyL.read (dlyTimeL), dr = dlyR.read (dlyTimeR);
        dlyLpL += lpA * (dl * fb - dlyLpL);
        dlyLpR += lpA * (dr * fb - dlyLpR);

        if (ping) { dlyL.push ((l[i] + r[i]) * 0.5f + dlyLpR); dlyR.push (dlyLpL); }
        else      { dlyL.push (l[i] + dlyLpL);                 dlyR.push (r[i] + dlyLpR); }

        l[i] = l[i] * (1.0f - mix) + dl * mix;
        r[i] = r[i] * (1.0f - mix) + dr * mix;
    }
}

// ------------------------------------------------------------------ chorus (0) / flanger (1): rate, depth, delay, feedback, width
void FxChain::modFx (int which, float* l, float* r, int n, const Params& p, float mix)
{
    auto& m = mods[which];
    const float sr = (float) sampleRate;
    const float base = std::max (1.0f, p[2] * 0.001f * sr);
    const float depth = p[1], fb = p[3], width = p[4];
    const float inc = p[0] / sr;

    for (int i = 0; i < n; ++i)
    {
        const float lfoL = std::sin (6.28318530718f * m.phase);
        const float lfoR = std::sin (6.28318530718f * (m.phase + width * 0.25f));
        const float dL = std::max (1.0f, base * (1.0f + 0.9f * depth * lfoL));
        const float dR = std::max (1.0f, base * (1.0f + 0.9f * depth * lfoR));
        const float wl = m.l.read (dL), wr = m.r.read (dR);
        m.l.push (l[i] + wl * fb);
        m.r.push (r[i] + wr * fb);
        l[i] = l[i] * (1.0f - mix * 0.5f) + wl * mix;
        r[i] = r[i] * (1.0f - mix * 0.5f) + wr * mix;
        m.phase += inc;
        if (m.phase >= 1.0f) m.phase -= 1.0f;
    }
}

// ------------------------------------------------------------------ phaser: rate, depth, centre, feedback, stages
void FxChain::phaser (float* l, float* r, int n, const Params& p, float mix)
{
    static const int stageTable[5] = { 2, 4, 6, 8, 12 };
    const int stages = stageTable[std::min (4, std::max (0, (int) p[4]))];
    const float sr = (float) sampleRate;
    const float inc = p[0] / sr;
    const float fb = p[3] * 0.9f;

    for (int i = 0; i < n; ++i)
    {
        if ((i & 15) == 0)
        {
            const float lfo = std::sin (6.28318530718f * phPhase);
            const float f = std::min (0.45f * sr, std::max (20.0f, p[2] * std::pow (2.0f, p[1] * 2.0f * lfo)));
            const float tn = std::tan (3.14159265359f * f / sr);
            phCoefL = phCoefR = (tn - 1.0f) / (tn + 1.0f);
        }
        float* io[2] = { &l[i], &r[i] };
        const float coef = phCoefL;
        for (int c = 0; c < 2; ++c)
        {
            const float x = *io[c];
            float y = x + fb * phFb[c];
            for (int s = 0; s < stages; ++s)
            {
                const float o = coef * y + phZ[c][s];
                phZ[c][s] = y - coef * o;
                y = o;
            }
            phFb[c] = y;
            *io[c] = x * (1.0f - mix * 0.5f) + y * mix * 0.5f + (mix > 0.0f ? 0.0f : 0.0f);
        }
        phPhase += inc;
        if (phPhase >= 1.0f) phPhase -= 1.0f;
    }
}

// ------------------------------------------------------------------ distortion: type, drive, tone, bits, output
void FxChain::distortion (float* l, float* r, int n, const Params& p, float mix)
{
    const int type = (int) p[0];
    const float g = dbToGain (p[1]);
    const float out = dbToGain (p[4]);
    const float sr = (float) sampleRate;
    for (int c = 0; c < 2; ++c) distLp[c].lowpass (sr, p[2], 0.707f);
    const float levels = std::pow (2.0f, std::max (1.0f, p[3] - 1.0f));

    const int hold = 1 + (int) ((16.0f - p[3]) * 1.5f);      // used by the Downsample type
    float* ch[2] = { l, r };
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            const float x = ch[c][i];
            const float v = x * g;
            float y;
            if (type == 11)
            {
                if (crushCnt[c] <= 0) { crushHold[c] = std::tanh (v); crushCnt[c] = std::max (1, hold); }
                --crushCnt[c];
                y = crushHold[c];
            }
            else y = distShape (type, v, levels);
            y = distLp[c].tick (y) * out;
            ch[c][i] = x * (1.0f - mix) + y * mix;
        }
}

// ------------------------------------------------------------------ 3-band EQ: lowFreq, lowGain, midFreq, midGain, midQ, highFreq, highGain
void FxChain::eq (float* l, float* r, int n, const Params& p)
{
    const float sr = (float) sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        eqLow[c].lowShelf (sr, p[0], p[1]);
        eqMid[c].peak (sr, p[2], p[4], p[3]);
        eqHigh[c].highShelf (sr, p[5], p[6]);
        eqHp[c].highpass (sr, p[7], 0.7071f);     // low cut
        eqLp[c].lowpass (sr, p[8], 0.7071f);      // high cut
    }
    for (int i = 0; i < n; ++i)
    {
        l[i] = eqLp[0].tick (eqHp[0].tick (eqHigh[0].tick (eqMid[0].tick (eqLow[0].tick (l[i])))));
        r[i] = eqLp[1].tick (eqHp[1].tick (eqHigh[1].tick (eqMid[1].tick (eqLow[1].tick (r[i])))));
    }
}

// ------------------------------------------------------------------ OTT style multiband up/down compressor: depth, time, lowGain, midGain, highGain
void FxChain::ott (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 2; ++k)
        {
            xo1lp[c][k].lowpass (sr, 120.0f, 0.7071f);  xo1hp[c][k].highpass (sr, 120.0f, 0.7071f);
            xo2lp[c][k].lowpass (sr, 2500.0f, 0.7071f); xo2hp[c][k].highpass (sr, 2500.0f, 0.7071f);
        }

    const float depth = p[0];
    const float atk = 1.0f - std::exp (-1.0f / (sr * (0.0005f + p[1] * 0.02f)));
    const float rel = 1.0f - std::exp (-1.0f / (sr * (0.02f + p[1] * 0.4f)));
    const float bandGain[3] = { dbToGain (p[2]), dbToGain (p[3]), dbToGain (p[4]) };

    for (int i = 0; i < n; ++i)
    {
        float in[2] = { l[i], r[i] };
        float lowB[2], midB[2], highB[2];
        for (int c = 0; c < 2; ++c)
        {
            lowB[c] = xo1lp[c][1].tick (xo1lp[c][0].tick (in[c]));
            const float rest = xo1hp[c][1].tick (xo1hp[c][0].tick (in[c]));
            midB[c] = xo2lp[c][1].tick (xo2lp[c][0].tick (rest));
            highB[c] = xo2hp[c][1].tick (xo2hp[c][0].tick (rest));
        }
        float* bands[3] = { lowB, midB, highB };
        float outS[2] = { 0.0f, 0.0f };
        for (int b = 0; b < 3; ++b)
        {
            const float lvl = std::max (std::abs (bands[b][0]), std::abs (bands[b][1]));
            ottEnv[b] += (lvl > ottEnv[b] ? atk : rel) * (lvl - ottEnv[b]);
            const float db = 20.0f * std::log10 (ottEnv[b] + 1.0e-6f);
            float gdb = 0.0f;
            if (db > -18.0f) gdb = -(db + 18.0f) * (1.0f - 1.0f / 4.0f);
            else if (db < -40.0f) gdb = std::min (24.0f, (-40.0f - db) * (1.0f - 1.0f / 3.0f));
            const float gain = dbToGain (gdb * depth) * bandGain[b];
            outS[0] += bands[b][0] * gain;
            outS[1] += bands[b][1] * gain;
        }
        l[i] = in[0] * (1.0f - mix) + outS[0] * mix;
        r[i] = in[1] * (1.0f - mix) + outS[1] * mix;
    }
}

// ------------------------------------------------------------------ limiter: threshold, release, gain
void FxChain::limiter (float* l, float* r, int n, const Params& p)
{
    lim.setThreshold (p[0]);
    lim.setRelease (p[1]);
    float* chans[2] = { l, r };
    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    lim.process (juce::dsp::ProcessContextReplacing<float> (block));
    const float g = dbToGain (p[2]);
    for (int i = 0; i < n; ++i) { l[i] *= g; r[i] *= g; }
}
}
