#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/MicroTuning.h"
#include "DSP/Engine.h"
#include "DSP/Arpeggiator.h"
#include "DSP/Fx.h"
#include "DSP/UserContent.h"
#include "AI/PromptBridge.h"
#include "Presets/PresetManager.h"
#include "Presets/UndoHistory.h"
#include "DSP/SharedData.h"

class NuSynthAudioProcessor final : public juce::AudioProcessor
{
public:
    NuSynthAudioProcessor();
    ~NuSynthAudioProcessor() override { keyboardState.removeListener (&collector); nusynth::unregisterShared (&apvts); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NuSynth"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    nusynth::PromptBridge& getPromptBridge() { return promptBridge; }
    nusynth::PresetManager& getPresets() { return presets; }
    nusynth::UserContent& getUserContent() { return userContent; }
    const nusynth::NoiseSample& getNoiseSample() const { return noiseSample; }
    nusynth::UndoHistory& getUndo() { return undo; }
    void syncShapesFromState();
    void requestPanic() { panicRequested.store (true); }
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };               // output meter
    std::atomic<juce::uint64> heldLo { 0 }, heldHi { 0 };            // notes currently sent to the engine (keyboard display)
    std::atomic<float> uiVelocity { 0.8f };                          // velocity used by the on-screen keyboard

    // on-screen keyboard / wheels feed MIDI through the collector (thread safe)
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector collector;
    void sendMidi (const juce::MidiMessage& m)
    {
        auto msg = m;
        msg.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
        collector.addMessageToQueue (msg);
    }
    std::atomic<int> lastNote { 60 };      // for the KEY / VEL display
    std::atomic<float> lastVelocity { 0.0f };

private:
    nusynth::SharedData shared;
    nusynth::NoiseSample noiseSample;
    nusynth::UserContent userContent { noiseSample };
    nusynth::MicroTuning tuning;
    nusynth::Engine engine;
    nusynth::Arpeggiator arp;
    nusynth::FxChain fx;
    juce::MidiBuffer arpOut;
    std::atomic<float>* masterGainDb = nullptr;
    std::atomic<bool> panicRequested { false };
    std::array<int, 128> noteMap {};
    juce::MidiBuffer shifted;
    nusynth::PresetManager presets;
    nusynth::UndoHistory undo;
    nusynth::PromptBridge promptBridge;   // declared last -> destroyed first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NuSynthAudioProcessor)
};
