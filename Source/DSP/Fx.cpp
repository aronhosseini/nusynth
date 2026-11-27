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

/** audio thread: keep the largest value until the UI picks it up (it reads with exchange (0)) */
inline void atomicMax (std::atomic<float>& a, float v)
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}
enum { kMeterOtt = 0, kMeterLimiter = 1, kMeterComp = 2 };
}

void FxEngine::prepare (double sr, int maxBlock)
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
    for (auto& b : dlyHp) b.clear();
    for (auto& b : dlyLp) b.clear();
    fxFilt.prepare (sr);
    for (auto& r : revIn) r.prepare ((int) (2.0 * sr));
    for (auto& r : dlyRev) r.prepare ((int) (2.1 * sr));
    for (auto& b : shimBuf) b.prepare ((int) (0.3 * sr));
    shimPhase = tapePh = revFbL = revFbR = tapeHpL = tapeHpR = 0.0f;
    acGr = 0.0f;
    hypL.prepare ((int) (0.08 * sr)); hypR.prepare ((int) (0.08 * sr));
    dimL.prepare ((int) (0.04 * sr)); dimR.prepare ((int) (0.04 * sr));
    for (int v = 0; v < 7; ++v) hypPh[v] = (float) v / 7.0f;
    dimPhase = 0.0f;
    for (auto& b : dimHp) b.clear();

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxB, 2 };
    lim.prepare (spec);
    lim.reset();
}

void FxEngine::run (int type, float* l, float* r, int n, const Params& p, float mix)
{
    switch (type)
    {
        case 0: reverb (l, r, n, p, mix); break;
        case 1: delay (l, r, n, p, mix); break;
        case 2: modFx (0, l, r, n, p, mix); break;
        case 3: modFx (1, l, r, n, p, mix); break;
        case 4: phaser (l, r, n, p, mix); break;
        case 5: distortion (l, r, n, p, mix); break;
        case 6: eq (l, r, n, p); break;
        case 7:
            if (p[6] >= 0.5f) ott (l, r, n, p, mix);                 // mode "OTT": parameters p0..p5
            else                                                      // mode "Single": the analog compressor, parameters p7..p12
            {
                Params q {};
                for (int k = 0; k < 6; ++k) q[(size_t) k] = p[(size_t) (7 + k)];
                analogComp (l, r, n, q, mix);
            }
            break;
        case 8: limiter (l, r, n, p); break;
        case 10: filterFx (l, r, n, p, mix); break;
        case 11: hyperDim (l, r, n, p, mix); break;
        default: break;
    }
}

void FxChain::prepare (double sr, int maxBlock)
{
    for (auto& e : engines) e.prepare (sr, maxBlock);
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
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    for (auto& e : engines) e.setBpm (bpm);

    // stable sort of slot indices by their "order" parameter
    for (int i = 0; i < kNumFxSlots; ++i) order[i] = i;
    std::stable_sort (order, order + kNumFxSlots, [this] (int a, int b)
    { return slots[(size_t) a].order->load() < slots[(size_t) b].order->load(); });

    for (int oi = 0; oi < kNumFxSlots; ++oi)
    {
        const int fx = order[oi];
        const auto& s = slots[(size_t) fx];
        if (s.on->load() < 0.5f) continue;
        const int type = fxTypeOf (fx), copy = fxCopyOf (fx);

        // effective parameters = knob value + modulation from LFOs / envelopes / macros / matrix (also published for the UI)
        auto eff = [this] (float base, int dest, const juce::NormalisableRange<float>& rg)
        {
            if (shared == nullptr || dest < 0) return base;
            const float m = shared->live.mod[(size_t) dest].load (std::memory_order_relaxed);
            const float v = m == 0.0f ? base : rg.convertFrom0to1 (std::min (1.0f, std::max (0.0f, rg.convertTo0to1 (base) + m)));
            shared->live.v[(size_t) dest].store (v, std::memory_order_relaxed);
            return v;
        };
        FxEngine::Params p {};
        for (int k = 0; k < kMaxFxParams; ++k) p[(size_t) k] = s.p[k] != nullptr ? eff (s.p[k]->load(), s.dest[k], s.range[k]) : 0.0f;
        const float mix = s.mixDest >= 0 ? eff (s.mix->load(), s.mixDest, s.mixRange) : s.mix->load();
        engines[(size_t) copy].run (type, L, R, n, p, mix);
    }
}

