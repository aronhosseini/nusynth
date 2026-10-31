#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include "EngineParams.h"
#include "Filters.h"
#include "MicroTuning.h"
#include "ModDestinations.h"
#include "Wavetables.h"
#include "UserContent.h"
#include "SharedData.h"

namespace nusynth
{
/** Polyphonic wavetable engine: 2 wavetable oscillators (unison / warp), sub, noise, 2 filters with per-source routing,
    3 envelopes, 3 LFOs, 16-slot modulation matrix, poly / mono / legato / glide. */
class Engine
{
public:
    Engine() = default;

    void prepare (double sampleRate);
    void bind (juce::AudioProcessorValueTreeState& apvts, const MicroTuning* tuning);
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double bpm);
    void allNotesOff();
    void killAll();      // panic: silence everything immediately
    void setNoiseSample (const NoiseSample* s) { nsample = s; }
    void setShared (SharedData* s) { shared = s; }

private:
    static constexpr int kChunk = 32;

    struct Snap
    {
        float base[kNumDests] {}, norm[kNumDests] {};
        int tbl[kNumOsc] {}, warpMode[kNumOsc] {}, warpSrc[kNumOsc] {}, unison[kNumOsc] {};
        float oscPhase[kNumOsc] {};
        int subWave = 0, subOct = 0, noiseType = 0;
        bool noiseHP = false, noiseLP = true;
        int route[5] {};
        bool oscOn[kNumOsc] {}, subOn = true, noiseOn = true, noiseLoop = true;
        bool fOn[2] {}, fKey[2] {}; int fType[2] {};
        float macro[kNumMacros] {}; int bendUp = 2, bendDown = 2;
        bool useF1 = false, useF2 = false, useSer = false;
        struct L { int shape = 1; bool sync = false; float beats = 1.0f, depth = 1.0f, phase = 0.0f, smooth = 0.0f; LfoCustom custom; } lfo[kNumLfos];
        struct E { float a = 0.005f, d = 0.3f, s = 0.8f, r = 0.2f, ea = 1.0f, ed = 1.0f, er = 1.0f; } env[kNumEnvs];
        struct Slot { int src = 0, dst = 0; float amount = 0.0f, expo = 1.0f; bool bipolar = true; } slots[kNumModSlots];
        int nSlots = 0;
        int playMode = 0, maxVoices = 16; float glideMs = 0.0f, glideCurve = 0.0f; float bpm = 120.0f;
    };

    struct EnvState
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage = Idle;
        float level = 0.0f, t = 0.0f, startLevel = 0.0f, relStart = 0.0f;
        void noteOn()  { startLevel = level; t = 0.0f; stage = Attack; }
        void noteOff() { if (stage != Idle) { relStart = level; t = 0.0f; stage = Release; } }
        float tick (const Snap::E& e, float invSr);
    };

    struct LfoState { float phase = 0.0f, smooth = 0.0f, sh = 0.0f; };

    struct NoiseState
    {
        float b[7] {}; float brown = 0.0f, lp = 0.0f, hp = 0.0f, tape = 0.0f, sh = 0.0f, prev = 0.0f;
    };

    struct Voice
    {
        bool active = false, held = false, sustained = false, monoVoice = false;
        int note = 60; float vel = 1.0f; uint64_t age = 0;
        double curSemis = 60.0, tgtSemis = 60.0;
        float phase[kNumOsc][16] {};
        float subPhase = 0.0f, randVal = 0.0f;
        double samplePos = 0.0;
        float sCutLog[2] {}, sRes[2] {}, sDrv[2] {}, sMix[2] {};   // smoothed filter parameters (no zipper noise)
        bool fInit = false;
        float prevRaw[kNumOsc][kChunk] {};
        EnvState env[kNumEnvs];
        float envVal[kNumEnvs] {};
        LfoState lfo[kNumLfos];
        NoiseState noise;
        float prevDM[kNumDests] {};
        std::array<FilterUnit, 4> flt;   // 0: F1 (direct/parallel)  1: F2 (direct/parallel)  2: F1 (series)  3: F2 (series)
    };

    struct OscVals { float pitch, fine, position, spread, detune, warp, level, pan; };

    void snapshot (double bpm);
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float vel);
    void noteOff (int note);
    void startVoice (Voice& v, int note, float vel);
    void releaseVoice (Voice& v);
    Voice* findFree();
    Voice* steal();
    void renderRange (juce::AudioBuffer<float>& buf, int start, int end);
    void renderVoice (Voice& v, float* outL, float* outR, int len);
    void renderOsc (Voice& v, int o, const OscVals& p, double baseFreq, float* oL, float* oR, float* raw, const float* modSrc, int len);
    double freqForSemis (double semis) const;
    float noiseTick (NoiseState& n, int type);
    uint32_t nextRand() { rng = rng * 1664525u + 1013904223u; return rng; }
    float rand01() { return (float) (nextRand() >> 8) / 16777216.0f; }

    double sampleRate = 44100.0;
    EngineParams params;
    const MicroTuning* tuning = nullptr;
    const NoiseSample* nsample = nullptr;
    SharedData* shared = nullptr;
    uint64_t liveTarget = 0;
    std::array<std::atomic<float>*, kNumDests> destBase {};
    std::array<juce::NormalisableRange<float>, kNumDests> ranges;
    Snap snap;
    std::array<Voice, kNumVoices> voices;
    std::vector<int> monoStack;
    uint64_t ageCounter = 0;
    uint32_t rng = 12345u;
    double pitchBend = 0.0, lastSemis = 60.0;
    bool lastValid = false, sustainDown = false;
    float modWheel = 0.0f, aftertouch = 0.0f;
    std::atomic<float>* dummy = nullptr;
};
}
