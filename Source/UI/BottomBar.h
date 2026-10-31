#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"

namespace nusynth::ui
{
// ------------------------------------------------------------------ 8 macros (header, right side of the menu)
struct MacroCell final : juce::Component
{
    MacroCell (APVTS& a, int index)
        : pill (MS_Macro1 + index, "M" + juce::String (index + 1))
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        knob.setPopupDisplayEnabled (true, true, nullptr);
        att = std::make_unique<juce::SliderParameterAttachment> (*P (a, ids::macro (index)), knob, nullptr);
        pill.setTooltip ("Macro " + juce::String (index + 1) + " - drag onto any knob");
        addAndMakeVisible (knob);
        addAndMakeVisible (pill);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        pill.setBounds (r.removeFromBottom (15).reduced (3, 0));
        knob.setBounds (r);
    }
    juce::Slider knob;
    ModSourceHandle pill;
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

struct MacroBar final : juce::Component
{
    explicit MacroBar (APVTS& a)
    {
        for (int i = 0; i < kNumMacros; ++i)
        {
            cells[(size_t) i] = std::make_unique<MacroCell> (a, i);
            addAndMakeVisible (*cells[(size_t) i]);
        }
    }
    void resized() override
    {
        const int w = getWidth() / kNumMacros;
        for (int i = 0; i < kNumMacros; ++i) cells[(size_t) i]->setBounds (i * w, 0, w, getHeight());
    }
    std::array<std::unique_ptr<MacroCell>, kNumMacros> cells;
};

// ------------------------------------------------------------------ KEY / VEL tracker with drag keys (click / drag the VEL bar to set the keyboard velocity)
struct KeyVelPanel final : juce::Component, private juce::Timer
{
    explicit KeyVelPanel (NuSynthAudioProcessor& p) : proc (p), keyPill (MS_Key, "KEY"), velPill (MS_Velocity, "VEL")
    {
        addAndMakeVisible (keyPill); addAndMakeVisible (velPill);
        velPill.setTooltip ("Drag onto any knob. Click / drag the bar to set the on-screen keyboard velocity");
        startTimerHz (30);
    }
    ~KeyVelPanel() override { stopTimer(); }

    void timerCallback() override
    {
        const int n = proc.lastNote.load();
        const float v = proc.lastVelocity.load();
        if (n != note || v != lastVel) { note = n; lastVel = v; repaint(); }
    }

    juce::Rectangle<int> velBar() const
    {
        auto r = getLocalBounds().reduced (8, 8);
        r.removeFromTop (30);
        auto row = r.removeFromTop (22);
        row.removeFromLeft (66);
        return row.removeFromLeft (row.getWidth() - 36).reduced (0, 5);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 8);
        keyPill.setBounds (r.removeFromTop (22).removeFromLeft (60));
        r.removeFromTop (8);
        velPill.setBounds (r.removeFromTop (22).removeFromLeft (60));
    }

