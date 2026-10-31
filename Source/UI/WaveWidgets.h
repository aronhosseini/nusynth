#pragma once
#include "Widgets.h"
#include "Parameters/ParameterLayout.h"
#include "DSP/Wavetables.h"

namespace nusynth::ui
{
/** Implemented by the editor: opens the Serum-style harmonic editor overlay for an oscillator. */
struct HarmonicHost
{
    virtual ~HarmonicHost() = default;
    virtual void openHarmonicEditor (int oscIndex) = 0;
};

/** Small pencil button (top right of the oscillator wave view). */
struct PencilButton final : juce::Button
{
    PencilButton() : juce::Button ("edit") { setTooltip ("Edit harmonics (harmonic editor)"); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto box = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (down ? colours::accent.darker (0.2f) : (over ? colours::accent.brighter (0.15f) : colours::accent));
        g.fillRoundedRectangle (box, 4.0f);
        auto b = box.reduced (4.0f);
        g.setColour (juce::Colours::white);
        juce::Path p;
        p.startNewSubPath (b.getX(), b.getBottom());
        p.lineTo (b.getX() + b.getWidth() * 0.18f, b.getBottom() - b.getHeight() * 0.38f);
        p.lineTo (b.getRight() - b.getWidth() * 0.18f, b.getY() + b.getHeight() * 0.05f);
        p.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.22f);
        p.lineTo (b.getX() + b.getWidth() * 0.38f, b.getBottom() - b.getHeight() * 0.18f);
        p.closeSubPath();
        g.fillPath (p);
    }
};

/** Generic picker for a choice parameter: click = menu, arrows = previous / next (used by the noise type). */
struct ChoiceSelector final : juce::Component
{
    ChoiceSelector (APVTS& a, const juce::String& id)
    {
        auto* par = P (a, id);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (par)) items = c->choices;
        attach = std::make_unique<juce::ParameterAttachment> (*par, [this] (float v) { idx = (int) std::lround (v); repaint(); }, nullptr);
        attach->sendInitialUpdate();
    }
    void setIndex (int i) { const int n = std::max (1, items.size()); attach->setValueAsCompleteGesture ((float) ((i % n + n) % n)); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (colours::accent);
        g.setFont (12.0f);
        g.drawText ("<", getLocalBounds().removeFromLeft (18), juce::Justification::centred);
        g.drawText (">", getLocalBounds().removeFromRight (18), juce::Justification::centred);
        g.setColour (colours::text);
        g.drawText (items.size() > 0 ? items[juce::jlimit (0, items.size() - 1, idx)] : juce::String(), getLocalBounds().reduced (22, 0), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 18) { setIndex (idx - 1); return; }
        if (e.x > getWidth() - 18) { setIndex (idx + 1); return; }
        juce::PopupMenu m;
        for (int i = 0; i < items.size(); ++i) m.addItem (i + 1, items[i], true, i == idx);
        juce::Component::SafePointer<ChoiceSelector> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) { if (safe != nullptr && r > 0) safe->setIndex (r - 1); });
    }
    juce::StringArray items;
    int idx = 0;
    std::unique_ptr<juce::ParameterAttachment> attach;
};

/** Wave picker grouped like Serum: ANALOG / DIGITAL / VOWEL / USER. Click = menu, arrows = previous / next. */
struct WaveSelector final : juce::Component
{
    WaveSelector (APVTS& a, int o) : names (wavetableNames())
    {
        auto* par = P (a, ids::osc (o, "wavetable"));
        attach = std::make_unique<juce::ParameterAttachment> (*par, [this] (float v) { idx = (int) std::lround (v); repaint(); }, nullptr);
        attach->sendInitialUpdate();
    }

    void setIndex (int i)
    {
        const int n = names.size();
        attach->setValueAsCompleteGesture ((float) ((i % n + n) % n));
    }

    void paint (juce::Graphics& g) override
    {
        static const char* catNames[4] = { "ANALOG", "DIGITAL", "VOWEL", "USER" };
        auto b = getLocalBounds().toFloat();
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (colours::accent);
        g.setFont (12.0f);
        g.drawText ("<", getLocalBounds().removeFromLeft (18), juce::Justification::centred);
        g.drawText (">", getLocalBounds().removeFromRight (18), juce::Justification::centred);
        const int cat = wavetableCategory (idx);
        g.setColour (colours::textDim);
        g.setFont (9.5f);
        g.drawText (catNames[cat], getLocalBounds().reduced (22, 0).removeFromLeft (46), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (12.0f);
        g.drawText (names[juce::jlimit (0, names.size() - 1, idx)], getLocalBounds().reduced (66, 0), juce::Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 18) { setIndex (idx - 1); return; }
        if (e.x > getWidth() - 18) { setIndex (idx + 1); return; }

        static const char* catNames[4] = { "Analog", "Digital", "Vowel", "User" };
        juce::PopupMenu subs[4];
        for (int i = 0; i < names.size(); ++i)
            subs[wavetableCategory (i)].addItem (i + 1, names[i], true, i == idx);
        juce::PopupMenu root;
        for (int c = 0; c < 4; ++c) root.addSubMenu (catNames[c], subs[c]);
        juce::Component::SafePointer<WaveSelector> safe (this);
        root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe != nullptr && r > 0) safe->setIndex (r - 1);
        });
    }

    juce::StringArray names;
    int idx = 0;
    std::unique_ptr<juce::ParameterAttachment> attach;
};
}
