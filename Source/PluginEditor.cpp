#include "PluginEditor.h"
#include "UI/Pages.h"

using namespace nusynth;

NuSynthEditor::NuSynthEditor (NuSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (p), headerRight (p), bottomBar (p), harmonic (p)
{
    setLookAndFeel (&laf);

    pages[0] = std::make_unique<ui::MainPage> (proc);
    pages[1] = std::make_unique<ui::Padded<ui::FxPanel>> (proc.apvts);
    pages[2] = std::make_unique<ui::OtherPage> (proc);

    aronLink.setFont (juce::Font (juce::FontOptions (12.0f)), false, juce::Justification::centredLeft);
    aronLink.setColour (juce::HyperlinkButton::textColourId, colours::accent);
    aronLink.setTooltip ("Open Aron H on Instagram");
    addAndMakeVisible (aronLink);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (headerRight);
    addAndMakeVisible (bottomBar);
    addChildComponent (harmonic);

    const char* names[kNumTabs] = { "MAIN", "FX", "OTHER" };
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& b = tabButtons[(size_t) i];
        b.setButtonText (names[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
        addChildComponent (*pages[(size_t) i]);
    }

    setResizable (true, true);
    setResizeLimits (940, 590, 2000, 1254);
    getConstrainer()->setFixedAspectRatio (1172.0 / 735.0);
    setSize (1172, 735);                              // 7% smaller than before

    showPage (0);
}

NuSynthEditor::~NuSynthEditor()
{
    setLookAndFeel (nullptr);
}

void NuSynthEditor::showPage (int index)
{
    for (int i = 0; i < kNumTabs; ++i)
    {
        pages[(size_t) i]->setVisible (i == index);
        tabButtons[(size_t) i].setToggleState (i == index, juce::dontSendNotification);
    }
}

void NuSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("NuSynth", 16, 6, 140, 30, juce::Justification::centredLeft);
}

void NuSynthEditor::openHarmonicEditor (int oscIndex)
{
    harmonic.setBounds (getLocalBounds().withTrimmedTop (58).withTrimmedBottom (108));
    harmonic.open (oscIndex);
}

void NuSynthEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (58);
    bottomBar.setBounds (r.removeFromBottom (108).reduced (8, 0));
    harmonic.setBounds (r);

    // header: [title+link] [preset bar (flexible)] [tabs] [undo redo tune master meter]
    auto right = header.removeFromRight (252).reduced (4, 2);
    headerRight.setBounds (right);
    auto tabs = header.removeFromRight (kNumTabs * 76 + 6).reduced (0, 14);
    for (auto& b : tabButtons) b.setBounds (tabs.removeFromLeft (76).reduced (2, 0));

    aronLink.setBounds (18, 38, 150, 16);
    auto presetArea = header.withTrimmedLeft (176).reduced (6, 15);
    presetBar.setBounds (presetArea);

    for (auto& p : pages)
        p->setBounds (r);
}
