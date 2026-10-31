#include "Engine.h"
#include "LfoShape.h"
#include <cmath>
#include <algorithm>

namespace nusynth
{
namespace
{
inline float shp (float t, float e) { return e == 1.0f ? t : std::pow (t, e); }

inline float triFold (float x)
{
    float m = (x + 1.0f) * 0.25f;
    m -= std::floor (m);
    m *= 4.0f;
    return 1.0f - std::abs (m - 2.0f);
}

// 32 bar, 16 bar, 8 bar, 4 bar, 2 bar, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64   (in beats; a bar = 4 beats)
const float kBeats[12] = { 128.0f, 64.0f, 32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f };
}

float Engine::EnvState::tick (const Snap::E& e, float invSr)
{
    switch (stage)
    {
        case Attack:
            t += invSr / std::max (e.a, 0.0005f);
            if (t >= 1.0f) { level = 1.0f; stage = Decay; t = 0.0f; }
            else level = startLevel + (1.0f - startLevel) * shp (t, e.ea);
            break;
        case Decay:
            t += invSr / std::max (e.d, 0.0005f);
            if (t >= 1.0f) { level = e.s; stage = Sustain; }
            else level = 1.0f - (1.0f - e.s) * shp (t, e.ed);
            break;
        case Sustain:
            level = e.s;
            break;
        case Release:
            t += invSr / std::max (e.r, 0.0005f);
            if (t >= 1.0f) { level = 0.0f; stage = Idle; }
            else level = relStart * (1.0f - shp (t, e.er));
            break;
        default:
            level = 0.0f;
            break;
    }
    return level;
}

void Engine::prepare (double sr)
{
    sampleRate = sr;
    WavetableBank::get();   // make sure tables exist before audio starts
    for (auto& v : voices)
    {
        v = Voice();
        for (auto& f : v.flt) f.prepare (sr);
    }
    monoStack.clear();
    monoStack.reserve (128);
}

void Engine::bind (juce::AudioProcessorValueTreeState& apvts, const MicroTuning* t)
{
    tuning = t;
    params.bind (apvts);
    for (int i = 0; i < kNumDests; ++i)
    {
        const auto& id = destTable()[(size_t) i].id;
        destBase[(size_t) i] = apvts.getRawParameterValue (id);
        ranges[(size_t) i] = apvts.getParameter (id)->getNormalisableRange();
    }
}

void Engine::allNotesOff()
{
    for (auto& v : voices)
        if (v.active) { v.sustained = false; releaseVoice (v); }
    monoStack.clear();
    sustainDown = false;
}

void Engine::killAll()
{
    for (auto& v : voices)
    {
        v.active = false; v.held = false; v.sustained = false;
        for (auto& e : v.env) e = EnvState();
    }
    monoStack.clear();
    sustainDown = false;
}

double Engine::freqForSemis (double semis) const
{
    semis = std::min (127.0, std::max (0.0, semis));
    int i0 = (int) std::floor (semis);
    i0 = std::min (i0, 126);
    const double fr = semis - (double) i0;
    const double f0 = tuning->frequencyForNote (i0), f1 = tuning->frequencyForNote (i0 + 1);
    return f0 * std::pow (f1 / f0, fr);
}

// ------------------------------------------------------------------ snapshot of all block-rate params
void Engine::snapshot (double bpm)
{
    Snap& s = snap;
    s.bpm = (float) bpm;

    for (int d = 0; d < kNumDests; ++d)
    {
        s.base[d] = ld (destBase[(size_t) d]);
        s.norm[d] = ranges[(size_t) d].convertTo0to1 (s.base[d]);
    }
    for (int o = 0; o < kNumOsc; ++o)
    {
        const auto& p = params.osc[o];
        s.tbl[o] = std::min (WavetableBank::kTables - 1, std::max (0, (int) ld (p.wavetable)));
        s.warpMode[o] = (int) ld (p.warpMode);
        s.warpSrc[o] = (int) ld (p.warpSrc);
        s.unison[o] = std::min (16, std::max (1, (int) ld (p.unison)));
        s.oscPhase[o] = ld (p.phase) / 360.0f;
    }
    for (int o = 0; o < kNumOsc; ++o) s.oscOn[o] = ld (params.osc[o].on) > 0.5f;
    s.subOn = ld (params.subOn) > 0.5f; s.noiseOn = ld (params.noiseOn) > 0.5f; s.noiseLoop = ld (params.noiseLoop) > 0.5f;
    s.subWave = (int) ld (params.subWave);
    s.subOct = (int) ld (params.subOct);
    s.noiseType = (int) ld (params.noiseType);
    s.noiseHP = ld (params.noiseHP) > 0.5f;
    s.noiseLP = ld (params.noiseLP) > 0.5f;

    s.useF1 = s.useF2 = s.useSer = false;
    for (int i = 0; i < 5; ++i)
    {
        s.route[i] = (int) ld (params.route[i]);
        if (s.route[i] == 1 || s.route[i] == 3) s.useF1 = true;
        if (s.route[i] == 2 || s.route[i] == 3) s.useF2 = true;
        if (s.route[i] == 4) s.useSer = true;
    }
    for (int f = 0; f < 2; ++f)
    {
        s.fOn[f] = ld (params.filt[f].on) > 0.5f;
        s.fType[f] = (int) ld (params.filt[f].type);
        s.fKey[f] = ld (params.filt[f].keytrack) > 0.5f;
    }
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& L = s.lfo[l]; const auto& p = params.lfo[l];
        L.shape = (int) ld (p.shape);
        L.sync = ld (p.sync) > 0.5f;
        L.beats = kBeats[std::min (11, std::max (0, (int) ld (p.div)))];
        L.depth = ld (p.depth); L.phase = ld (p.phase); L.smooth = ld (p.smooth);
        if (shared != nullptr) L.custom = shared->lfo.get (l);
    }
    for (int e = 0; e < kNumEnvs; ++e)
    {
        auto& E = s.env[e]; const auto& p = params.env[e];
        E.a = ld (p.attack); E.d = ld (p.decay); E.s = ld (p.sustain); E.r = ld (p.release);
        E.ea = std::pow (3.0f, ld (p.aCurve)); E.ed = std::pow (3.0f, ld (p.dCurve)); E.er = std::pow (3.0f, ld (p.rCurve));
    }
    s.nSlots = 0;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& p = params.slot[i];
        const int src = (int) ld (p.src), dst = (int) ld (p.dst);
        const float amt = ld (p.amount);
        if (src <= 0 || dst <= 0 || amt == 0.0f) continue;
        auto& sl = s.slots[s.nSlots++];
        sl.src = src; sl.dst = dst - 1; sl.amount = amt;
        sl.expo = std::pow (4.0f, ld (p.curve));
        sl.bipolar = ((int) ld (p.type)) == 1;
    }
    s.playMode = (int) ld (params.playMode);
    s.maxVoices = std::min (kNumVoices, std::max (1, (int) ld (params.maxVoices)));
    s.glideMs = ld (params.glideTime);
    s.glideCurve = ld (params.glideCurve);
    for (int m = 0; m < kNumMacros; ++m) s.macro[m] = ld (params.macro[m]);
    s.bendUp = (int) ld (params.bendUp); s.bendDown = (int) ld (params.bendDown);
}