// ------------------------------------------------------------------ reverb: size, damping, width, predelay, lowcut, highcut, type
float FxEngine::shimmerTick (int ch, float in)
{
    auto& b = shimBuf[ch];
    b.push (in);
    const float G = (float) (0.08 * sampleRate);
    float p1 = shimPhase, p2 = shimPhase + 0.5f;
    p2 -= std::floor (p2);
    const float w1 = std::sin (3.14159265f * p1), w2 = std::sin (3.14159265f * p2);
    return b.read (G * p1 + 1.0f) * w1 * w1 + b.read (G * p2 + 1.0f) * w2 * w2;
}

void FxEngine::reverb (float* l, float* r, int n, const Params& p, float mix)
{
    if (n > (int) wetL.size()) return;
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
    {   // Decay: caps the tail length.  Freeverb feedback = roomSize * 0.28 + 0.7  and  RT60 ~ 3 * 0.03 s / -log10 (feedback)
        const float decay = juce::jlimit (0.05f, 12.0f, p[7]);
        const float fb = std::pow (10.0f, -0.09f / decay);
        rp.roomSize = std::min (rp.roomSize, juce::jlimit (0.0f, 1.0f, (fb - 0.7f) / 0.28f));
    }
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
void FxEngine::delay (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    float tgtL = std::max (1.0f, p[0] * 0.001f * sr), tgtR = std::max (1.0f, p[1] * 0.001f * sr);
    if (p[5] >= 0.5f)   // tempo sync: note lengths instead of milliseconds
    {
        const float secPerBeat = 60.0f / bpm;
        tgtL = delaySyncBeats ((int) p[6]) * secPerBeat * sr;
        tgtR = delaySyncBeats ((int) p[7]) * secPerBeat * sr;
    }
    const float maxT = 2.0f * sr;                       // the delay lines hold 2.1 s
    tgtL = std::max (1.0f, std::min (maxT, tgtL)); tgtR = std::max (1.0f, std::min (maxT, tgtR));
    const float fb = p[2];
    const float lpA = 1.0f - std::exp (-6.28318530718f * std::min (p[3], 0.45f * sr) / sr);
    const int type = std::min (3, std::max (0, (int) p[4]));

    const bool lcOn = p[8] > 21.0f, hcOn = p[9] < 19900.0f;              // low cut / high cut (bypassed at their end stops)
    if (lcOn) for (auto& b : dlyHp) b.highpass (sr, p[8], 0.707f);
    if (hcOn) for (auto& b : dlyLp) b.lowpass (sr, p[9], 0.707f);
    auto cutWet = [&] (int c, float x)
    {
        if (lcOn) x = dlyHp[c].tick (x);
        if (hcOn) x = dlyLp[c].tick (x);
        return x;
    };

    for (int i = 0; i < n; ++i)
    {
        if (type == 3)
        {   // reverse delay: the last <time> milliseconds are played backwards
            const float ol = cutWet (0, dlyRev[0].process (l[i] + revFbL, (int) tgtL));
            const float orr = cutWet (1, dlyRev[1].process (r[i] + revFbR, (int) tgtR));
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
        const float dl = cutWet (0, dlyL.read (std::max (1.0f, tl))), dr = cutWet (1, dlyR.read (std::max (1.0f, tr)));
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
void FxEngine::modFx (int which, float* l, float* r, int n, const Params& p, float mix)
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
void FxEngine::phaser (float* l, float* r, int n, const Params& p, float mix)
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
void FxEngine::distortion (float* l, float* r, int n, const Params& p, float mix)
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
void FxEngine::eq (float* l, float* r, int n, const Params& p)
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
void FxEngine::ott (float* l, float* r, int n, const Params& p, float mix)
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
    float mIn = 0.0f, mOut = 0.0f, mRed = 0.0f, mUp = 0.0f;          // level display

    for (int i = 0; i < n; ++i)
    {
        float in[2] = { l[i], r[i] };
        mIn = std::max (mIn, std::max (std::abs (in[0]), std::abs (in[1])));
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
        float gSum = 0.0f;
        for (int b = 0; b < 3; ++b)
        {
            const float lvl = std::max (std::abs (bands[b][0]), std::abs (bands[b][1]));
            ottEnv[b] += (lvl > ottEnv[b] ? atk : rel) * (lvl - ottEnv[b]);
            const float db = 20.0f * std::log10 (ottEnv[b] + 1.0e-6f);
            float gdb = 0.0f;
            if (db > -18.0f) gdb = -(db + 18.0f) * (1.0f - 1.0f / 4.0f);
            else if (db < -40.0f) gdb = std::min (24.0f, (-40.0f - db) * (1.0f - 1.0f / 3.0f));
            const float gain = dbToGain (gdb * depth) * bandGain[b];
            gSum += gdb * depth;
            outS[0] += bands[b][0] * gain;
            outS[1] += bands[b][1] * gain;
        }
        const float gAvg = gSum * (1.0f / 3.0f);                      // average band gain change (dB): < 0 = down, > 0 = up
        mRed = std::max (mRed, -gAvg); mUp = std::max (mUp, gAvg);
        l[i] = in[0] * (1.0f - mix) + outS[0] * outGain * mix;
        r[i] = in[1] * (1.0f - mix) + outS[1] * outGain * mix;
        mOut = std::max (mOut, std::max (std::abs (l[i]), std::abs (r[i])));
    }
    if (shared != nullptr)
    {
        auto& m = shared->fxMeter;
        atomicMax (m.inPeak[kMeterOtt], mIn);  atomicMax (m.outPeak[kMeterOtt], mOut);
        atomicMax (m.reduction[kMeterOtt], mRed * mix);  atomicMax (m.boost[kMeterOtt], mUp * mix);
    }
}

// ------------------------------------------------------------------ limiter: threshold, release, gain, input, ceiling, soft clip, width
void FxEngine::limiter (float* l, float* r, int n, const Params& p)
{
    const float inGain = dbToGain (p[3]), sat = juce::jlimit (0.0f, 1.0f, p[5]), width = juce::jlimit (0.0f, 2.0f, p[6]);
    const float ceiling = dbToGain (std::min (0.0f, p[4]));
    float mIn = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float a = l[i] * inGain, b = r[i] * inGain;
        if (std::abs (width - 1.0f) > 0.001f) { const float m = 0.5f * (a + b), s = 0.5f * (a - b) * width; a = m + s; b = m - s; }   // stereo width (mid / side)
        if (sat > 0.0f) { a += sat * (std::tanh (a) - a); b += sat * (std::tanh (b) - b); }                                          // soft clip in front of the limiter
        l[i] = a; r[i] = b;
        mIn = std::max (mIn, std::max (std::abs (a), std::abs (b)));
    }

    lim.setThreshold (p[0]);
    lim.setRelease (p[1]);
    float* chans[2] = { l, r };
    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    lim.process (juce::dsp::ProcessContextReplacing<float> (block));

    float postLim = 0.0f;
    for (int i = 0; i < n; ++i) postLim = std::max (postLim, std::max (std::abs (l[i]), std::abs (r[i])));

    const float g = dbToGain (p[2]);
    float mOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        l[i] = juce::jlimit (-ceiling, ceiling, l[i] * g);
        r[i] = juce::jlimit (-ceiling, ceiling, r[i] * g);
        mOut = std::max (mOut, std::max (std::abs (l[i]), std::abs (r[i])));
    }

    if (shared != nullptr)
    {
        auto& m = shared->fxMeter;
        const float red = (mIn > 1.0e-5f && postLim > 1.0e-6f) ? std::max (0.0f, 20.0f * std::log10 (mIn / postLim)) : 0.0f;
        atomicMax (m.inPeak[kMeterLimiter], mIn);  atomicMax (m.outPeak[kMeterLimiter], mOut);
        atomicMax (m.reduction[kMeterLimiter], red);
    }
}

// ------------------------------------------------------------------ analog compressor: threshold, ratio, attack, release, knee, gain
void FxEngine::analogComp (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    const float thr = p[0], ratio = std::max (1.0f, p[1]), knee = std::max (0.01f, p[4]), makeup = dbToGain (p[5]);
    const float atk = std::exp (-1.0f / (sr * std::max (0.0001f, p[2] * 0.001f)));
    const float rel = std::exp (-1.0f / (sr * std::max (0.001f, p[3] * 0.001f)));
    float mIn = 0.0f, mOut = 0.0f, mRed = 0.0f;                        // level display

    for (int i = 0; i < n; ++i)
    {
        const float lvl = std::max (std::abs (l[i]), std::abs (r[i]));
        mIn = std::max (mIn, lvl);
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
        mRed = std::max (mRed, -acGr);
        mOut = std::max (mOut, std::max (std::abs (l[i]), std::abs (r[i])));
    }
    if (shared != nullptr)
    {
        auto& m = shared->fxMeter;
        atomicMax (m.inPeak[kMeterComp], mIn);  atomicMax (m.outPeak[kMeterComp], mOut);
        atomicMax (m.reduction[kMeterComp], mRed);
    }
}

// ------------------------------------------------------------------ filter: type (30 modes, same as the main filter), cutoff, resonance, drive
void FxEngine::filterFx (float* l, float* r, int n, const Params& p, float mix)
{
    fxFilt.setParams ((int) p[0], p[1], p[2], p[3], 1.0f);
    for (int i = 0; i < n; ++i)
    {
        float a = l[i], b = r[i];
        fxFilt.process (a, b);
        l[i] = l[i] * (1.0f - mix) + a * mix;
        r[i] = r[i] * (1.0f - mix) + b * mix;
    }
}

// ------------------------------------------------------------------ hyper / dimension: rate, detune, unison, width, hyper, size, dimension
// HYPER:     2..7 voices of the stereo signal, each through its own slowly modulated short delay (= detune), spread over the stereo field.
// DIMENSION: a mono-compatible, Dimension-D style widener: two opposite-polarity modulated delays fed back into the sides.
void FxEngine::hyperDim (float* l, float* r, int n, const Params& p, float mix)
{
    const float sr = (float) sampleRate;
    const int voices = 2 + std::min (5, std::max (0, (int) p[2]));
    const float rate = std::max (0.01f, p[0]), detune = juce::jlimit (0.0f, 1.0f, p[1]), width = juce::jlimit (0.0f, 1.0f, p[3]);
    const float hyper = juce::jlimit (0.0f, 1.0f, p[4]), size = juce::jlimit (0.0f, 1.0f, p[5]), dim = juce::jlimit (0.0f, 1.0f, p[6]);

    const float inc = rate / sr;
    const float centre = 0.011f * sr;                                  // ~11 ms centre delay of every voice
    const float depth = (0.0005f + 0.0042f * detune) * sr;             // modulation depth (the "detune")
    const float norm = 1.0f / std::sqrt ((float) voices);

    float gL[7], gR[7], rMul[7];
    for (int v = 0; v < voices; ++v)
    {
        const float pos = voices > 1 ? 2.0f * (float) v / (float) (voices - 1) - 1.0f : 0.0f;      // -1 .. +1 across the voices
        const float th = (pos * width * 0.5f + 0.5f) * 1.57079633f;                                  // constant-power pan, centre = unity
        gL[v] = std::cos (th) * 1.41421356f;
        gR[v] = std::sin (th) * 1.41421356f;
        rMul[v] = 1.0f + 0.113f * (float) v;
    }

    // Dimension
    const float dBase = (0.0015f + 0.0100f * size) * sr, dDepth = (0.0002f + 0.0009f * size) * sr;
    const float dInc = 0.35f / sr, wetGain = 0.85f * dim;
    for (auto& b : dimHp) b.highpass (sr, 140.0f, 0.707f);

    for (int i = 0; i < n; ++i)
    {
        const float dryL = l[i], dryR = r[i];
        hypL.push (dryL); hypR.push (dryR);

        float hl = 0.0f, hr = 0.0f;
        for (int v = 0; v < voices; ++v)
        {
            const float d = std::max (1.0f, centre + depth * std::sin (6.28318530718f * hypPh[v]));
            hypPh[v] += inc * rMul[v]; if (hypPh[v] >= 1.0f) hypPh[v] -= 1.0f;
            hl += hypL.read (d) * gL[v];
            hr += hypR.read (d) * gR[v];
        }
        hl *= norm; hr *= norm;
        float yl = dryL * (1.0f - 0.5f * hyper) + hl * hyper;
        float yr = dryR * (1.0f - 0.5f * hyper) + hr * hyper;

        if (dim > 0.0f)
        {
            const float m = 0.5f * (yl + yr);
            dimL.push (m); dimR.push (m);
            const float s = std::sin (6.28318530718f * dimPhase);
            const float dA = dimL.read (std::max (1.0f, dBase * (1.0f + 0.35f * s)  + dDepth * s));
            const float dB = dimR.read (std::max (1.0f, dBase * (1.0f - 0.35f * s)  - dDepth * s));
            const float wl = dimHp[0].tick (dA - 0.6f * dB), wr = dimHp[1].tick (dB - 0.6f * dA);
            yl += wl * wetGain; yr += wr * wetGain;
        }

        l[i] = dryL * (1.0f - mix) + yl * mix;
        r[i] = dryR * (1.0f - mix) + yr * mix;

        dimPhase += dInc; if (dimPhase >= 1.0f) dimPhase -= 1.0f;
    }
}
}
