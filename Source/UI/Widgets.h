#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <initializer_list>
#include <algorithm>
#include "Theme.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ModAssign.h"
#include "DSP/ModDestinations.h"
#include "DSP/SharedData.h"
#include "Parameters/ParameterLayout.h"
#include <functional>
#include "Parameters/ParameterLayout.h"
#include "Parameters/ModAssign.h"
#include "DSP/ModDestinations.h"

namespace nusynth::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

inline juce::RangedAudioParameter* P (APVTS& a, const juce::String& id)
{
    auto* p = a.getParameter (id);
    jassert (p != nullptr);
    return p;
}

/** Splits `area` into equal columns, one per component. */
inline void row (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps)
{
    if (comps.size() == 0) return;
    const int n = (int) comps.size();
    const int w = area.getWidth() / n;
    int i = 0;
    for (auto* c : comps)
        c->setBounds (area.getX() + i++ * w, area.getY(), w, area.getHeight());
}

inline juce::Rectangle<int> centred (juce::Rectangle<int> cell, int w, int h)
{
    return juce::Rectangle<int> (w, h).withCentre (cell.getCentre());
}

// ------------------------------------------------------------------ draggable modulation source ("LFO 1", "ENV 2"...)
struct ModSourceHandle : juce::Component, juce::SettableTooltipClient
{
    ModSourceHandle (int sourceIndex, juce::String text) : src (sourceIndex), label (std::move (text))
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag onto any knob to modulate it");
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colours::orange);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (juce::Colours::black);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (label, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 4) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive())
                c->startDragging ("modsrc:" + juce::String (src), this);
    }

    int src;
    juce::String label;
};

// ------------------------------------------------------------------ rotary knob + label (accepts modulation drops)
/** Modulated value of a destination (what the newest voice really uses), or the plain value if nothing is published. */
inline float liveValue (APVTS& a, int dest, float fallback)
{
    if (auto* sd = sharedFor (&a))
    {
        const float v = sd->live.v[(size_t) dest].load (std::memory_order_relaxed);
        if (v > -900.0f) return v;
    }
    return fallback;
}

struct KnobSlider : juce::Slider
{
    std::function<bool (const juce::MouseEvent&)> onPopup, onModDown;
    std::function<void (const juce::MouseEvent&)> onModDrag;
    bool modDragging = false;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onPopup && onPopup (e)) return;
        if (onModDown && onModDown (e)) { modDragging = true; return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (modDragging) { if (onModDrag) onModDrag (e); return; }
        juce::Slider::mouseDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (modDragging) { modDragging = false; return; }
        juce::Slider::mouseUp (e);
    }
};