// ------------------------------------------------------------------ MIDI / voices
void Engine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                         noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                   noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
    else if (m.isPitchWheel())                pitchBend = std::max (-1.0, std::min (1.0, ((double) m.getPitchWheelValue() - 8192.0) / 8192.0));
    else if (m.isChannelPressure())           aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1) modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.getControllerNumber() == 64)
        {
            const bool down = m.getControllerValue() >= 64;
            if (sustainDown && ! down)
                for (auto& v : voices)
                    if (v.active && v.sustained) { v.sustained = false; releaseVoice (v); }
            sustainDown = down;
        }
    }
}

Engine::Voice* Engine::findFree()
{
    for (auto& v : voices) if (! v.active) return &v;
    return nullptr;
}

Engine::Voice* Engine::steal()
{
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! v.held && (best == nullptr || v.age < best->age)) best = &v;
    if (best != nullptr) return best;
    for (auto& v : voices)
        if (best == nullptr || v.age < best->age) best = &v;
    return best;
}

void Engine::startVoice (Voice& v, int note, float vel)
{
    const bool wasActive = v.active;
    v.note = note; v.vel = vel; v.held = true; v.sustained = false; v.age = ++ageCounter;
    v.monoVoice = snap.playMode != 0;
    v.samplePos = 0.0;
    v.tgtSemis = (double) note;

    if (! wasActive)
    {
        const double keepCur = (snap.glideMs >= 1.0f && lastValid) ? lastSemis : (double) note;
        v.curSemis = keepCur;
        for (auto& e : v.env) e = EnvState();
        for (auto& f : v.flt) f.reset();
        v.noise = NoiseState();
        v.fInit = false;
        for (auto& d : v.prevDM) d = 0.0f;
        for (int o = 0; o < kNumOsc; ++o)
            for (int u = 0; u < 16; ++u)
            {
                float p = snap.oscPhase[o] + (u == 0 ? 0.0f : rand01());
                p -= std::floor (p);
                v.phase[o][u] = p;
            }
        v.subPhase = 0.0f;
        for (int l = 0; l < kNumLfos; ++l)
        {
            v.lfo[l] = LfoState();
            v.lfo[l].phase = snap.lfo[l].phase;
            v.lfo[l].sh = rand01() * 2.0f - 1.0f;
        }
    }
    v.randVal = rand01();
    for (auto& e : v.env) e.noteOn();
    v.active = true;
}

