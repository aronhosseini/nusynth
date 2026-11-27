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
        setColour (juce::TextButton::buttonOnColourId, accentDim);
        setColour (juce::TextButton::textColourOffId, text.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::backgroundColourId, panelDark);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::TextEditor::backgroundColourId, panelDark);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, outline);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, accentDim);
        setColour (juce::BubbleComponent::backgroundColourId, panel);
        setColour (juce::BubbleComponent::outlineColourId, accent);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::ScrollBar::thumbColourId, border);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, border);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.6f)));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }

    // ---- buttons: dark rounded pills, active = teal fill + neon cyan edge
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float corner = juce::jmin (6.0f, r.getHeight() * 0.3f);
        auto base = bg;
        if (! b.isEnabled()) base = base.withMultipliedAlpha (0.5f);
        else if (down)       base = base.brighter (0.12f);
        else if (over)       base = base.brighter (0.08f);

        if (b.getProperties().contains ("generate"))                               // AI "GENERATE": cyan -> pink gradient
        {
            auto c1 = accent, c2 = hotPink;
            if (! b.isEnabled()) { c1 = c1.withAlpha (0.4f); c2 = c2.withAlpha (0.4f); }
            else if (down)       { c1 = c1.darker (0.15f);  c2 = c2.darker (0.15f); }
            else if (over)       { c1 = c1.brighter (0.1f); c2 = c2.brighter (0.1f); }
            g.setGradientFill (juce::ColourGradient (c1, r.getX(), r.getY(), c2, r.getRight(), r.getY(), false));
            g.fillRoundedRectangle (r, corner);
            return;
        }

        g.setGradientFill (juce::ColourGradient (base.brighter (0.10f), r.getX(), r.getY(), base.darker (0.15f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);

        if (on && b.isEnabled()) gfx::neonOutline (g, r, accent, corner, 0.9f);
        else if (b.getProperties().contains ("tab"))                                // FX / OTHER tabs: pink-violet edge
        {
            g.setColour (magenta.withAlpha (over ? 0.95f : 0.65f));
            g.drawRoundedRectangle (r, corner, 1.2f);
        }
        else
        {
            g.setColour ((over ? border.brighter (0.2f) : juce::Colour (0xff3b4a5d)).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    // ---- combo boxes: dark field + cyan chevron
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace colours;
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
        gfx::insetField (g, r, 5.0f);
        if (box.hasKeyboardFocus (false)) gfx::neonOutline (g, r.reduced (0.5f), accent, 5.0f, 0.5f);
        gfx::chevron (g, juce::Rectangle<float> ((float) w - 22.0f, 0.0f, 20.0f, (float) h), box.isEnabled() ? accent : textDim);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
        g.setColour (colours::panel);
        g.fillRect (r);
        g.setColour (colours::accent.withAlpha (0.45f));
        g.drawRect (r, 1.0f);
    }

    // ---- rotary knob: brushed-metal cap in a dark bezel, neon value arc (cyan, or pink-violet for modulation) + white pointer
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace colours;
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float lw = juce::jmax (2.6f, r * 0.17f);
        const float arcR = r - lw * 0.5f;
        const bool pink = (bool) s.getProperties().getWithDefault ("pink", false);
        const juce::Colour ring = pink ? magenta : accent;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff141b25));
        g.strokePath (track, juce::PathStrokeType (lw + 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff202b38));
        g.strokePath (track, juce::PathStrokeType (lw - 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ang = startAngle + pos * (endAngle - startAngle);
        if (pos > 0.005f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, ang, true);
            gfx::glowStroke (g, val, s.isEnabled() ? ring : ring.withAlpha (0.35f), lw);
        }

        const float br = r - lw * 1.55f;                                                    // radius of the metal cap
        g.setColour (juce::Colours::black.withAlpha (0.55f));                               // drop shadow
        g.fillEllipse (c.x - br - 1.0f, c.y - br + 1.4f, br * 2.0f + 2.0f, br * 2.0f + 2.0f);
        const float bz = br + 2.2f;                                                         // dark bezel ring around the cap
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff05080c), c.x, c.y - bz, juce::Colour (0xff1c2530), c.x, c.y + bz, false));
        g.fillEllipse (c.x - bz, c.y - bz, bz * 2.0f, bz * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (c.x - bz + 0.5f, c.y - bz + 0.5f, bz * 2.0f - 1.0f, bz * 2.0f - 1.0f, 0.8f);

        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int px = juce::jlimit (8, 384, (int) std::ceil (br * 2.0f * juce::jmax (1.0f, scale)));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (s.isEnabled() ? 1.0f : 0.55f);
        g.drawImage (gfx::metalCapImage (px), juce::Rectangle<float> (c.x - br, c.y - br, br * 2.0f, br * 2.0f));
        g.setOpacity (1.0f);

        const auto rot = juce::AffineTransform::rotation (ang).translated (c.x, c.y);       // pointer: short bright bar near the rim
        juce::Path p;
        p.addRoundedRectangle (-1.5f, -br + 1.2f, 3.0f, br * 0.58f, 1.4f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (p, rot.translated (0.6f, 0.9f));
        g.setColour (juce::Colours::white);
        g.fillPath (p, rot);
    }
};
}
