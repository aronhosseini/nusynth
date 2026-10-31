#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/NuLookAndFeel.h"
#include "UI/PresetBar.h"
#include "UI/BottomBar.h"
#include "UI/HeaderBar.h"
#include "UI/HarmonicEditor.h"

class NuSynthEditor final : public juce::AudioProcessorEditor,
                            public juce::DragAndDropContainer,       // lets LFO / ENV / VEL / KEY / MOD / macro keys be dragged onto knobs
                            public nusynth::ui::HarmonicHost
{
public:
    explicit NuSynthEditor (NuSynthAudioProcessor&);
    ~NuSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void openHarmonicEditor (int oscIndex) override;

private:
    static constexpr int kNumTabs = 3;
    void showPage (int index);

    NuSynthAudioProcessor& proc;
    nusynth::NuLookAndFeel laf;                      // declared first -> destroyed last
    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::HyperlinkButton aronLink { "developed by Aron H", juce::URL ("https://www.instagram.com/aronh.official") };
    nusynth::ui::PresetBar presetBar;
    nusynth::ui::HeaderRight headerRight;
    nusynth::ui::KeyboardBar bottomBar;
    std::array<juce::TextButton, kNumTabs> tabButtons;
    std::array<std::unique_ptr<juce::Component>, kNumTabs> pages;
    nusynth::ui::HarmonicEditor harmonic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NuSynthEditor)
};
