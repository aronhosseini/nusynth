#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Parameters/ParameterIDs.h"

namespace nusynth
{
inline float ld (const std::atomic<float>* p) noexcept { return p->load (std::memory_order_relaxed); }

/** Raw (lock-free) pointers to every parameter the engine reads. */
struct EngineParams
{
    using A = std::atomic<float>*;

    struct Osc { A on, pitch, fine, unison, spread, detune, wavetable, position, warpMode, warpSrc, warp, phase, level, pan; } osc[kNumOsc];
    struct Flt { A on, keytrack, type, cutoff, res, drive, mix; } filt[2];
    struct Lfo { A shape, rate, depth, phase, sync, div, smooth; } lfo[kNumLfos];
    struct Env { A attack, decay, sustain, release, aCurve, dCurve, rCurve; } env[kNumEnvs];
    struct Slot { A src, dst, amount, curve, type; } slot[kNumModSlots];
    A subOn, noiseOn, noiseLoop;
    A subWave, subOct, subLevel, subPan, noiseType, noiseCutoff, noiseHP, noiseLP, noiseLevel, noisePitch, noiseFine, noisePan;
    A route[5];
    A maxVoices, bendUp, bendDown;
    A macro[kNumMacros];
    A playMode, glideTime, glideCurve;

    void bind (juce::AudioProcessorValueTreeState& a)
    {
        auto g = [&a] (const juce::String& id) { auto* p = a.getRawParameterValue (id); jassert (p != nullptr); return p; };

        for (int o = 0; o < kNumOsc; ++o)
        {
            auto& x = osc[o];
            x.pitch = g (ids::osc (o, "pitch"));       x.fine = g (ids::osc (o, "fine"));
            x.unison = g (ids::osc (o, "unison"));     x.spread = g (ids::osc (o, "spread"));
            x.detune = g (ids::osc (o, "detune"));     x.wavetable = g (ids::osc (o, "wavetable"));
            x.position = g (ids::osc (o, "position")); x.warpMode = g (ids::osc (o, "warpMode"));
            x.warp = g (ids::osc (o, "warp"));         x.phase = g (ids::osc (o, "phase"));
            x.level = g (ids::osc (o, "level"));
            x.pan = g (ids::osc (o, "pan"));
            x.on = g (ids::osc (o, "on"));
            x.warpSrc = g (ids::osc (o, "warpSrc"));
        }
        for (int f = 0; f < 2; ++f)
        {
            auto& x = filt[f];
            x.on = g (ids::filter (f, "on"));  x.keytrack = g (ids::filter (f, "keytrack"));       x.type = g (ids::filter (f, "type"));
            x.cutoff = g (ids::filter (f, "cutoff")); x.res = g (ids::filter (f, "res"));
            x.drive = g (ids::filter (f, "drive")); x.mix = g (ids::filter (f, "mix"));
        }
        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& x = lfo[l];
            x.shape = g (ids::lfo (l, "shape")); x.rate = g (ids::lfo (l, "rate")); x.depth = g (ids::lfo (l, "depth"));
            x.phase = g (ids::lfo (l, "phase")); x.sync = g (ids::lfo (l, "sync")); x.div = g (ids::lfo (l, "div"));
            x.smooth = g (ids::lfo (l, "smooth"));
        }
        for (int e = 0; e < kNumEnvs; ++e)
        {
            auto& x = env[e];
            x.attack = g (ids::env (e, "attack")); x.decay = g (ids::env (e, "decay")); x.sustain = g (ids::env (e, "sustain"));
            x.release = g (ids::env (e, "release")); x.aCurve = g (ids::env (e, "aCurve"));
            x.dCurve = g (ids::env (e, "dCurve")); x.rCurve = g (ids::env (e, "rCurve"));
        }
        for (int s = 0; s < kNumModSlots; ++s)
        {
            auto& x = slot[s];
            x.src = g (ids::mod (s, "src")); x.dst = g (ids::mod (s, "dst")); x.amount = g (ids::mod (s, "amount"));
            x.curve = g (ids::mod (s, "curve")); x.type = g (ids::mod (s, "type"));
        }
        subOn = g (ids::subOn); noiseOn = g (ids::noiseOn); noiseLoop = g (ids::noiseLoop);
        subPan = g (ids::subPan); noisePitch = g (ids::noisePitch); noiseFine = g (ids::noiseFine);
        noisePan = g (ids::noisePan); maxVoices = g (ids::maxVoices);
        bendUp = g (ids::bendUp); bendDown = g (ids::bendDown);
        for (int m = 0; m < kNumMacros; ++m) macro[m] = g (ids::macro (m));
        subWave = g (ids::subWave); subOct = g (ids::subOctave); subLevel = g (ids::subLevel);
        noiseType = g (ids::noiseType); noiseCutoff = g (ids::noiseCutoff); noiseHP = g (ids::noiseHP);
        noiseLP = g (ids::noiseLP); noiseLevel = g (ids::noiseLevel);
        route[0] = g (ids::route ("oscA")); route[1] = g (ids::route ("oscB")); route[2] = g (ids::route ("oscC"));
        route[3] = g (ids::route ("sub"));  route[4] = g (ids::route ("noise"));
        playMode = g (ids::playMode); glideTime = g (ids::glideTime); glideCurve = g (ids::glideCurve);
    }
};
}