void Engine::releaseVoice (Voice& v)
{
    v.held = false;
    for (auto& e : v.env) e.noteOff();
}

void Engine::noteOn (int note, float vel)
{
    if (snap.playMode == 0)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.held && x.note == note) { v = &x; break; }
        if (v == nullptr)
        {
            int activeCount = 0;
            for (auto& x : voices) if (x.active) ++activeCount;
            v = activeCount >= snap.maxVoices ? steal() : findFree();
        }
        if (v == nullptr) v = steal();
        startVoice (*v, note, vel);
    }
    else
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        monoStack.push_back (note);
        Voice& v = voices[0];
        if (snap.playMode == 2 && v.active && v.held)
        {
            v.tgtSemis = (double) note; v.note = note; v.vel = vel;
        }
        else startVoice (v, note, vel);
    }
    lastSemis = (double) note;
    lastValid = true;
}

void Engine::noteOff (int note)
{
    monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());

    for (auto& v : voices)
    {
        if (! v.active || ! v.held) continue;
        if (! v.monoVoice)
        {
            if (v.note != note) continue;                 // poly voice: match its key
        }
        else if (! monoStack.empty())
        {
            v.tgtSemis = (double) monoStack.back();       // mono / legato: fall back to the previous held key
            v.note = monoStack.back();
            continue;
        }
        if (sustainDown) v.sustained = true;
        else releaseVoice (v);
    }
}

// ------------------------------------------------------------------ rendering
void Engine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double bpm)
{
    snapshot (bpm);
    const int n = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = std::min (std::max (meta.samplePosition, pos), n);
        renderRange (buffer, pos, t);
        pos = t;
        handleMidi (meta.getMessage());
    }
    renderRange (buffer, pos, n);

    if (shared != nullptr && liveTarget == 0)          // nothing playing: show the plain parameter values
        for (int d = 0; d < kNumDests; ++d) shared->live.v[(size_t) d].store (snap.base[d], std::memory_order_relaxed);
}

void Engine::renderRange (juce::AudioBuffer<float>& buf, int start, int end)
{
    float* L = buf.getWritePointer (0);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : buf.getWritePointer (0);
    liveTarget = 0;
    for (auto& v : voices) if (v.active) liveTarget = std::max (liveTarget, v.age);
    while (start < end)
    {
        const int len = std::min (kChunk, end - start);
        for (auto& v : voices)
            if (v.active) renderVoice (v, L + start, R + start, len);
        start += len;
    }
}