struct ParamKnob : juce::Component, juce::DragAndDropTarget, private juce::Timer
{
    ParamKnob (APVTS& a, const juce::String& id, const juce::String& text)
        : apvts (a), paramId (id), destIndex (destForParam (id))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.onPopup = [this] (const juce::MouseEvent&) { return showModMenu(); };
        slider.onModDown = [this] (const juce::MouseEvent& e) { return startModEdit (e); };
        slider.onModDrag = [this] (const juce::MouseEvent& e) { dragModEdit (e); };
        addAndMakeVisible (slider);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, colours::textDim);
        label.setFont (juce::FontOptions (11.0f));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attach = std::make_unique<juce::SliderParameterAttachment> (*P (a, id), slider, nullptr);
        if (destIndex >= 0) startTimerHz (20);
    }
    ~ParamKnob() override { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (14));
        slider.setBounds (r);
    }

    struct Mod { int slot; int src; float amount; bool bipolar; };

    std::vector<Mod> activeMods() const
    {
        std::vector<Mod> m;
        if (destIndex < 0) return m;
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const int s = (int) getParamReal (apvts, ids::mod (i, "src"));
            const int d = (int) getParamReal (apvts, ids::mod (i, "dst"));
            const float amt = getParamReal (apvts, ids::mod (i, "amount"));
            if (s > 0 && d == destIndex + 1 && amt != 0.0f)
                m.push_back ({ i, s, amt, ((int) getParamReal (apvts, ids::mod (i, "type"))) == 1 });
        }
        return m;
    }

    void timerCallback() override
    {
        float sig = 0.0f;
        const auto mods = activeMods();
        for (const auto& m : mods) sig += (float) (m.slot + 1) * 7.0f + m.amount * 13.0f + (float) m.src;
        if (! mods.empty()) sig += liveNow() * 1000.0f;
        if (sig != lastSig) { lastSig = sig; repaint(); }
    }

    float liveNow() const
    {
        auto* par = P (apvts, paramId);
        return par->convertTo0to1 (liveValue (apvts, destIndex, par->convertFrom0to1 (par->getValue())));
    }

    // ---- Serum 2 style: left-click the outer ring (or Alt / Cmd) and move AROUND the knob to set the amount of the nearest modulator
    float ringPos (juce::Point<float> p) const
    {
        const auto c = slider.getBounds().toFloat().getCentre();
        float th = std::atan2 (p.x - c.x, -(p.y - c.y));
        if (th < 0.0f) th += juce::MathConstants<float>::twoPi;
        const float a0 = juce::MathConstants<float>::pi * 1.25f, span = juce::MathConstants<float>::pi * 1.5f;
        if (th >= a0) return juce::jlimit (0.0f, 1.0f, (th - a0) / span);
        if (th <= juce::MathConstants<float>::pi * 0.75f) return juce::jlimit (0.0f, 1.0f, (th + juce::MathConstants<float>::twoPi - a0) / span);
        return (th - juce::MathConstants<float>::pi * 0.75f) < (a0 - th) ? 1.0f : 0.0f;       // bottom gap -> nearest end
    }

    bool startModEdit (const juce::MouseEvent& e)
    {
        const auto mods = activeMods();
        if (mods.empty()) return false;
        const auto b = slider.getLocalBounds().toFloat();
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const bool outer = e.position.getDistanceFrom (b.getCentre()) > r * 0.72f;
        const bool alt = e.mods.isAltDown() || e.mods.isCommandDown();
        if (! (outer || alt)) return false;

        const float base = (float) P (apvts, paramId)->getValue();
        const float t = ringPos (e.getPosition().toFloat() + slider.getPosition().toFloat());
        float bestD = 10.0f;
        modEditSlot = mods.back().slot;
        for (const auto& m : mods)
        {
            const float ends[2] = { m.bipolar ? base + std::abs (m.amount) : base + m.amount, m.bipolar ? base - std::abs (m.amount) : base + m.amount };
            for (float en : ends)
            {
                const float d = std::abs (t - juce::jlimit (0.0f, 1.0f, en));
                if (d < bestD) { bestD = d; modEditSlot = m.slot; }
            }
        }
        for (const auto& m : mods)
            if (m.slot == modEditSlot) { modEditBipolar = m.bipolar; modEditStart = m.amount; }
        modEditBase = base;
        modEditVertical = alt && ! outer;
        modEditY = e.position.y;
        return true;
    }

    void dragModEdit (const juce::MouseEvent& e)
    {
        if (modEditSlot < 0) return;
        float amt;
        if (modEditVertical) amt = modEditStart + (modEditY - e.position.y) * 0.01f;
        else
        {
            const float t = ringPos (e.getPosition().toFloat() + slider.getPosition().toFloat());
            amt = modEditBipolar ? (modEditStart < 0.0f ? -1.0f : 1.0f) * std::abs (t - modEditBase) : (t - modEditBase);
        }
        setParamReal (apvts, ids::mod (modEditSlot, "amount"), juce::jlimit (-1.0f, 1.0f, amt));
        repaint();
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        const auto mods = activeMods();
        if (! mods.empty())
        {
            auto b = slider.getBounds().toFloat().reduced (2.0f);
            const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
            const auto c = b.getCentre();
            const float lw = juce::jmax (2.0f, r * 0.16f), arcR = r - lw * 0.5f;
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
            const float base = (float) P (apvts, paramId)->getValue();
            g.setColour (colours::orange);
            for (const auto& m : mods)
            {
                const float lo = juce::jlimit (0.0f, 1.0f, m.bipolar ? base - std::abs (m.amount) : juce::jmin (base, base + m.amount));
                const float hi = juce::jlimit (0.0f, 1.0f, m.bipolar ? base + std::abs (m.amount) : juce::jmax (base, base + m.amount));
                juce::Path p;
                p.addCentredArc (c.x, c.y, arcR - lw * 0.95f, arcR - lw * 0.95f, 0.0f, a0 + lo * (a1 - a0), a0 + hi * (a1 - a0), true);
                g.strokePath (p, juce::PathStrokeType (2.2f));
            }
            const float live = juce::jlimit (0.0f, 1.0f, liveNow());
            const float ang = a0 + live * (a1 - a0), rr = arcR - lw * 0.95f;
            g.setColour (juce::Colours::white);
            g.fillEllipse (c.x + std::sin (ang) * rr - 2.5f, c.y - std::cos (ang) * rr - 2.5f, 5.0f, 5.0f);
        }
        if (hover)
        {
            g.setColour (colours::orange);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
        }
    }

    // ---- right click: edit / remove modulation
    bool showModMenu()
    {
        const auto mods = activeMods();
        if (mods.empty()) return false;
        const auto srcNames = modSourceNames();
        static const float amounts[7] = { 1.0f, 0.75f, 0.5f, 0.25f, -0.25f, -0.5f, -1.0f };
        juce::PopupMenu menu;
        for (const auto& m : mods)
        {
            juce::PopupMenu sub;
            for (int k = 0; k < 7; ++k)
                sub.addItem (1000 + m.slot * 20 + k, "Amount " + juce::String (amounts[k], 2));
            sub.addSeparator();
            juce::PopupMenu srcMenu;
            for (int q = 1; q < srcNames.size(); ++q) srcMenu.addItem (2000 + m.slot * 30 + q, srcNames[q], true, q == m.src);
            sub.addSubMenu ("Change source", srcMenu);
            sub.addItem (1000 + m.slot * 20 + 19, "Remove");
            menu.addSubMenu (srcNames[juce::jlimit (0, srcNames.size() - 1, m.src)] + "  (" + juce::String (m.amount, 2) + ")", sub);
        }
        menu.addSeparator();
        menu.addItem (9999, "Remove all modulations");
        juce::Component::SafePointer<ParamKnob> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe] (int result)
        {
            if (safe == nullptr || result < 1000) return;
            if (result >= 2000 && result < 9999)
            {
                setParamReal (safe->apvts, ids::mod ((result - 2000) / 30, "src"), (float) ((result - 2000) % 30));
                safe->repaint();
                return;
            }
            if (result == 9999) { for (const auto& m : safe->activeMods()) clearModSlot (safe->apvts, m.slot); safe->repaint(); return; }
            const int slot = (result - 1000) / 20, code = (result - 1000) % 20;
            static const float amt[7] = { 1.0f, 0.75f, 0.5f, 0.25f, -0.25f, -0.5f, -1.0f };
            if (code == 19) clearModSlot (safe->apvts, slot);
            else if (code < 7) setParamReal (safe->apvts, ids::mod (slot, "amount"), amt[code]);
            safe->repaint();
        });
        return true;
    }

    // ---- DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return destIndex >= 0 && d.description.toString().startsWith ("modsrc:");
    }
    void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { hover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        hover = false;
        const int src = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
        assignModulation (apvts, src, destIndex);
        repaint();
    }

    APVTS& apvts;
    juce::String paramId;
    int destIndex;
    bool hover = false;
    float lastSig = -1.0e9f;
    int modEditSlot = -1;
    float modEditStart = 0.0f, modEditY = 0.0f, modEditBase = 0.0f;
    bool modEditBipolar = true, modEditVertical = false;
    KnobSlider slider;
    juce::Label label;
    std::unique_ptr<juce::SliderParameterAttachment> attach;
};

