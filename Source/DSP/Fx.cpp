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
    for (auto& r : revIn) r.prepare ((int) (2.0 * sr));
    for (auto& r : dlyRev) r.prepare ((int) (2.1 * sr));
    for (auto& b : shimBuf) b.prepare ((int) (0.3 * sr));
    shimPhase = tapePh = revFbL = revFbR = tapeHpL = tapeHpR = 0.0f;
    acGr = 0.0f;

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
        for (int k = 0; k < kMaxFxParams; ++k) { s.p[k] = nullptr; s.dest[k] = -1; }
        const int n = (int) fxSpecs (i).size();
        for (int k = 0; k < n && k < kMaxFxParams; ++k)
        {
            s.p[k] = a.getRawParameterValue (ids::fxp (i, k));
            s.dest[k] = destForParam (ids::fxp (i, k));
            if (s.dest[k] >= 0) s.range[k] = a.getParameter (ids::fxp (i, k))->getNormalisableRange();
        }
        s.mixDest = destForParam (ids::fx (i, "mix"));
        if (s.mixDest >= 0) s.mixRange = a.getParameter (ids::fx (i, "mix"))->getNormalisableRange();
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

        // effective parameters = knob value + modulation from LFOs / envelopes / macros / matrix (also published for the UI)
        auto eff = [this] (float base, int dest, const juce::NormalisableRange<float>& rg)
        {
            if (shared == nullptr || dest < 0) return base;
            const float m = shared->live.mod[(size_t) dest].load (std::memory_order_relaxed);
            const float v = m == 0.0f ? base : rg.convertFrom0to1 (std::min (1.0f, std::max (0.0f, rg.convertTo0to1 (base) + m)));
            shared->live.v[(size_t) dest].store (v, std::memory_order_relaxed);
            return v;
        };
        Params p {};
        for (int k = 0; k < kMaxFxParams; ++k) p[(size_t) k] = s.p[k] != nullptr ? eff (s.p[k]->load(), s.dest[k], s.range[k]) : 0.0f;
        const float mix = s.mixDest >= 0 ? eff (s.mix->load(), s.mixDest, s.mixRange) : s.mix->load();
        if (s.on->load() < 0.5f) continue;

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
            case 8: limiter (L, R, n, p); break;
            default: analogComp (L, R, n, p, mix); break;
        }
    }
}

// ------------------------------------------------------------------ reverb: size, damping, width, predelay, lowcut, highcut, type
float FxChain::shimmerTick (int ch, float in)
{
    auto& b = shimBuf[ch];
    b.push (in);
    const float G = (float) (0.08 * sampleRate);
    float p1 = shimPhase, p2 = shimPhase + 0.5f;
    p2 -= std::floor (p2);
    const float w1 = std::sin (3.14159265f * p1), w2 = std::sin (3.14159265f * p2);
    return b.read (G * p1 + 1.0f) * w1 * w1 + b.read (G * p2 + 1.0f) * w2 * w2;
}

void FxChain::reverb (float* l, float* r, int n, const Params& p, float mix)
{
    const int type = std::min (5, std::max (0, (int) p[6]));
    float size = p[0], damp = p[1], extraPre = 0.0f;
    switch (type)
    {
        case 1:  size *= 0.42f; damp = std::min (1.0f, damp + 0.25f); break;               // room
        case 2:  size = 0.35f + 0.45f * size; damp *= 0.4f; break;                         // plate
        case 3:  size = 0.8f + 0.2f * size; damp *= 0.5f; extraPre = 25.0f; break;         // cathedral
        default: break;
    }
    juce::Reverb::Parameters rp;
    rp.roomSize = std::min (1.0f, size); rp.damping = std::min (1.0f, damp); rp.width = p[2];
    rp.wetLevel = 0.33f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
    rv.setParameters (rp);

    const float sr = (float) sampleRate;
    const float pd = (p[3] + extraPre) * 0.001f * sr;
    const int grain = (int) ((0.18f + 0.9f * p[0]) * sr);
    for (int i = 0; i < n; ++i)
    {
        preL.push (l[i]); preR.push (r[i]);
        float inL = preL.read (pd + 1.0f), inR = preR.read (pd + 1.0f);
        if (type == 5) { inL = revIn[0].process (inL, grain); inR = revIn[1].process (inR, grain); }    // reverse
        wetL[(size_t) i] = inL;
        wetR[(size_t) i] = inR;
    }
    rv.processStereo (wetL.data(), wetR.data(), n);

    if (type == 4)   // shimmer: add an octave-up copy of the tail
    {
        const float G = (float) (0.08 * sampleRate);
        for (int i = 0; i < n; ++i)
        {
            wetL[(size_t) i] += 0.4f * shimmerTick (0, wetL[(size_t) i]);
            wetR[(size_t) i] += 0.4f * shimmerTick (1, wetR[(size_t) i]);
            shimPhase -= 1.0f / G;
            if (shimPhase < 0.0f) shimPhase += 1.0f;
        }
    }

    for (int c = 0; c < 2; ++c) { rvHp[c].highpass (sr, p[4], 0.707f); rvLp[c].lowpass (sr, p[5], 0.707f); }
    for (int i = 0; i < n; ++i)
    {
        const float wl = rvLp[0].tick (rvHp[0].tick (wetL[(size_t) i]));
        const float wr = rvLp[1].tick (rvHp[1].tick (wetR[(size_t) i]));
        l[i] = l[i] * (1.0f - mix) + wl * mix;
        r[i] = r[i] * (1.0f - mix) + wr * mix;
    }
}

