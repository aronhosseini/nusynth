#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"

namespace nusynth::ui
{
/** Tiny stereo output meter. */
struct LevelMeter final : juce::Component, private juce::Timer
{
    explicit LevelMeter (NuSynthAudioProcessor& p) : proc (p) { startTimerHz (30); }
    ~LevelMeter() override { stopTimer(); }

    void timerCallback() override
    {
        const float l = proc.peakL.exchange (0.0f), r = proc.peakR.exchange (0.0f);
        lvL = std::max (l, lvL * 0.86f);
        lvR = std::max (r, lvR * 0.86f);
        if (std::abs (lvL - shownL) > 0.002f || std::abs (lvR - shownR) > 0.002f) { shownL = lvL; shownR = lvR; repaint(); }
    }

    static float toPos (float lin)
    {
        const float db = 20.0f * std::log10 (std::max (lin, 1.0e-4f));
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (b, 3.0f);
        const float w = (b.getWidth() - 3.0f) * 0.5f;
        const float vals[2] = { toPos (shownL), toPos (shownR) };
        for (int i = 0; i < 2; ++i)
        {
            auto bar = juce::Rectangle<float> (b.getX() + 1.0f + (float) i * (w + 1.0f), b.getY() + 1.0f, w, b.getHeight() - 2.0f);
            const float h = bar.getHeight() * vals[i];
            juce::ColourGradient grad (juce::Colour (0xffff5252), 0.0f, bar.getY(), juce::Colour (0xff2fd27a), 0.0f, bar.getBottom(), false);
            grad.addColour (0.25, juce::Colour (0xfff2c94c));
            g.setGradientFill (grad);
            g.fillRect (bar.withTop (bar.getBottom() - h));
        }
    }

    NuSynthAudioProcessor& proc;
    float lvL = 0.0f, lvR = 0.0f, shownL = 0.0f, shownR = 0.0f;
};

/** Top right: [UNDO][REDO]  [Tune]  [Master]  [meter] */
struct HeaderRight final : juce::Component, private juce::Timer
{
    explicit HeaderRight (NuSynthAudioProcessor& p)
        : proc (p), tune (p.apvts, ids::masterTune, "Tune A4"), master (p.apvts, ids::masterGainDb, "Master"), meter (p)
    {
        undoBtn.setButtonText ("UNDO"); redoBtn.setButtonText ("REDO");
        undoBtn.setTooltip ("Undo the last change"); redoBtn.setTooltip ("Redo");
        undoBtn.onClick = [this] { proc.getUndo().undo(); };
        redoBtn.onClick = [this] { proc.getUndo().redo(); };
        for (juce::Component* c : std::initializer_list<juce::Component*> { &undoBtn, &redoBtn, &tune, &master, &meter })
            addAndMakeVisible (c);
        startTimerHz (8);
    }
    ~HeaderRight() override { stopTimer(); }

    void timerCallback() override
    {
        undoBtn.setEnabled (proc.getUndo().canUndo());
        redoBtn.setEnabled (proc.getUndo().canRedo());
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto btns = r.removeFromLeft (84).reduced (0, 12);
        undoBtn.setBounds (btns.removeFromLeft (40));
        redoBtn.setBounds (btns.removeFromRight (40));
        r.removeFromLeft (6);
        tune.setBounds (r.removeFromLeft (60));
        master.setBounds (r.removeFromLeft (60));
        r.removeFromLeft (4);
        meter.setBounds (r.removeFromLeft (22).reduced (0, 6));
    }

    NuSynthAudioProcessor& proc;
    juce::TextButton undoBtn, redoBtn;
    ParamKnob tune, master;
    LevelMeter meter;
};
}