float Engine::noiseTick (NoiseState& n, int type)
{
    const float w = rand01() * 2.0f - 1.0f;
    auto pink = [&n, w]
    {
        n.b[0] = 0.99886f * n.b[0] + w * 0.0555179f;
        n.b[1] = 0.99332f * n.b[1] + w * 0.0750759f;
        n.b[2] = 0.96900f * n.b[2] + w * 0.1538520f;
        n.b[3] = 0.86650f * n.b[3] + w * 0.3104856f;
        n.b[4] = 0.55000f * n.b[4] + w * 0.5329522f;
        n.b[5] = -0.7616f * n.b[5] - w * 0.0168980f;
        const float p = n.b[0] + n.b[1] + n.b[2] + n.b[3] + n.b[4] + n.b[5] + n.b[6] + w * 0.5362f;
        n.b[6] = w * 0.115926f;
        return p * 0.11f;
    };
    switch (type)
    {
        case 1:  return pink();
        case 2:  n.brown = (n.brown + 0.02f * w) / 1.02f; return n.brown * 3.5f;
        case 3:  { const float p = pink(); n.tape += 0.25f * (w - n.tape); const float hiss = n.tape - n.prev; n.prev = n.tape; return 0.7f * p + 0.5f * hiss; }
        case 4:  { if (rand01() < 0.004f) n.sh = rand01() * 2.0f - 1.0f; return 0.6f * n.sh + 0.25f * w; }
        default: return w;
    }
}

namespace
{
enum WarpMode { WM_Off, WM_Sync, WM_Remap, WM_BendP, WM_BendM, WM_BendPM, WM_PWM, WM_AsymP, WM_AsymM, WM_AsymPM,
                WM_Quant, WM_Mirror, WM_Fold, WM_FM, WM_AM, WM_RM };

inline float pivotMap (float t, float p) { return t < p ? 0.5f * t / p : 0.5f + 0.5f * (t - p) / (1.0f - p); }
}

void Engine::renderOsc (Voice& v, int o, const OscVals& p, double baseFreq, float* oL, float* oR, float* raw, const float* modSrc, int len)
{
    const auto& bank = WavetableBank::get();
    const int tbl = snap.tbl[o], mode = snap.warpMode[o], nUni = snap.unison[o];
    const float sr = (float) sampleRate;
    const float w = std::min (1.0f, std::max (0.0f, p.warp));

    const float frameF = std::min (1.0f, std::max (0.0f, p.position)) * (float) (WavetableBank::kFrames - 1);
    const int f0 = std::min ((int) frameF, WavetableBank::kFrames - 2);
    const float fw = frameF - (float) f0;

    const double fBase = baseFreq * std::pow (2.0, ((double) p.pitch + (double) p.fine / 100.0) / 12.0);
    const float warpMul = (mode == WM_Sync) ? 1.0f + w * 7.0f : 1.0f;
    const float norm = 1.0f / std::sqrt ((float) nUni);

    const float panA = (std::min (1.0f, std::max (-1.0f, p.pan)) + 1.0f) * 0.78539816f;
    const float panL = std::cos (panA) * 1.41421356f, panR = std::sin (panA) * 1.41421356f;
    float inc[16], gL[16], gR[16];
    const float* fr0[16];
    const float* fr1[16];
    for (int u = 0; u < nUni; ++u)
    {
        const float t = nUni > 1 ? (2.0f * (float) u / (float) (nUni - 1) - 1.0f) : 0.0f;
        const double fu = fBase * std::pow (2.0, (double) (t * p.detune * 75.0f) / 1200.0);
        inc[u] = (float) (fu / sampleRate);
        const int lvl = WavetableBank::levelFor ((float) fu * warpMul, sr);
        fr0[u] = bank.frame (tbl, lvl, f0);
        fr1[u] = bank.frame (tbl, lvl, f0 + 1);
        const float ang = (t * p.spread * 0.5f + 0.5f) * 1.57079632679f;
        gL[u] = std::cos (ang) * 1.41421356f * p.level * norm * panL;
        gR[u] = std::sin (ang) * 1.41421356f * p.level * norm * panR;
    }

    const float bendExp = 1.0f + w * 3.0f;
    float* ph = v.phase[o];
    const float kSize = (float) WavetableBank::kSize;
    for (int i = 0; i < len; ++i)
    {
        float sl = 0.0f, sr2 = 0.0f, sRaw = 0.0f;
        const float ms = modSrc != nullptr ? modSrc[i] : 0.0f;
        for (int u = 0; u < nUni; ++u)
        {
            float x = ph[u];
            float rp = x;
            switch (mode)
            {
                case WM_Sync:   rp = x * warpMul; rp -= std::floor (rp); break;
                case WM_Remap:  rp = x + (x * x * (3.0f - 2.0f * x) - x) * w; break;
                case WM_BendP:  rp = std::pow (x, bendExp); break;
                case WM_BendM:  rp = 1.0f - std::pow (1.0f - x, bendExp); break;
                case WM_BendPM: rp = x < 0.5f ? 0.5f * std::pow (2.0f * x, bendExp) : 1.0f - 0.5f * std::pow (2.0f - 2.0f * x, bendExp); break;
                case WM_PWM:    rp = pivotMap (x, 0.5f - 0.45f * w); break;
                case WM_Mirror: rp = x + ((x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x) - x) * w; break;
                case WM_FM:     rp = x + w * 1.5f * ms; rp -= std::floor (rp); break;
                default: break;
            }
            if (rp < 0.0f) rp = 0.0f;
            if (rp >= 1.0f) rp = 0.99999f;

            const float idx = rp * kSize;
            const int ii = (int) idx;
            const float fr = idx - (float) ii;
            const float* a = fr0[u];
            const float* c = fr1[u];
            const float va = a[ii] + (a[ii + 1] - a[ii]) * fr;
            const float vc = c[ii] + (c[ii + 1] - c[ii]) * fr;
            float val = va + (vc - va) * fw;

            switch (mode)
            {
                case WM_AsymP:  if (val < 0.0f) val *= 1.0f - 0.9f * w; break;
                case WM_AsymM:  if (val > 0.0f) val *= 1.0f - 0.9f * w; break;
                case WM_AsymPM: val = std::max (-1.2f, std::min (1.2f, val + 0.5f * w * val * std::abs (val))); break;
                case WM_Quant:  { const float L = (2.0f + (1.0f - w) * 62.0f) * 0.5f; val = std::floor (val * L + 0.5f) / L; break; }
                case WM_Fold:   val = triFold (val * (1.0f + w * 5.0f)); break;
                case WM_AM:     val *= (1.0f - w) + w * (0.5f + 0.5f * ms); break;
                case WM_RM:     val = val * (1.0f - w) + w * val * ms; break;
                default: break;
            }

            sRaw += val;
            sl += val * gL[u];
            sr2 += val * gR[u];
            x += inc[u];
            if (x >= 1.0f) x -= 1.0f;
            ph[u] = x;
        }
        raw[i] = sRaw * norm;
        oL[i] = sl;
        oR[i] = sr2;
    }
}