// ------------------------------------------------------------------ delay: timeL, timeR, feedback, tone, type (stereo / ping-pong / tape echo / reverse)
void FxChain::delay (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    const float tgtL = std::max (1.0f, p[0] * 0.001f * sr), tgtR = std::max (1.0f, p[1] * 0.001f * sr);
    const float fb = p[2];
    const float lpA = 1.0f - std::exp (-6.28318530718f * std::min (p[3], 0.45f * sr) / sr);
    const int type = std::min (3, std::max (0, (int) p[4]));

    for (int i = 0; i < n; ++i)
    {
        if (type == 3)
        {   // reverse delay: the last <time> milliseconds are played backwards
            const float ol = dlyRev[0].process (l[i] + revFbL, (int) tgtL);
            const float orr = dlyRev[1].process (r[i] + revFbR, (int) tgtR);
            revFbL += lpA * (ol * fb - revFbL);
            revFbR += lpA * (orr * fb - revFbR);
            l[i] = l[i] * (1.0f - mix) + ol * mix;
            r[i] = r[i] * (1.0f - mix) + orr * mix;
            continue;
        }

        dlyTimeL += (tgtL - dlyTimeL) * 0.002f;
        dlyTimeR += (tgtR - dlyTimeR) * 0.002f;
        float tl = dlyTimeL, tr = dlyTimeR;
        if (type == 2)
        {   // tape echo: wow + flutter on the delay time
            tapePh += 0.7f / sr; if (tapePh >= 1.0f) tapePh -= 1.0f;
            const float wob = 0.0007f * sr * std::sin (6.28318530718f * tapePh) + 0.00015f * sr * std::sin (6.28318530718f * tapePh * 9.0f);
            tl += wob; tr -= wob;
        }
        const float dl = dlyL.read (std::max (1.0f, tl)), dr = dlyR.read (std::max (1.0f, tr));
        float fL = dl * fb, fR = dr * fb;
        if (type == 2)
        {   // saturated, band-limited feedback like an old tape machine
            fL = std::tanh (fL * 1.4f); fR = std::tanh (fR * 1.4f);
            tapeHpL += 0.02f * (fL - tapeHpL); tapeHpR += 0.02f * (fR - tapeHpR);
            fL -= tapeHpL; fR -= tapeHpR;
        }
        dlyLpL += lpA * (fL - dlyLpL);
        dlyLpR += lpA * (fR - dlyLpR);

        if (type == 1) { dlyL.push ((l[i] + r[i]) * 0.5f + dlyLpR); dlyR.push (dlyLpL); }
        else           { dlyL.push (l[i] + dlyLpL);                 dlyR.push (r[i] + dlyLpR); }

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
    const float outGain = dbToGain (p[5]);

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
        l[i] = in[0] * (1.0f - mix) + outS[0] * outGain * mix;
        r[i] = in[1] * (1.0f - mix) + outS[1] * outGain * mix;
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

// ------------------------------------------------------------------ analog compressor: threshold, ratio, attack, release, knee, gain
void FxChain::analogComp (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    const float thr = p[0], ratio = std::max (1.0f, p[1]), knee = std::max (0.01f, p[4]), makeup = dbToGain (p[5]);
    const float atk = std::exp (-1.0f / (sr * std::max (0.0001f, p[2] * 0.001f)));
    const float rel = std::exp (-1.0f / (sr * std::max (0.001f, p[3] * 0.001f)));

    for (int i = 0; i < n; ++i)
    {
        const float lvl = std::max (std::abs (l[i]), std::abs (r[i]));
        const float lvlDb = 20.0f * std::log10 (lvl + 1.0e-9f);
        const float over = lvlDb - thr;
        float outDb;
        if (2.0f * over < -knee)              outDb = lvlDb;
        else if (2.0f * std::abs (over) <= knee) { const float t = over + knee * 0.5f; outDb = lvlDb + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee); }
        else                                  outDb = thr + over / ratio;
        const float target = outDb - lvlDb;                       // <= 0 : wanted gain reduction in dB
        const float coef = target < acGr ? atk : rel;             // attack when reduction increases
        acGr = coef * acGr + (1.0f - coef) * target;
        const float g = dbToGain (acGr) * makeup;
        const float yl = std::tanh (l[i] * g * 1.03f) / 1.03f, yr = std::tanh (r[i] * g * 1.03f) / 1.03f;   // gentle analog-style saturation
        l[i] = l[i] * (1.0f - mix) + yl * mix;
        r[i] = r[i] * (1.0f - mix) + yr * mix;
    }
}
}