    void setVelFromMouse (const juce::MouseEvent& e)
    {
        const auto b = velBar();
        if (e.y < b.getY() - 6 || e.y > b.getBottom() + 6 || b.getWidth() <= 0) return;
        const float v = juce::jlimit (0.01f, 1.0f, (float) (e.x - b.getX()) / (float) b.getWidth());
        proc.uiVelocity.store (v);
        if (onVelocity) onVelocity (v);
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override { setVelFromMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override { setVelFromMouse (e); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        auto r = getLocalBounds().reduced (8, 8);
        auto keyRow = r.removeFromTop (22);
        r.removeFromTop (8);
        auto velRow = r.removeFromTop (22);
        keyRow.removeFromLeft (66); velRow.removeFromLeft (66);

        auto kb = keyRow.removeFromLeft (keyRow.getWidth() - 36).toFloat().reduced (0, 5);
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (kb, 3.0f);
        g.setColour (colours::outline);
        for (int o = 1; o < 11; ++o) g.drawVerticalLine ((int) (kb.getX() + kb.getWidth() * (float) o * 12.0f / 128.0f), kb.getY(), kb.getBottom());
        g.setColour (colours::orange);
        g.fillRoundedRectangle (kb.getX() + kb.getWidth() * (float) note / 127.0f - 2.0f, kb.getY() - 2.0f, 4.0f, kb.getHeight() + 4.0f, 2.0f);
        g.setColour (colours::text);
        g.setFont (11.0f);
        g.drawText (juce::String (names[note % 12]) + juce::String (note / 12 - 1), keyRow, juce::Justification::centred);

        auto vb = velRow.removeFromLeft (velRow.getWidth() - 36).toFloat().reduced (0, 5);
        const float uv = proc.uiVelocity.load();
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (vb, 3.0f);
        g.setColour (colours::orange);
        g.fillRoundedRectangle (vb.withWidth (vb.getWidth() * uv), 3.0f);
        g.setColour (juce::Colours::white);
        g.fillRect (vb.getX() + vb.getWidth() * lastVel - 1.0f, vb.getY() - 2.0f, 2.0f, vb.getHeight() + 4.0f);       // last played velocity
        g.setColour (colours::text);
        g.drawText (juce::String ((int) std::round (uv * 127.0f)), velRow, juce::Justification::centred);
    }

    NuSynthAudioProcessor& proc;
    ModSourceHandle keyPill, velPill;
    std::function<void (float)> onVelocity;
    int note = 60;
    float lastVel = 0.0f;
};

// ------------------------------------------------------------------ Yamaha style wheel (ridged cylinder)
struct WheelControl final : juce::Component
{
    WheelControl (bool isBipolar, std::function<void (float)> cb) : bipolar (isBipolar), onChange (std::move (cb)) {}

    void setValue (float v, bool notify)
    {
        v = bipolar ? juce::jlimit (-1.0f, 1.0f, v) : juce::jlimit (0.0f, 1.0f, v);
        if (v == value) return;
        value = v;
        if (notify && onChange) onChange (value);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
        // cylinder body: dark at the sides, lighter in the middle
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0f13), b.getX(), 0.0f, juce::Colour (0xff0b0f13), b.getRight(), 0.0f, false));
        g.fillRoundedRectangle (b, 4.0f);
        juce::ColourGradient body (juce::Colour (0xff1b232b), b.getX(), 0.0f, juce::Colour (0xff1b232b), b.getRight(), 0.0f, false);
        body.addColour (0.5, juce::Colour (0xff5c6b78));
        g.setGradientFill (body);
        g.fillRoundedRectangle (b.reduced (1.0f), 4.0f);

        // ridges scroll with the value (illusion of a rotating wheel)
        const float spacing = 9.0f, travel = b.getHeight() * 0.9f;
        const float offset = std::fmod ((bipolar ? value * 0.5f : value) * travel + 1000.0f * spacing, spacing);
        g.saveState();
        g.reduceClipRegion (b.reduced (1.0f).toNearestInt());
        for (float y = b.getY() - spacing + offset; y < b.getBottom() + spacing; y += spacing)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawHorizontalLine ((int) y, b.getX() + 1.0f, b.getRight() - 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.drawHorizontalLine ((int) y + 1, b.getX() + 1.0f, b.getRight() - 1.0f);
        }
        // top / bottom shading (cylinder roundness)
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.65f), 0.0f, b.getY(), juce::Colours::transparentBlack, 0.0f, b.getY() + b.getHeight() * 0.28f, false));
        g.fillRect (b);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.65f), 0.0f, b.getBottom(), juce::Colours::transparentBlack, 0.0f, b.getBottom() - b.getHeight() * 0.28f, false));
        g.fillRect (b);
        g.restoreState();

        g.setColour (colours::outline.brighter (0.3f));
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
        if (bipolar)
        {
            g.setColour (colours::accent.withAlpha (0.8f));
            g.fillRect (b.getX() - 1.0f, b.getCentreY() - 1.0f, 4.0f, 2.0f);     // centre detent mark
            g.fillRect (b.getRight() - 3.0f, b.getCentreY() - 1.0f, 4.0f, 2.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { startY = e.position.y; startValue = value; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float span = (float) getHeight() * 0.85f;
        const float d = (startY - e.position.y) / span;
        setValue (startValue + d * (bipolar ? 2.0f : 1.0f), true);
    }
    void mouseUp (const juce::MouseEvent&) override { if (bipolar) setValue (0.0f, true); }      // pitch wheel springs back
    void mouseDoubleClick (const juce::MouseEvent&) override { if (bipolar) setValue (0.0f, true); }

    bool bipolar;
    std::function<void (float)> onChange;
    float value = 0.0f, startY = 0.0f, startValue = 0.0f;
};

// ------------------------------------------------------------------ keyboard that also lights up notes coming from MIDI
struct PlayKeyboard final : juce::MidiKeyboardComponent
{
    PlayKeyboard (NuSynthAudioProcessor& p, juce::MidiKeyboardState& st)
        : juce::MidiKeyboardComponent (st, juce::MidiKeyboardComponent::horizontalKeyboard), proc (p) {}

    bool held (int n) const
    {
        return n < 64 ? ((proc.heldLo.load() >> (juce::uint64) n) & 1u) != 0 : ((proc.heldHi.load() >> (juce::uint64) (n - 64)) & 1u) != 0;
    }

    void drawWhiteNote (int n, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour line, juce::Colour text) override
    {
        juce::MidiKeyboardComponent::drawWhiteNote (n, g, area, isDown || held (n), isOver, line, text);
    }
    void drawBlackNote (int n, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour fill) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (n, g, area, isDown || held (n), isOver, fill);
    }

    NuSynthAudioProcessor& proc;
};

