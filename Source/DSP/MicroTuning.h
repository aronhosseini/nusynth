#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include "Parameters/ParameterIDs.h"

namespace nusynth
{
/** Octave-based microtuning. 12 cent offsets (-100..+100), one per pitch class,
    applied to every MIDI note 0-127 (note % 12). Audio-thread safe: only reads
    atomics and writes to a table owned by the audio thread. */
class MicroTuning
{
public:
    void bind (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < kNumMicroKeys; ++i)
            cents[(size_t) i] = apvts.getRawParameterValue (ids::micro (i));

        masterTune = apvts.getRawParameterValue (ids::masterTune);

        for (int n = 0; n < 128; ++n)
            baseHz[(size_t) n] = 440.0f * std::pow (2.0f, (float) (n - 69) / 12.0f);

        update();
    }

    /** Call once per block on the audio thread (cheap: 12 pow + 128 mults). */
    void update() noexcept
    {
        const float tuneScale = masterTune != nullptr ? masterTune->load (std::memory_order_relaxed) / 440.0f : 1.0f;
        float mult[kNumMicroKeys];
        for (int i = 0; i < kNumMicroKeys; ++i)
            mult[i] = std::pow (2.0f, cents[(size_t) i]->load (std::memory_order_relaxed) / 1200.0f);

        for (int n = 0; n < 128; ++n)
            table[(size_t) n] = baseHz[(size_t) n] * tuneScale * mult[n % 12];
    }

    float frequencyForNote (int midiNote) const noexcept
    {
        return table[(size_t) juce::jlimit (0, 127, midiNote)];
    }

private:
    std::array<std::atomic<float>*, kNumMicroKeys> cents {};
    std::atomic<float>* masterTune = nullptr;
    std::array<float, 128> baseHz {};
    std::array<float, 128> table {};
};
}
