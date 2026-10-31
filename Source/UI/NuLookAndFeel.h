#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace nusynth
{
class NuLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NuLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::ComboBox::backgroundColourId, panelDark);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, panelDark);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, textDim);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::backgroundColourId, panelDark);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::TextEditor::backgroundColourId, panelDark);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, outline);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::BubbleComponent::backgroundColourId, panel);
        setColour (juce::BubbleComponent::outlineColourId, accent);
        setColour (juce::TooltipWindow::textColourId, text);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.6f)));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        using namespace colours;
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float lw = juce::jmax (2.0f, r * 0.16f);
        const float arcR = r - lw * 0.5f;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (outline);
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ang = startAngle + pos * (endAngle - startAngle);
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, ang, true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float br = r - lw * 1.7f;
        g.setColour (panelDark.brighter (0.15f));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (outline.brighter (0.2f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

        juce::Path p;
        p.addRoundedRectangle (-1.3f, -br + 1.0f, 2.6f, br * 0.55f, 1.0f);
        g.setColour (text);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};
}