void Engine::renderVoice (Voice& v, float* outL, float* outR, int len)
{
    const Snap& b = snap;
    const float sr = (float) sampleRate, invSr = 1.0f / sr;

    // ---- glide (portamento)
    {
        const double diff = v.tgtSemis - v.curSemis;
        if (b.glideMs < 1.0f || std::abs (diff) < 1.0e-4) v.curSemis = v.tgtSemis;
        else
        {
            const float c = b.glideCurve;
            const float tc = (b.glideMs * 0.001f) * (c < 0.0f ? 1.0f - c * 1.5f : 1.0f / (1.0f + c * 3.0f)) / 3.0f;
            v.curSemis += diff * (1.0 - std::exp (-(double) len / ((double) sr * (double) std::max (tc, 0.001f))));
        }
    }

    // ---- LFOs (control rate)
    float src[kNumModSources] = {};
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& L = v.lfo[l];
        const auto& s = b.lfo[l];
        const int di = D_Lfo1Rate + l;
        float rate;
        if (s.sync) rate = (b.bpm / 60.0f) / s.beats;
        else rate = v.prevDM[di] == 0.0f ? b.base[di]
                                         : ranges[(size_t) di].convertFrom0to1 (std::min (1.0f, std::max (0.0f, b.norm[di] + v.prevDM[di])));
        L.phase += rate * (float) len * invSr;
        if (L.phase >= 1.0f)
        {
            L.phase -= std::floor (L.phase);
            if (s.shape == 5) L.sh = rand01() * 2.0f - 1.0f;
        }
        const float raw = lfoWave (s.shape, L.phase, s.custom, L.sh) * s.depth;
        if (s.smooth > 0.001f)
        {
            const float tau = s.smooth * s.smooth * 0.25f + 1.0e-4f;
            L.smooth += (raw - L.smooth) * (1.0f - std::exp (-(float) len * invSr / tau));
            src[MS_Lfo1 + l] = L.smooth;
        }
        else
        {
            L.smooth = raw;
            src[MS_Lfo1 + l] = raw;
        }
    }
    for (int e = 0; e < kNumEnvs; ++e) src[MS_Env1 + e] = v.envVal[e];
    src[MS_Velocity] = v.vel;
    src[MS_Key] = (float) v.note / 127.0f;
    src[MS_ModWheel] = modWheel;
    src[MS_Aftertouch] = aftertouch;
    src[MS_Random] = v.randVal;
    for (int m = 0; m < kNumMacros; ++m) src[MS_Macro1 + m] = b.macro[m];

    // ---- modulation matrix
    float dm[kNumDests] = {};
    for (int i = 0; i < b.nSlots; ++i)
    {
        const auto& sl = b.slots[i];
        const float s = src[sl.src];
        const float x01 = (sl.src >= MS_Lfo1 && sl.src <= MS_Lfo6) ? (s + 1.0f) * 0.5f : s;
        float x = sl.bipolar ? x01 * 2.0f - 1.0f : x01;
        if (sl.expo != 1.0f) x = std::copysign (std::pow (std::abs (x), sl.expo), x);
        dm[sl.dst] += x * sl.amount;
    }
    for (int d = 0; d < kNumDests; ++d) v.prevDM[d] = dm[d];

    auto M = [&] (int d) -> float
    {
        const float m = dm[d];
        if (m == 0.0f) return b.base[d];
        return ranges[(size_t) d].convertFrom0to1 (std::min (1.0f, std::max (0.0f, b.norm[d] + m)));
    };

    if (shared != nullptr && v.age == liveTarget)
        for (int d = 0; d < kNumDests; ++d) shared->live.v[(size_t) d].store (M (d), std::memory_order_relaxed);

    // ---- pitch (with configurable pitch-bend range)
    const double bendSemis = pitchBend * (double) (pitchBend >= 0.0 ? b.bendUp : b.bendDown);
    const double baseFreq = freqForSemis (v.curSemis) * std::pow (2.0, bendSemis / 12.0);

    // ---- which signals are needed as FM / AM / RM sources?
    bool srcUsed[5] = { false, false, false, false, false };
    for (int o = 0; o < kNumOsc; ++o)
        if (b.oscOn[o] && b.warpMode[o] >= WM_FM && b.warpSrc[o] != o) srcUsed[std::min (4, std::max (0, b.warpSrc[o]))] = true;

    const float subLevel = b.subOn ? M (D_SubLevel) : 0.0f;
    const float noiseLevel = b.noiseOn ? M (D_NoiseLevel) : 0.0f;

    // ---- sub oscillator (unit amplitude buffer)
    float subBuf[kChunk];
    {
        const float subInc = (float) (baseFreq * std::pow (2.0, (double) b.subOct) / (double) sr);
        const bool needSub = b.subOn && (subLevel > 0.0005f || srcUsed[3]);
        for (int i = 0; i < len; ++i)
        {
            float sub = 0.0f;
            if (needSub)
            {
                if (b.subWave == 0)      sub = std::sin (6.28318530718f * v.subPhase);
                else if (b.subWave == 1) sub = 1.0f - 4.0f * std::abs (std::fmod (v.subPhase + 0.25f, 1.0f) - 0.5f);
                else if (b.subWave == 2) sub = v.subPhase < 0.5f ? 0.7f : -0.7f;
                else if (b.subWave == 3) sub = 2.0f * v.subPhase - 1.0f;
                else                     sub = v.subPhase < 0.25f ? 0.7f : -0.7f;
            }
            subBuf[i] = sub;
            v.subPhase += subInc;
            if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
        }
    }

    // ---- noise / sample (unit amplitude buffer)
    float nzBuf[kChunk];
    {
        const float pitchSemis = M (D_NoisePitch) + M (D_NoiseFine) / 100.0f;
        const bool isSample = b.noiseType == 5;
        const float cutBase = M (D_NoiseCutoff) * (isSample ? 1.0f : std::pow (2.0f, pitchSemis / 12.0f));
        const float nA = 1.0f - std::exp (-6.28318530718f * std::min (cutBase, 0.45f * sr) * invSr);
        const float nAhp = 1.0f - std::exp (-6.28318530718f * std::min (cutBase * 0.5f, 0.45f * sr) * invSr);
        const bool needNoise = b.noiseOn && (noiseLevel > 0.0005f || srcUsed[4]);

        int sampLen = 0;
        const float* sampData = (isSample && nsample != nullptr) ? nsample->data (sampLen) : nullptr;
        const double sampRate = sampData != nullptr ? (double) nsample->sampleRate.load() / (double) sr * std::pow (2.0, ((double) pitchSemis + bendSemis) / 12.0) : 1.0;

        for (int i = 0; i < len; ++i)
        {
            float nz = 0.0f;
            if (needNoise)
            {
                if (isSample)
                {
                    if (sampData != nullptr && sampLen > 1 && (b.noiseLoop || v.samplePos < (double) sampLen - 1.0))
                    {
                        const int i0 = (int) v.samplePos;
                        const float fr = (float) (v.samplePos - (double) i0);
                        nz = sampData[i0 % sampLen] + (sampData[(i0 + 1) % sampLen] - sampData[i0 % sampLen]) * fr;
                        v.samplePos += sampRate;
                        if (b.noiseLoop && v.samplePos >= (double) sampLen) v.samplePos -= (double) sampLen;
                    }
                }
                else nz = noiseTick (v.noise, b.noiseType);
                if (b.noiseLP) { v.noise.lp += nA * (nz - v.noise.lp); nz = v.noise.lp; }
                if (b.noiseHP) { v.noise.hp += nAhp * (nz - v.noise.hp); nz -= v.noise.hp; }
                nz *= 0.5f;
            }
            nzBuf[i] = nz;
        }
    }

    // ---- oscillators (A, B, C) - cross modulation sources read earlier oscillators of this chunk or the previous chunk
    float oL[kNumOsc][kChunk], oR[kNumOsc][kChunk], raw[kNumOsc][kChunk];
    for (int o = 0; o < kNumOsc; ++o)
    {
        const int d0 = D_OscA + o * 8;
        OscVals pv { M (d0 + OD_Pitch), M (d0 + OD_Fine), M (d0 + OD_Position), M (d0 + OD_Spread),
                     M (d0 + OD_Detune), M (d0 + OD_Warp), M (d0 + OD_Level), M (d0 + OD_Pan) };
        if (! b.oscOn[o] || (pv.level < 0.0005f && ! srcUsed[o]))
        {
            for (int i = 0; i < len; ++i) oL[o][i] = oR[o][i] = raw[o][i] = 0.0f;
            continue;
        }
        const float* modSrc = nullptr;
        if (b.warpMode[o] >= WM_FM)
        {
            const int si = std::min (4, std::max (0, b.warpSrc[o]));
            if (si != o) modSrc = si < 3 ? (si < o ? raw[si] : v.prevRaw[si]) : (si == 3 ? subBuf : nzBuf);
        }
        renderOsc (v, o, pv, baseFreq, oL[o], oR[o], raw[o], modSrc, len);
    }
    for (int o = 0; o < kNumOsc; ++o)
        for (int i = 0; i < len; ++i) v.prevRaw[o][i] = raw[o][i];

    // ---- pan for sub / noise
    const float subPanA = (std::min (1.0f, std::max (-1.0f, M (D_SubPan))) + 1.0f) * 0.78539816f;
    const float subGL = std::cos (subPanA) * 1.41421356f, subGR = std::sin (subPanA) * 1.41421356f;
    const float noisePanA = (std::min (1.0f, std::max (-1.0f, M (D_NoisePan))) + 1.0f) * 0.78539816f;
    const float noiseGL = std::cos (noisePanA) * 1.41421356f, noiseGR = std::sin (noisePanA) * 1.41421356f;

    // ---- filters (with optional key tracking)
    const bool wantF1 = b.useF1, wantF2 = b.useF2, wantSer = b.useSer;
    const float keyMul = std::pow (2.0f, ((float) v.curSemis - 60.0f) / 12.0f);
    float tCut[2], tRes[2], tDrv[2], tMix[2];
    for (int f = 0; f < 2; ++f)
    {
        const int base = D_F1 + f * 4;
        tCut[f] = std::max (20.0f, M (base + FD_Cutoff) * (b.fKey[f] ? keyMul : 1.0f));
        tRes[f] = M (base + FD_Res); tDrv[f] = M (base + FD_Drive); tMix[f] = M (base + FD_Mix);
    }
    if (! v.fInit)
    {
        for (int f = 0; f < 2; ++f) { v.sCutLog[f] = std::log2 (tCut[f]); v.sRes[f] = tRes[f]; v.sDrv[f] = tDrv[f]; v.sMix[f] = tMix[f]; }
        v.fInit = true;
    }
    // Smooth the filter parameters (12 ms) and re-design the filters every 8 samples: moving a cutoff knob is click- and stair-free.
    const float smoothA = 1.0f - std::exp (-8.0f / (sr * 0.012f));
    auto updateFilters = [&]
    {
        for (int f = 0; f < 2; ++f)
        {
            v.sCutLog[f] += (std::log2 (tCut[f]) - v.sCutLog[f]) * smoothA;
            v.sRes[f] += (tRes[f] - v.sRes[f]) * smoothA;
            v.sDrv[f] += (tDrv[f] - v.sDrv[f]) * smoothA;
            v.sMix[f] += (tMix[f] - v.sMix[f]) * smoothA;
            const float cut = std::exp2 (v.sCutLog[f]);
            const bool direct = f == 0 ? wantF1 : wantF2;
            if (direct) v.flt[(size_t) f].setParams (b.fType[f], cut, v.sRes[f], v.sDrv[f], v.sMix[f]);
            if (wantSer) v.flt[(size_t) f + 2].setParams (b.fType[f], cut, v.sRes[f], v.sDrv[f], v.sMix[f]);
        }
    };

    // ---- per-sample mix / routing / amp
    for (int i = 0; i < len; ++i)
    {
        if ((i & 7) == 0) updateFilters();
        float sL[5], sR[5];
        for (int o = 0; o < kNumOsc; ++o) { sL[o] = oL[o][i]; sR[o] = oR[o][i]; }
        sL[3] = subBuf[i] * subLevel * subGL; sR[3] = subBuf[i] * subLevel * subGR;
        sL[4] = nzBuf[i] * noiseLevel * noiseGL; sR[4] = nzBuf[i] * noiseLevel * noiseGR;

        float byL = 0, byR = 0, f1L = 0, f1R = 0, f2L = 0, f2R = 0, paL = 0, paR = 0, seL = 0, seR = 0;
        for (int s = 0; s < 5; ++s)
        {
            switch (b.route[s])
            {
                case 0:  byL += sL[s]; byR += sR[s]; break;
                case 1:  f1L += sL[s]; f1R += sR[s]; break;
                case 2:  f2L += sL[s]; f2R += sR[s]; break;
                case 3:  paL += sL[s]; paR += sR[s]; break;
                default: seL += sL[s]; seR += sR[s]; break;
            }
        }

        float totL = byL, totR = byR;
        if (wantF1)
        {
            float l = f1L + paL, r = f1R + paR;
            if (b.fOn[0]) v.flt[0].process (l, r);
            totL += l; totR += r;
        }
        if (wantF2)
        {
            float l = f2L + paL, r = f2R + paR;
            if (b.fOn[1]) v.flt[1].process (l, r);
            totL += l; totR += r;
        }
        if (wantSer)
        {
            float l = seL, r = seR;
            if (b.fOn[0]) v.flt[2].process (l, r);
            if (b.fOn[1]) v.flt[3].process (l, r);
            totL += l; totR += r;
        }

        const float e1 = v.env[0].tick (b.env[0], invSr);
        for (int k = 1; k < kNumEnvs; ++k) v.env[k].tick (b.env[k], invSr);
        const float amp = e1 * v.vel * 0.5f;
        outL[i] += totL * amp;
        outR[i] += totR * amp;
    }

    for (int e = 0; e < kNumEnvs; ++e) v.envVal[e] = v.env[e].level;
    if (v.env[0].stage == EnvState::Idle) v.active = false;
}
}