// ------------------------------------------------------------------ combo
struct ParamCombo : juce::ComboBox
{
    ParamCombo (APVTS& a, const juce::String& id)
    {
        auto* p = P (a, id);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
            addItemList (c->choices, 1);
        attach = std::make_unique<juce::ComboBoxParameterAttachment> (*p, *this, nullptr);
    }
    std::unique_ptr<juce::ComboBoxParameterAttachment> attach;
};

// ------------------------------------------------------------------ toggle button
struct ParamToggle : juce::TextButton
{
    ParamToggle (APVTS& a, const juce::String& id, const juce::String& text)
    {
        setButtonText (text);
        setClickingTogglesState (true);
        attach = std::make_unique<juce::ButtonParameterAttachment> (*P (a, id), *this, nullptr);
    }
    std::unique_ptr<juce::ButtonParameterAttachment> attach;
};

// ------------------------------------------------------------------ list of choices (vertical or horizontal)
struct ChoiceList : juce::Component
{
    ChoiceList (APVTS& a, const juce::String& id, bool horiz = false) : horizontal (horiz)
    {
        auto* p = P (a, id);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) items = c->choices;
        attach = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
        {
            selected = (int) std::round (v);
            repaint();
        }, nullptr);
        attach->sendInitialUpdate();
    }

    juce::Rectangle<int> cell (int i) const
    {
        const int n = juce::jmax (1, items.size());
        return horizontal ? juce::Rectangle<int> (i * getWidth() / n, 0, getWidth() / n, getHeight())
                          : juce::Rectangle<int> (0, i * getHeight() / n, getWidth(), getHeight() / n);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setFont (fontSize);
        for (int i = 0; i < items.size(); ++i)
        {
            auto r = cell (i);
            const bool sel = (i == selected);
            if (sel)
            {
                g.setColour (colours::accent);
                g.fillRoundedRectangle (r.reduced (2).toFloat(), 3.0f);
            }
            g.setColour (sel ? juce::Colours::black : colours::textDim);
            g.drawText (items[i], r.reduced (8, 0), horizontal ? juce::Justification::centred
                                                               : juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < items.size(); ++i)
            if (cell (i).contains (e.getPosition()))
                attach->setValueAsCompleteGesture ((float) i);
    }

    bool horizontal;
    float fontSize = 12.0f;
    juce::StringArray items;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attach;
};