// ------------------------------------------------------------------ wheels + octave + keyboard + key/vel tracker (bottom of the plugin)
struct KeyboardBar final : juce::Component, private juce::Timer
{
    explicit KeyboardBar (NuSynthAudioProcessor& p)
        : proc (p),
          pitchWheel (true, [this] (float v) { proc.sendMidi (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) (8192.0f + v * 8191.0f)))); }),
          modWheel (false, [this] (float v) { proc.sendMidi (juce::MidiMessage::controllerEvent (1, 1, (int) std::round (v * 127.0f))); }),
          modPill (MS_ModWheel, "MOD"), keyboard (p, p.keyboardState), kv (p)
    {
        pitchLabel.setText ("PITCH", juce::dontSendNotification);
        octLabel.setJustificationType (juce::Justification::centred);
        octLabel.setColour (juce::Label::textColourId, colours::text);
        octLabel.setFont (juce::FontOptions (11.0f));
        for (auto* l : { &pitchLabel })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::textColourId, colours::textDim);
            l->setFont (juce::FontOptions (9.5f));
        }
        octUp.setButtonText ("OCT +"); octDown.setButtonText ("OCT -");
        octUp.onClick   = [this] { setParamReal (proc.apvts, ids::octaveShift, std::min (3.0f, getParamReal (proc.apvts, ids::octaveShift) + 1.0f)); };
        octDown.onClick = [this] { setParamReal (proc.apvts, ids::octaveShift, std::max (-3.0f, getParamReal (proc.apvts, ids::octaveShift) - 1.0f)); };

        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colours::keyWhite);
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, colours::background);
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, colours::outline);
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha (0.7f));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.25f));
        keyboard.setScrollButtonsVisible (false);
        keyboard.setAvailableRange (24, 108);
        keyboard.setLowestVisibleKey (36);
        keyboard.setVelocity (proc.uiVelocity.load(), false);
        kv.onVelocity = [this] (float v) { keyboard.setVelocity (v, false); };

        for (juce::Component* c : std::initializer_list<juce::Component*> { &pitchWheel, &modWheel, &modPill, &pitchLabel, &octUp, &octDown, &octLabel, &keyboard, &kv })
            addAndMakeVisible (c);
        startTimerHz (30);
    }
    ~KeyboardBar() override { stopTimer(); }

    void timerCallback() override
    {
        const auto lo = proc.heldLo.load(), hi = proc.heldHi.load();
        if (lo != lastLo || hi != lastHi) { lastLo = lo; lastHi = hi; keyboard.repaint(); }
        const int oct = (int) getParamReal (proc.apvts, ids::octaveShift);
        const auto txt = "OCT " + juce::String (oct > 0 ? "+" : "") + juce::String (oct);
        if (octLabel.getText() != txt) octLabel.setText (txt, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 4);
        kv.setBounds (r.removeFromRight (240));
        r.removeFromRight (6);

        auto wheels = r.removeFromLeft (84);
        r.removeFromLeft (4);
        auto pcol = wheels.removeFromLeft (40);
        pitchLabel.setBounds (pcol.removeFromTop (14));
        pitchWheel.setBounds (pcol);
        modPill.setBounds (wheels.removeFromTop (14).reduced (2, 0));
        modWheel.setBounds (wheels.reduced (3, 0));

        auto oct = r.removeFromLeft (52);
        r.removeFromLeft (4);
        octUp.setBounds (oct.removeFromTop (28));
        octLabel.setBounds (oct.removeFromTop (22));
        octDown.setBounds (oct.removeFromTop (28));

        keyboard.setKeyWidth (juce::jmax (9.0f, (float) r.getWidth() / 50.0f));
        keyboard.setBounds (r);
    }

    NuSynthAudioProcessor& proc;
    WheelControl pitchWheel, modWheel;
    ModSourceHandle modPill;
    juce::Label pitchLabel, octLabel;
    juce::TextButton octUp, octDown;
    PlayKeyboard keyboard;
    KeyVelPanel kv;
    juce::uint64 lastLo = 0, lastHi = 0;
};
}