// ------------------------------------------------------------------ power (on / off) button
struct PowerButton final : juce::Button
{
    PowerButton (APVTS& a, const juce::String& id) : juce::Button ("power")
    {
        setClickingTogglesState (true);
        setTooltip ("On / off");
        attach = std::make_unique<juce::ButtonParameterAttachment> (*P (a, id), *this, nullptr);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const bool on = getToggleState();
        const juce::Colour c = on ? colours::accent : colours::textDim.withAlpha (over ? 0.9f : 0.6f);
        g.setColour (on ? colours::accent.withAlpha (0.18f) : colours::panelDark);
        g.fillEllipse (b);
        g.setColour (c);
        const float r = b.getWidth() * 0.28f;
        juce::Path arc;
        arc.addCentredArc (b.getCentreX(), b.getCentreY() + 0.5f, r, r, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
        g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (b.getCentreX(), b.getCentreY() - r * 1.1f, b.getCentreX(), b.getCentreY(), 1.6f);
    }

    std::unique_ptr<juce::ButtonParameterAttachment> attach;
};

// ------------------------------------------------------------------ panel
struct Panel : juce::Component
{
    explicit Panel (juce::String t) : title (std::move (t)) {}

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (title, 10, 6, getWidth() - 20, 16, juce::Justification::centredLeft);
        paintExtra (g);
    }

    virtual void paintExtra (juce::Graphics&) {}
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (8).withTrimmedTop (20); }

    /** Section on / off button next to the title. */
    void addPower (APVTS& a, const juce::String& id)
    {
        power = std::make_unique<PowerButton> (a, id);
        addAndMakeVisible (*power);
    }
    void placeHeader()
    {
        if (power != nullptr) power->setBounds (10 + title.length() * 9 + 6, 5, 18, 18);
    }

    juce::String title;
    std::unique_ptr<PowerButton> power;
};

/** Panel with a row of small selector buttons that switches between stacked pages.
    Optionally shows a draggable yellow modulation-source key (e.g. "LFO 2") that follows the selected page. */
struct MultiPanel : Panel
{
    MultiPanel (juce::String t, const juce::StringArray& labels, int firstSrc = 0, const juce::String& handlePrefix = {})
        : Panel (std::move (t)), firstSource (firstSrc), prefix (handlePrefix)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto b = std::make_unique<juce::TextButton> (labels[i]);
            b->setClickingTogglesState (false);          // we manage the on/off state ourselves (only ONE is ever lit)
            b->onClick = [this, i] { select (i); };
            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
        }
        if (firstSrc > 0)
        {
            handle = std::make_unique<ModSourceHandle> (firstSrc, prefix + " 1");
            addAndMakeVisible (*handle);
        }
    }

    void addPage (std::unique_ptr<juce::Component> p) { addChildComponent (*p); pages.push_back (std::move (p)); }

    void select (int i)
    {
        for (size_t j = 0; j < pages.size(); ++j) pages[j]->setVisible ((int) j == i);
        for (size_t j = 0; j < buttons.size(); ++j) buttons[j]->setToggleState ((int) j == i, juce::dontSendNotification);
        if (handle != nullptr)
        {
            handle->src = firstSource + i;
            handle->label = prefix + " " + juce::String (i + 1);
            handle->repaint();
        }
    }

    virtual juce::Rectangle<int> pageBounds() const { return content(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 5);
        auto h = r.removeFromTop (18);
        for (int i = (int) buttons.size() - 1; i >= 0; --i)
            buttons[(size_t) i]->setBounds (h.removeFromRight (buttons.size() > 4 ? 25 : 30).reduced (1, 0));
        if (handle != nullptr) handle->setBounds (h.removeFromRight (74).reduced (2, 0));
        for (auto& p : pages) p->setBounds (pageBounds());
    }

    int firstSource;
    juce::String prefix;
    std::unique_ptr<ModSourceHandle> handle;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<std::unique_ptr<juce::Component>> pages;
};

/** Places components row by row in a cols x rows grid. */
inline void grid (juce::Rectangle<int> area, int cols, int rows, std::initializer_list<juce::Component*> comps)
{
    const int w = area.getWidth() / std::max (1, cols), h = area.getHeight() / std::max (1, rows);
    int i = 0;
    for (auto* c : comps)
    {
        c->setBounds (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h);
        ++i;
    }
}
}
