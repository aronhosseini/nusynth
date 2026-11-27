#pragma once
#include "Widgets.h"
#include "Displays.h"
#include "FxGraphs.h"
#include "WaveWidgets.h"
#include "ImportMenu.h"
#include "BottomBar.h"
#include "Parameters/FxSpecs.h"
#include "PluginProcessor.h"
#include <map>
#include <algorithm>

namespace nusynth::ui
{
// ------------------------------------------------------------------ OSC A / B / C
inline bool isAudioFile (const juce::String& name)
{
    static const char* ext[] = { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a" };
    for (auto* e : ext) if (name.endsWithIgnoreCase (e)) return true;
    return false;
}

/** The warp selector of an oscillator: one button, a menu with the warp types grouped like Serum 2
    (Off / Sync / Alt Warp > / Filter > / Distortion > / FM > / PD > / AM > / RM >; FM, PD, AM and RM pick their source in the sub-menu). */
struct WarpMenuButton final : juce::Component, juce::SettableTooltipClient, private juce::Timer
{
    WarpMenuButton (APVTS& a, int o) : apvts (a), osc (o), modeId (ids::osc (o, "warpMode")), srcId (ids::osc (o, "warpSrc"))
    {
        setTooltip ("Warp type. FM / PD / AM / RM take their signal from another oscillator, the sub, the noise or a filter");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (10);
    }
    ~WarpMenuButton() override { stopTimer(); }

    int mode() const { return (int) getParamReal (apvts, modeId); }
    int src() const  { return (int) getParamReal (apvts, srcId); }

    void timerCallback() override { const auto t = warpDisplayText (mode(), src()); if (t != shown) repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (mode() == WM_Off ? colours::outline : colours::accent.withAlpha (0.8f));
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
        shown = warpDisplayText (mode(), src());
        g.setColour (mode() == WM_Off ? colours::textDim : colours::text);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (shown, getLocalBounds().reduced (8, 0).withTrimmedRight (14), juce::Justification::centredLeft, true);
        g.setColour (colours::textDim);
        g.drawText ("v", getLocalBounds().withTrimmedRight (6), juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent&) override { showMenu(); }

    void showMenu()
    {
        const int cur = mode(), curSrc = src();
        auto idOf = [] (int m, int s) { return 1000 + m * 10 + s; };
        juce::PopupMenu root;
        root.addItem (idOf (WM_Off, 0), "Off", true, cur == WM_Off);
        root.addItem (idOf (WM_Sync, 0), "Sync", true, cur == WM_Sync);
        for (const char* group : { "Alt Warp", "Filter", "Distortion" })
        {
            juce::PopupMenu sub;
            bool ticked = false;
            for (const auto& it : warpMenuItems())
                if (juce::String (it.group) == group) { sub.addItem (idOf (it.mode, 0), it.label, true, cur == it.mode); ticked = ticked || cur == it.mode; }
            root.addSubMenu (group, sub, true, nullptr, ticked);
        }
        const auto sources = warpSourceNames();
        const struct { int mode; const char* name; } srcModes[4] = { { WM_FM, "FM" }, { WM_PD, "PD" }, { WM_AM, "AM" }, { WM_RM, "RM" } };
        for (const auto& sm : srcModes)
        {
            juce::PopupMenu sub;
            for (int si = 0; si < sources.size(); ++si)
                sub.addItem (idOf (sm.mode, si), sources[si], si != osc, cur == sm.mode && curSrc == si);     // (an oscillator cannot modulate itself)
            root.addSubMenu (sm.name, sub, true, nullptr, cur == sm.mode);
        }
        juce::Component::SafePointer<WarpMenuButton> safe (this);
        root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr || r < 1000) return;
            const int m = (r - 1000) / 10, si = (r - 1000) % 10;
            setParamReal (safe->apvts, safe->modeId, (float) m);
            if (warpUsesSource (m)) setParamReal (safe->apvts, safe->srcId, (float) si);
            safe->repaint();
        });
    }

    APVTS& apvts;
    int osc;
    juce::String modeId, srcId, shown;
};

struct OscPanel final : Panel, juce::FileDragAndDropTarget
{
    OscPanel (NuSynthAudioProcessor& p, int o)
        : Panel (juce::String ("OSC ") + juce::String::charToString ((juce::juce_wchar) ('A' + o))), proc (p), oscIndex (o), display (p.apvts, o),
          wave (p.apvts, o), warpMenu (p.apvts, o),
          level (p.apvts, ids::osc (o, "level"), "Level"), pan (p.apvts, ids::osc (o, "pan"), "Pan"),
          pitch (p.apvts, ids::osc (o, "pitch"), "Pitch"), fine (p.apvts, ids::osc (o, "fine"), "Fine"), coarse (p.apvts, ids::osc (o, "coarse"), "Coarse"),
          position (p.apvts, ids::osc (o, "position"), "WT Pos"),
          warp (p.apvts, ids::osc (o, "warp"), "Warp"), voices (p.apvts, ids::osc (o, "unison"), "Voices"), spread (p.apvts, ids::osc (o, "spread"), "Spread"),
          detune (p.apvts, ids::osc (o, "detune"), "Detune"), blend (p.apvts, ids::osc (o, "blend"), "Blend"), phase (p.apvts, ids::osc (o, "phase"), "Phase")
    {
        display.onLoadFile = [this] (int slot, const juce::File& f) { importDropped (slot, f, true); };
        addPower (p.apvts, ids::osc (o, "on"));
        pencil.onClick = [this] { if (auto* h = findParentComponentOfClass<HarmonicHost>()) h->openHarmonicEditor (oscIndex); };
        addAndMakeVisible (pencil);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &display, &wave, &warpMenu, &level, &pan, &pitch, &fine, &coarse, &position,
                                                                           &warp, &voices, &spread, &detune, &blend, &phase })
            addAndMakeVisible (c);
    }

    // ---- drag & drop: any audio file dropped on this oscillator becomes its wavetable (a menu asks how to cut it into frames)
    bool isInterestedInFileDrag (const juce::StringArray& files) override { return files.size() > 0 && isAudioFile (files[0]); }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        repaint();
        if (files.size() == 0) return;
        const int cur = (int) getParamReal (proc.apvts, ids::osc (oscIndex, "wavetable"));
        const int slot = WavetableBank::isUserIndex (cur) ? cur - WavetableBank::kFirstUser : oscIndex;            // OSC A -> User 1, B -> User 2, C -> User 3
        importDropped (slot, juce::File (files[0]), true);
    }

    void importDropped (int slot, const juce::File& f, bool askMode)
    {
        auto done = [this, slot] (bool ok)
        {
            if (! ok) return;
            setParamReal (proc.apvts, ids::osc (oscIndex, "wavetable"), (float) (WavetableBank::kFirstUser + slot));
            ++display.editStamp;
            display.repaint();
        };
        if (! askMode) { done (proc.getUserContent().importWave (slot, f, UserContent::ImportMode::Default)); return; }
        juce::Component::SafePointer<OscPanel> safe (this);
        showWavetableImportMenu (&display, [safe, slot, f, done] (UserContent::ImportMode m)
        {
            if (safe != nullptr) done (safe->proc.getUserContent().importWave (slot, f, m));
        });
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! dragOver) return;
        g.setColour (colours::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 8.0f);
        g.setColour (colours::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 8.0f, 2.0f);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("Drop to import as wavetable", getLocalBounds(), juce::Justification::centred);
    }

    void resized() override
    {
        placeHeader();
        auto r = content();
        display.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.17f)));
        pencil.setBounds (display.getRight() - 24, display.getY() + 3, 20, 20);
        pencil.toFront (false);                   // must sit above the wave display
        r.removeFromTop (4);
        wave.setBounds (r.removeFromTop (24));
        r.removeFromTop (3);
        warpMenu.setBounds (r.removeFromTop (24));
        r.removeFromTop (3);
        grid (r, 4, 3, { &level, &pan, &pitch, &fine, &coarse, &position, &warp, &phase, &voices, &spread, &detune, &blend });
    }

    NuSynthAudioProcessor& proc;
    int oscIndex;
    bool dragOver = false;
    WaveDisplay display;
    PencilButton pencil;
    WaveSelector wave;
    WarpMenuButton warpMenu;
    ParamKnob level, pan, pitch, fine, coarse, position, warp, voices, spread, detune, blend, phase;
};

// ------------------------------------------------------------------ Noise
struct NoisePanel final : Panel, juce::FileDragAndDropTarget
{
    explicit NoisePanel (NuSynthAudioProcessor& p)
        : Panel ("NOISE OSC"), proc (p), display (p.apvts, p.getNoiseSample()), selector (p.apvts, ids::noiseType),
          loop (p.apvts, ids::noiseLoop, "LOOP"),
          cutoff (p.apvts, ids::noiseCutoff, "Filter"), hp (p.apvts, ids::noiseHP, "HP"), lp (p.apvts, ids::noiseLP, "LP"), level (p.apvts, ids::noiseLevel, "Level"),
          pitch (p.apvts, ids::noisePitch, "Pitch"), fine (p.apvts, ids::noiseFine, "Fine"), pan (p.apvts, ids::noisePan, "Pan")
    {
        addPower (p.apvts, ids::noiseOn);
        {   // menu order: Analog (the old basic noises + the analog set), Organic, S2 Noises, then Sample (parameter indices stay unchanged for old presets)
            constexpr int a0 = catalog::kFirstExtraNoise, b0 = a0 + catalog::kNoiseA, o0 = b0 + catalog::kNoiseB, end = o0 + catalog::kNoiseOrg;
            ChoiceSelector::Group analog { "Analog", { 0, 1, 2, 3, 4 } }, organic { "Organic", {} }, s2 { "S2 Noises", {} }, sampleItem { {}, { 5 } };
            for (int i = a0; i < b0; ++i) analog.items.push_back (i);
            for (int i = b0; i < o0; ++i) s2.items.push_back (i);
            for (int i = o0; i < end; ++i) organic.items.push_back (i);
            selector.groups = { analog, organic, s2, sampleItem };
        }
        loadButton.setButtonText ("LOAD");
        loadButton.setTooltip ("Load an audio sample for the 'Sample' noise type (or drop a file here)");
        loop.setTooltip ("Sample playback: lit = loop, off = one-shot");
        loadButton.onClick = [this]
        {
            chooser = std::make_shared<juce::FileChooser> ("Choose a sample for the noise oscillator", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            juce::Component::SafePointer<NoisePanel> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
            {
                if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadSample (fc.getResult());
            });
        };
        for (juce::Component* c : std::initializer_list<juce::Component*> { &display, &selector, &loadButton, &loop, &cutoff, &hp, &lp, &level, &pitch, &fine, &pan })
            addAndMakeVisible (c);
    }

    void loadSample (const juce::File& f)
    {
        if (proc.getUserContent().loadNoiseSample (f))
            setParamReal (proc.apvts, ids::noiseType, 5.0f);        // switch to "Sample"
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override { return files.size() > 0 && isAudioFile (files[0]); }
    void filesDropped (const juce::StringArray& files, int, int) override { loadSample (juce::File (files[0])); }

    void resized() override
    {
        placeHeader();
        auto r = content();
        display.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.27f)));
        r.removeFromTop (3);
        auto selRow = r.removeFromTop (24);                       // (LOAD / LOOP live here, so they can never cover the on / off button in the header)
        loop.setBounds (selRow.removeFromRight (44));
        selRow.removeFromRight (3);
        loadButton.setBounds (selRow.removeFromRight (44));
        selRow.removeFromRight (3);
        selector.setBounds (selRow);
        r.removeFromTop (3);
        auto top = r.removeFromTop (r.getHeight() / 2);
        const int w = top.getWidth() / 4;
        cutoff.setBounds (top.removeFromLeft (w));
        hp.setBounds (centred (top.removeFromLeft (w), w - 6, 22));
        lp.setBounds (centred (top.removeFromLeft (w), w - 6, 22));
        level.setBounds (top);
        row (r, { &pitch, &fine, &pan });
    }

    NuSynthAudioProcessor& proc;
    NoiseDisplay display;
    ChoiceSelector selector;
    juce::TextButton loadButton;
    ParamToggle loop;
    ParamKnob cutoff;
    ParamToggle hp, lp;
    ParamKnob level, pitch, fine, pan;
    std::shared_ptr<juce::FileChooser> chooser;
};

// ------------------------------------------------------------------ Sub
struct SubPanel final : Panel
{
    explicit SubPanel (APVTS& a)
        : Panel ("SUB OSC"), wave (a, ids::subWave, true), octave (a, ids::subOctave, "Octave"), level (a, ids::subLevel, "Level"),
          pan (a, ids::subPan, "Pan")
    {
        addPower (a, ids::subOn);
        wave.fontSize = 10.0f;
        addAndMakeVisible (wave); addAndMakeVisible (octave); addAndMakeVisible (level); addAndMakeVisible (pan);
    }

    void resized() override
    {
        placeHeader();
        auto r = content();
        wave.setBounds (r.removeFromTop (24));
        r.removeFromTop (2);
        row (r, { &octave, &level, &pan });
    }

    ChoiceList wave;
    ParamKnob octave, level, pan;
};

// ------------------------------------------------------------------ Filters (two instances + routing)
struct FilterControls final : juce::Component
{
    FilterControls (APVTS& a, int f)
        : on (a, ids::filter (f, "on"), "ON"), key (a, ids::filter (f, "keytrack"), "KEY"), type (a, ids::filter (f, "type")), display (a, f),
          cutoff (a, ids::filter (f, "cutoff"), "Cutoff"), res (a, ids::filter (f, "res"), "Resonance"),
          drive (a, ids::filter (f, "drive"), "Drive"), mix (a, ids::filter (f, "mix"), "Mix")
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &on, &key, &type, &display, &cutoff, &res, &drive, &mix })
            addAndMakeVisible (c);
        key.setTooltip ("Key track: the cutoff follows the played note (1 octave per octave)");
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (24);
        on.setBounds (top.removeFromLeft (40));
        top.removeFromLeft (3);
        key.setBounds (top.removeFromLeft (40));
        top.removeFromLeft (4);
        type.setBounds (top);
        r.removeFromTop (4);
        auto knobs = r.removeFromBottom (juce::jmax (52, (int) ((float) r.getHeight() * 0.4f)));
        display.setBounds (r.reduced (0, 2));
        row (knobs, { &cutoff, &res, &drive, &mix });
    }

    ParamToggle on, key;
    ParamCombo type;
    FilterDisplay display;
    ParamKnob cutoff, res, drive, mix;
};

struct FilterPanel final : MultiPanel
{
    explicit FilterPanel (APVTS& a) : MultiPanel ("FILTER", { "F1", "F2" })
    {
        addPage (std::make_unique<FilterControls> (a, 0));
        addPage (std::make_unique<FilterControls> (a, 1));
        const char* names[5]  = { "Osc A", "Osc B", "Osc C", "Sub", "Noise" };
        const char* routes[5] = { "oscA", "oscB", "oscC", "sub", "noise" };
        for (int i = 0; i < 5; ++i)
        {
            labels[(size_t) i].setText (names[i], juce::dontSendNotification);
            labels[(size_t) i].setColour (juce::Label::textColourId, colours::textDim);
            labels[(size_t) i].setFont (juce::FontOptions (11.0f));
            addAndMakeVisible (labels[(size_t) i]);
            combos[(size_t) i] = std::make_unique<ParamCombo> (a, ids::route (routes[i]));
            addAndMakeVisible (*combos[(size_t) i]);
        }
        select (0);
    }

    juce::Rectangle<int> pageBounds() const override { return content().withTrimmedBottom (88); }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (10.0f);
        g.drawText ("SIGNAL ROUTING", 10, getHeight() - 96, 120, 12, juce::Justification::centredLeft);
    }

    void resized() override
    {
        MultiPanel::resized();
        auto r = getLocalBounds().reduced (8);
        r = r.removeFromBottom (82);
        auto left = r.removeFromLeft (r.getWidth() / 2);
        auto place = [] (juce::Rectangle<int> cell, juce::Label& l, juce::Component& c)
        {
            l.setBounds (cell.removeFromLeft (42));
            c.setBounds (cell.reduced (2, 1));
        };
        place (left.removeFromTop (27), labels[0], *combos[0]);
        place (left.removeFromTop (27), labels[1], *combos[1]);
        place (left.removeFromTop (27), labels[2], *combos[2]);
        place (r.removeFromTop (27),    labels[3], *combos[3]);
        place (r.removeFromTop (27),    labels[4], *combos[4]);
    }

    std::array<juce::Label, 5> labels;
    std::array<std::unique_ptr<ParamCombo>, 5> combos;
};

// ------------------------------------------------------------------ LFOs (drag key sits in the header, behind the 1 2 3 buttons)
struct LfoControls final : juce::Component
{
    LfoControls (APVTS& a, int l)
        : editor (a, l), shape (a, ids::lfo (l, "shape")), mode (a, ids::lfo (l, "mode")), sync (a, ids::lfo (l, "sync"), "SYNC"), div (a, ids::lfo (l, "div")),
          rate (a, ids::lfo (l, "rate"), "Rate"), depth (a, ids::lfo (l, "depth"), "Depth"),
          phase (a, ids::lfo (l, "phase"), "Phase"), smooth (a, ids::lfo (l, "smooth"), "Smooth")
    {
        mode.setTooltip ("Free: the LFO keeps running between notes.  Retrig: restarts on every note.  Envelope: plays once per note, then holds its last value");
        for (juce::Component* c : std::initializer_list<juce::Component*> { &editor, &shape, &mode, &sync, &div, &rate, &depth, &phase, &smooth })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        editor.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.46f)));
        r.removeFromTop (4);
        auto mid = r.removeFromTop (22);
        div.setBounds (mid.removeFromRight (74));
        sync.setBounds (mid.removeFromRight (56).reduced (2, 0));
        mode.setBounds (mid.removeFromRight (78).reduced (2, 0));
        shape.setBounds (mid.withTrimmedRight (2));
        row (r, { &rate, &depth, &phase, &smooth });
    }

    LfoEditor editor;
    ParamCombo shape, mode;
    ParamToggle sync;
    ParamCombo div;
    ParamKnob rate, depth, phase, smooth;
};

struct LfoPanel final : MultiPanel
{
    explicit LfoPanel (APVTS& a) : MultiPanel ("6 LFOs", { "1", "2", "3", "4", "5", "6" }, MS_Lfo1, "LFO")
    {
        for (int i = 0; i < kNumLfos; ++i) addPage (std::make_unique<LfoControls> (a, i));
        select (0);
    }
};

// ------------------------------------------------------------------ Envelopes
struct EnvControls final : juce::Component
{
    EnvControls (APVTS& a, int e)
        : editor (a, e), attack (a, ids::env (e, "attack"), "Attack"), decay (a, ids::env (e, "decay"), "Decay"),
          sustain (a, ids::env (e, "sustain"), "Sustain"), release (a, ids::env (e, "release"), "Release")
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &editor, &attack, &decay, &sustain, &release })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        editor.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.6f)));
        r.removeFromTop (4);
        row (r, { &attack, &decay, &sustain, &release });
    }

    EnvEditor editor;
    ParamKnob attack, decay, sustain, release;
};

struct EnvPanel final : MultiPanel
{
    explicit EnvPanel (APVTS& a) : MultiPanel ("4 ENVELOPES", { "1", "2", "3", "4" }, MS_Env1, "ENV")
    {
        for (int i = 0; i < kNumEnvs; ++i) addPage (std::make_unique<EnvControls> (a, i));
        select (0);
    }
};

// ------------------------------------------------------------------ Play modes + polyphony + arpeggiator
struct PlayPanel final : Panel
{
    explicit PlayPanel (APVTS& a)
        : Panel ("PLAY MODES & ARPEGGIATOR"), mode (a, ids::playMode, true),
          glide (a, ids::glideTime, "Glide"), curve (a, ids::glideCurve, "Curve"), voices (a, ids::maxVoices, "Polyphony"),
          arpOn (a, ids::arpOn, "ARP ON"), pattern (a, ids::arpPattern), rate (a, ids::arpRate),
          octaves (a, ids::arpOctaves, "Octaves"), gate (a, ids::arpGate, "Gate"), swing (a, ids::arpSwing, "Swing"), display (a)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &glide, &curve, &voices, &arpOn, &pattern, &rate, &octaves, &gate, &swing, &display })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        auto r = content();
        auto top = r.removeFromTop (54);
        auto knobsTop = top.removeFromRight ((int) ((float) top.getWidth() * 0.6f));
        mode.setBounds (top.withSizeKeepingCentre (top.getWidth() - 6, 24));
        row (knobsTop, { &glide, &curve, &voices });
        r.removeFromTop (2);
        auto combos = r.removeFromTop (24);
        arpOn.setBounds (combos.removeFromLeft (72));
        combos.removeFromLeft (4);
        pattern.setBounds (combos.removeFromLeft ((int) ((float) combos.getWidth() * 0.55f)));
        combos.removeFromLeft (4);
        rate.setBounds (combos);
        r.removeFromTop (2);
        auto left = r.removeFromLeft ((int) ((float) r.getWidth() * 0.62f));
        row (left, { &octaves, &gate, &swing });
        display.setBounds (r.reduced (4, 2));
    }

    ChoiceList mode;
    ParamKnob glide, curve, voices;
    ParamToggle arpOn;
    ParamCombo pattern, rate;
    ParamKnob octaves, gate, swing;
    ArpDisplay display;
};

// ------------------------------------------------------------------ AI sound designer (compact)
struct AiPanel final : Panel
{
    explicit AiPanel (NuSynthAudioProcessor& p) : Panel ("AI SOUND DESIGNER"), proc (p)
    {
        prompt.setMultiLine (true, true);
        prompt.setReturnKeyStartsNewLine (false);
        prompt.setFont (juce::FontOptions (13.0f));
        prompt.setTextToShowWhenEmpty ("warm analog pad, slow, wide, with moving filter...", colours::textDim);
        prompt.onReturnKey = [this] { generate(); };
        addAndMakeVisible (prompt);

        button.getProperties().set ("generate", true);                     // gradient cyan -> pink button (see NuLookAndFeel)
        button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff04222a));
        button.onClick = [this] { generate(); };
        addAndMakeVisible (button);

        // OFFLINE (built-in designer, default) <-> ONLINE (OpenAI-compatible endpoint; NO fallback: shows an error when not connected)
        modeButton.setClickingTogglesState (true);
        modeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1f7a5a));
        modeButton.setToggleState (proc.getPromptBridge().isOnlineMode(), juce::dontSendNotification);
        modeButton.setButtonText (modeButton.getToggleState() ? "ONLINE" : "OFFLINE");
        modeButton.setTooltip ("OFFLINE: built-in sound designer. ONLINE: ask the configured AI endpoint (shows an error if it is not connected).");
        modeButton.onClick = [this]
        {
            const bool on = modeButton.getToggleState();
            proc.getPromptBridge().setOnlineMode (on);
            modeButton.setButtonText (on ? "ONLINE" : "OFFLINE");
            status.setColour (juce::Label::textColourId, colours::textDim);
            status.setText (on ? "Online mode: sounds come from the AI endpoint. If it is not connected you get an error (no offline fallback)."
                               : "Offline mode: built-in sound designer.", juce::dontSendNotification);
        };
        addAndMakeVisible (modeButton);

        status.setColour (juce::Label::textColourId, colours::textDim);
        status.setFont (juce::FontOptions (10.5f));
        status.setJustificationType (juce::Justification::topLeft);
        status.setText ("Uses oscillators, filters, envelopes, LFOs, mod matrix, FX and arp.", juce::dontSendNotification);
        addAndMakeVisible (status);
    }

    void generate()
    {
        const auto text = prompt.getText().trim();
        if (text.isEmpty()) return;
        button.setEnabled (false);
        status.setColour (juce::Label::textColourId, colours::textDim);
        status.setText (proc.getPromptBridge().isOnlineMode() ? "Designing (Online)..." : "Designing (Offline)...", juce::dontSendNotification);
        juce::Component::SafePointer<AiPanel> safe (this);
        proc.getPromptBridge().submit (text, [safe] (bool ok, const juce::String& msg)
        {
            if (safe == nullptr) return;
            safe->button.setEnabled (true);
            safe->status.setColour (juce::Label::textColourId, ok ? colours::textDim : juce::Colour (0xffff6b6b));     // red: online not connected
            safe->status.setText (msg, juce::dontSendNotification);
        });
    }

    void resized() override
    {
        auto r = content();
        auto row = r.removeFromBottom (26);
        modeButton.setBounds (row.removeFromRight (juce::jmin (84, row.getWidth() / 3)));
        row.removeFromRight (4);
        button.setBounds (row);
        r.removeFromBottom (4);
        status.setBounds (r.removeFromBottom ((int) ((float) r.getHeight() * 0.42f)));
        r.removeFromBottom (3);
        prompt.setBounds (r);
    }

    NuSynthAudioProcessor& proc;
    juce::TextEditor prompt;
    juce::TextButton button { "GENERATE" };
    juce::TextButton modeButton { "OFFLINE" };
    juce::Label status;
};

// ------------------------------------------------------------------ Mod matrix (scrolls; rows appear as connections are used)
struct ModPanel final : Panel
{
    ModPanel (APVTS& a, int rows, bool curve, const juce::String& title = "MOD MATRIX")
        : Panel (title), numRows (rows), view (a, rows, curve)
    {
        viewport.setViewedComponent (&view, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (10.5f);
        g.drawText ("Tip: drag a pink LFO / ENV / VEL / KEY key onto any knob to create a row automatically",
                    getWidth() - 560, 6, 550, 16, juce::Justification::centredRight);
    }

    void resized() override
    {
        viewport.setBounds (content());
        view.setSize (std::max (100, viewport.getWidth() - viewport.getScrollBarThickness()), view.heightForRows());
    }

    int numRows;
    ModMatrixView view;
    juce::Viewport viewport;
};

// ------------------------------------------------------------------ FX rack (Serum-style modules with graphs)
struct LabeledCombo final : juce::Component
{
    LabeledCombo (APVTS& a, const juce::String& id, const juce::String& text) : combo (a, id)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, colours::textDim);
        label.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (label);
        addAndMakeVisible (combo);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (3, 0);
        label.setBounds (r.removeFromBottom (14));
        combo.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
    }
    juce::Label label;
    ParamCombo combo;
};

/** Is this rack slot part of the rack?  (added through "+ FX", or switched on - old presets only set "on"). */
inline bool fxSlotVisible (APVTS& a, int slot)
{
    return getParamReal (a, ids::fx (slot, "added")) > 0.5f || getParamReal (a, ids::fx (slot, "on")) > 0.5f;
}

/** Slots in the rack, in processing order. */
inline std::vector<int> fxVisibleOrder (APVTS& a)
{
    std::vector<int> v;
    for (int i = 0; i < kNumFxSlots; ++i) if (fxSlotVisible (a, i)) v.push_back (i);
    std::stable_sort (v.begin(), v.end(), [&a] (int x, int y) { return getParamReal (a, ids::fx (x, "order")) < getParamReal (a, ids::fx (y, "order")); });
    return v;
}

/** Grip on the left of every effect header: drag it up / down to re-order the chain. */
struct FxGrip final : juce::Component, juce::SettableTooltipClient
{
    std::function<void (int, float)> callback;          // phase (0 start, 1 drag, 2 end), y in the rack
    juce::Component* rack = nullptr;

    FxGrip() { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); setTooltip ("Drag up / down to re-order the effect chain"); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.0f, 6.0f);
        g.setColour (colours::textDim);
        for (int i = 0; i < 3; ++i) g.fillRoundedRectangle (b.getX(), b.getY() + (float) i * b.getHeight() / 3.0f + 1.0f, b.getWidth(), 2.0f, 1.0f);
    }
    float rackY (const juce::MouseEvent& e) const { return rack != nullptr ? e.getEventRelativeTo (rack).position.y : e.position.y; }
    void mouseDown (const juce::MouseEvent& e) override { if (callback) callback (0, rackY (e)); }
    void mouseDrag (const juce::MouseEvent& e) override { if (callback) callback (1, rackY (e)); }
    void mouseUp (const juce::MouseEvent& e) override   { if (callback) callback (2, rackY (e)); }
};

/** One effect of the rack (slot = type + 12 * copy). The compressor shows the controls of its current mode (SINGLE / OTT). */
struct FxModule final : juce::Component, private juce::Timer
{
    std::function<void (juce::Point<int>)> onBlankRightClick;      // right-click on the empty background of this effect -> "+ FX" menu
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onBlankRightClick) onBlankRightClick (e.getScreenPosition());
    }

    FxModule (APVTS& a, int slotIdx, juce::Component* rackComp, std::function<void (int, int, float)> onDrag, std::function<void (int)> onRemove)
        : slot (slotIdx), type (fxTypeOf (slotIdx)),
          on (a, ids::fx (slotIdx, "on"), fxNames()[fxTypeOf (slotIdx)] + (fxCopyOf (slotIdx) > 0 ? " " + juce::String (fxCopyOf (slotIdx) + 1) : juce::String())),
          graph (a, slotIdx)
    {
        on.setColour (juce::TextButton::buttonOnColourId, fxColour (type));
        grip.rack = rackComp;
        grip.callback = [this, onDrag] (int phase, float y) { onDrag (slot, phase, y); };
        addAndMakeVisible (grip); addAndMakeVisible (on); addAndMakeVisible (graph);

        remove.setButtonText ("x");
        remove.setTooltip ("Remove this effect from the rack");
        remove.onClick = [this, onRemove] { onRemove (slot); };
        addAndMakeVisible (remove);

        if (type == 7 || type == 8)          // dynamics effects: live input level + gain reduction display
        {
            meter = std::make_unique<FxMeterView> (a, slotIdx);
            addAndMakeVisible (*meter);
        }
        if (type == 7) modeParam = a.getRawParameterValue (ids::fxp (slotIdx, 6));

        const auto& specs = fxSpecs (type);
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            const auto& sp = specs[(size_t) k];
            const juce::String id = ids::fxp (slotIdx, k);
            if (type == 7 && k == 6)                   controls.push_back (std::make_unique<ChoiceList> (a, id, true));   // SINGLE | OTT toggle
            else if (sp.kind == FxParamSpec::Float)    controls.push_back (std::make_unique<ParamKnob> (a, id, sp.name));
            else if (sp.kind == FxParamSpec::Choice)   controls.push_back (std::make_unique<LabeledCombo> (a, id, sp.name));
            else                                       controls.push_back (std::make_unique<ParamToggle> (a, id, sp.name));
            controlParam.push_back (k);
        }
        if (fxHasMix (type)) { controls.push_back (std::make_unique<ParamKnob> (a, ids::fx (slotIdx, "mix"), "Mix")); controlParam.push_back (-1); }
        for (auto& c : controls) addAndMakeVisible (*c);
        updateModeVisibility();
        if (type == 7) startTimerHz (8);
    }
    ~FxModule() override { stopTimer(); }

    bool isOttMode() const { return modeParam != nullptr && modeParam->load() >= 0.5f; }

    /** Compressor: parameters 0..5 belong to OTT, 7..12 to Single; 6 (the mode toggle) and Mix are always shown. */
    bool controlShown (size_t i) const
    {
        if (type != 7) return true;
        const int k = controlParam[i];
        if (k == 6 || k < 0) return true;
        return k < 6 ? isOttMode() : ! isOttMode();
    }

    void updateModeVisibility()
    {
        for (size_t i = 0; i < controls.size(); ++i) controls[i]->setVisible (controlShown (i));
        resized();
    }

    void timerCallback() override
    {
        const bool ott = isOttMode();
        if (ott != lastOtt) { lastOtt = ott; updateModeVisibility(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        gfx::panelBackground (g, b, 7.0f, 0.55f);                       // same teal -> violet glass as the main panels, a bit darker
        g.setColour (fxColour (type));
        g.fillRoundedRectangle (b.withWidth (5.0f).reduced (0.0f, 3.0f), 2.0f);
        if (dragging) { g.setColour (colours::accent); g.drawRoundedRectangle (b, 7.0f, 2.0f); }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto head = r.removeFromTop (24);
        remove.setBounds (head.removeFromRight (24));
        grip.setBounds (head.removeFromLeft (20));
        on.setBounds (head.removeFromLeft (170));
        r.removeFromTop (4);
        graph.setBounds (r.removeFromLeft (juce::jmax (170, (int) ((float) r.getWidth() * (meter != nullptr ? 0.26f : 0.32f)))));
        if (meter != nullptr)
        {
            r.removeFromLeft (6);
            meter->setBounds (r.removeFromLeft (juce::jmax (150, (int) ((float) r.getWidth() * 0.22f))));
        }
        r.removeFromLeft (8);

        // always two rows, so every knob has the same size in every effect (a short effect no longer gets giant knobs)
        std::vector<juce::Component*> shown;
        for (size_t i = 0; i < controls.size(); ++i) if (controlShown (i)) shown.push_back (controls[i].get());
        const int n = (int) shown.size();
        const int cols = juce::jmax (1, (n + 1) / 2), rowsN = 2;
        const int cw = r.getWidth() / cols, ch = r.getHeight() / rowsN;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<int> (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch);
            if (dynamic_cast<ParamToggle*> (shown[(size_t) i]) != nullptr || dynamic_cast<ChoiceList*> (shown[(size_t) i]) != nullptr) cell = centred (cell, cw - 8, 24);
            shown[(size_t) i]->setBounds (cell);
        }
    }

    int slot, type;
    bool dragging = false, lastOtt = true;
    FxGrip grip;
    ParamToggle on;
    FxGraph graph;
    juce::TextButton remove;
    std::unique_ptr<FxMeterView> meter;
    std::atomic<float>* modeParam = nullptr;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<int> controlParam;            // spec index of each control (-1 = the mix knob)
};

/** The rack: only effects that were added are shown (and exist as components). "+ FX" adds one, "x" removes one. */
struct FxRack final : juce::Component, private juce::Timer
{
    static constexpr int kModuleH = 152, kGap = 6;

    explicit FxRack (APVTS& a) : apvts (a)
    {
        setBufferedToImage (true);                 // smooth scrolling of the long effect list
        syncModules();
        startTimerHz (6);
    }
    ~FxRack() override { stopTimer(); }

    std::function<void()> onHeightChanged;

    int totalHeight() const { return juce::jmax (1, (int) modules.size()) * (kModuleH + kGap); }

    /** Adds a new instance of an effect type at the end of the chain (returns false when all kFxCopies copies are in use). */
    bool addEffect (int type)
    {
        int slot = -1;
        for (int c = 0; c < kFxCopies && slot < 0; ++c)
            if (! fxSlotVisible (apvts, type + kNumFxTypes * c)) slot = type + kNumFxTypes * c;
        if (slot < 0) return false;

        const auto cur = fxVisibleOrder (apvts);
        for (size_t q = 0; q < cur.size(); ++q) setParamReal (apvts, ids::fx (cur[q], "order"), (float) q);     // 0..n-1
        resetSlot (slot);
        setParamReal (apvts, ids::fx (slot, "order"), (float) cur.size());
        if (type == 7) setParamReal (apvts, ids::fxp (slot, 6), 0.0f);                                              // a new compressor starts in SINGLE mode
        setParamReal (apvts, ids::fx (slot, "added"), 1.0f);
        setParamReal (apvts, ids::fx (slot, "on"), 1.0f);
        syncModules();
        return true;
    }

    void removeEffect (int slot)
    {
        resetSlot (slot);
        setParamReal (apvts, ids::fx (slot, "added"), 0.0f);
        setParamReal (apvts, ids::fx (slot, "on"), 0.0f);
        juce::Component::SafePointer<FxRack> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->syncModules(); });       // never delete a module from inside its own button click
    }

    void resetSlot (int slot)
    {
        auto reset = [this] (const juce::String& id) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue()); };
        const int n = (int) fxSpecs (slot).size();
        for (int k = 0; k < n; ++k) reset (ids::fxp (slot, k));
        reset (ids::fx (slot, "mix"));
    }

    std::vector<int> currentOrder() const { return fxVisibleOrder (apvts); }

    /** order of the modules while one is being dragged (the dragged module is inserted where it would land) */
    std::vector<int> previewOrder() const
    {
        const int H = kModuleH + kGap;
        std::vector<int> others;
        for (int fx : dragOrder) if (fx != dragFx) others.push_back (fx);
        const int target = juce::jlimit (0, (int) others.size(), (int) ((dragY + (float) H * 0.5f) / (float) H));
        others.insert (others.begin() + target, dragFx);
        return others;
    }

    void layoutModules()
    {
        const int H = kModuleH + kGap;
        const auto ord = dragFx >= 0 ? previewOrder() : currentOrder();
        for (size_t q = 0; q < ord.size(); ++q)
        {
            const int fx = ord[q];
            auto it = modules.find (fx);
            if (it == modules.end()) continue;
            if (fx == dragFx) { it->second->setBounds (0, (int) dragY, getWidth(), kModuleH); it->second->toFront (false); }
            else              it->second->setBounds (0, (int) q * H, getWidth(), kModuleH);
        }
    }

    /** Creates / deletes module components so they match the effects that are in the rack. */
    void syncModules()
    {
        if (dragFx >= 0) return;
        bool changed = false;
        const auto vis = fxVisibleOrder (apvts);
        for (auto it = modules.begin(); it != modules.end();)
        {
            if (std::find (vis.begin(), vis.end(), it->first) == vis.end()) { removeChildComponent (it->second.get()); it = modules.erase (it); changed = true; }
            else ++it;
        }
        for (int slot : vis)
            if (modules.find (slot) == modules.end())
            {
                auto m = std::make_unique<FxModule> (apvts, slot, this, [this] (int fxIdx, int phase, float y) { onModuleDrag (fxIdx, phase, y); },
                                                     [this] (int s) { removeEffect (s); });
                m->onBlankRightClick = [this] (juce::Point<int> pos) { if (onBlankRightClick) onBlankRightClick (pos); };
                addAndMakeVisible (*m);
                modules[slot] = std::move (m);
                changed = true;
            }
        lastSig = signature();
        layoutModules();
        if (changed && onHeightChanged) onHeightChanged();
    }

    float signature() const
    {
        float sig = 0.0f;
        for (int i = 0; i < kNumFxSlots; ++i)
            if (fxSlotVisible (apvts, i)) sig += (getParamReal (apvts, ids::fx (i, "order")) + 1.0f) * (float) (i + 1) + (float) (i * 1000);
        return sig;
    }

    void onModuleDrag (int fx, int phase, float yInRack)
    {
        const int H = kModuleH + kGap;
        if (phase == 0)
        {
            dragFx = fx;
            dragOrder = currentOrder();
            grabOffset = yInRack - (float) modules[fx]->getY();
            modules[fx]->dragging = true;
        }
        if (dragFx < 0) return;
        dragY = juce::jlimit (0.0f, (float) (std::max (0, (int) modules.size() - 1) * H), yInRack - grabOffset);
        if (phase == 2)
        {
            const auto ord = previewOrder();
            for (size_t q = 0; q < ord.size(); ++q) setParamReal (apvts, ids::fx (ord[q], "order"), (float) q);
            modules[dragFx]->dragging = false;
            modules[dragFx]->repaint();
            dragFx = -1;
        }
        layoutModules();
    }

    void resized() override { layoutModules(); }

    std::function<void (juce::Point<int>)> onBlankRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onBlankRightClick) onBlankRightClick (e.getScreenPosition());
    }

    void timerCallback() override
    {
        if (dragFx >= 0) return;
        if (signature() != lastSig) syncModules();
    }

    APVTS& apvts;
    std::map<int, std::unique_ptr<FxModule>> modules;
    std::vector<int> dragOrder;
    int dragFx = -1;
    float dragY = 0.0f, grabOffset = 0.0f, lastSig = -1.0f;
};

struct FxPanel final : Panel
{
    explicit FxPanel (APVTS& a) : Panel ("FX RACK"), rack (a)
    {
        viewport.setViewedComponent (&rack, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
        rack.onHeightChanged = [this] { resizeRack(); };
        rack.onBlankRightClick = [this] (juce::Point<int> pos) { showAddMenu (pos); };

        addButton.setButtonText ("+ FX");
        addButton.setTooltip ("Add an effect to the rack - you can add as many as you like (several reverbs, several distortions ...)");
        addButton.onClick = [this] { showAddMenu(); };
        addAndMakeVisible (addButton);
    }

    void showAddMenu (juce::Point<int> screenPos = { -1, -1 })
    {
        juce::PopupMenu m;
        m.addSectionHeader ("+ FX");
        const auto names = fxNames();
        for (int t : fxMenuTypes()) m.addItem (t + 1, names[t]);
        juce::Component::SafePointer<FxPanel> safe (this);
        auto opts = screenPos.x >= 0 ? juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1))
                                     : juce::PopupMenu::Options().withTargetComponent (&addButton);
        m.showMenuAsync (opts, [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (! safe->rack.addEffect (r - 1))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "FX rack",
                                                        "All " + juce::String (kFxCopies) + " copies of this effect are already in the rack.");
            safe->resizeRack();
            safe->viewport.setViewPosition (0, std::max (0, safe->rack.getHeight() - safe->viewport.getHeight()));      // show the new effect
        });
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (10.5f);
        g.drawText ("+ FX or right-click empty space adds an effect  -  drag the grip (left) to re-order  -  x removes  -  drop pink keys on any knob  -  EQ: drag the nodes",
                    getWidth() - 880, 6, 810, 16, juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) showAddMenu (e.getScreenPosition());
    }

    void resizeRack() { rack.setSize (std::max (200, viewport.getWidth() - viewport.getScrollBarThickness()), std::max (rack.totalHeight(), viewport.getHeight())); }

    void resized() override
    {
        addButton.setBounds (10 + title.length() * 9 + 8, 5, 50, 17);
        viewport.setBounds (content());
        resizeRack();
    }

    FxRack rack;
    juce::Viewport viewport;
    juce::TextButton addButton;
};

// ------------------------------------------------------------------ Microtune (lives on the OTHER page, below the matrix)
// ------------------------------------------------------------------ Microtune page
struct MicrotunePanel final : Panel
{
    explicit MicrotunePanel (APVTS& a) : Panel ("PER-KEY MICROTUNING"), keyboard (a)
    {
        addAndMakeVisible (keyboard);
        reset.setButtonText ("RESET");
        reset.onClick = [this] { keyboard.resetAll(); };
        addAndMakeVisible (reset);
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (10.0f);
        g.drawText ("click a key - drag the slider (-100 / +100 cents) - applies to ALL octaves - double-click a key to TYPE its value - right-click: reset",
                    10, getHeight() - 20, getWidth() - 20, 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        reset.setBounds (getWidth() - 64, 5, 56, 17);
        auto r = content();
        r.removeFromBottom (14);
        keyboard.setBounds (r);
    }

    MicroKeyboard keyboard;
    juce::TextButton reset;
};

// ------------------------------------------------------------------ Voicing (Serum 2 style)
struct VoicePanel final : Panel
{
    explicit VoicePanel (NuSynthAudioProcessor& p)
        : Panel ("VOICING"), proc (p), mode (p.apvts, ids::playMode, true),
          voices (p.apvts, ids::maxVoices, "Polyphony"), glide (p.apvts, ids::glideTime, "Glide"), curve (p.apvts, ids::glideCurve, "Curve"),
          bendUp (p.apvts, ids::bendUp, "Bend +"), bendDown (p.apvts, ids::bendDown, "Bend -"), octave (p.apvts, ids::octaveShift, "Octave")
    {
        panic.setButtonText ("PANIC");
        panic.setTooltip ("Silence every voice immediately (use it if a note ever hangs)");
        panic.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7a2b2b));
        panic.setColour (juce::TextButton::textColourOffId, colours::text);
        panic.onClick = [this] { proc.requestPanic(); };
        for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &voices, &glide, &curve, &bendUp, &bendDown, &octave, &panic })
            addAndMakeVisible (c);
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (10.0f);
        g.drawText ("MODE", 12, 30, 60, 12, juce::Justification::centredLeft);
        g.drawText ("PITCH BEND RANGE: up to +/- 24 semitones", 12, getHeight() - 20, getWidth() - 24, 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = content();
        r.removeFromTop (14);
        mode.setBounds (r.removeFromTop (26).reduced (0, 1));
        r.removeFromTop (4);
        r.removeFromBottom (16);
        panic.setBounds (getWidth() - 70, 5, 60, 17);
        grid (r, 3, 2, { &voices, &glide, &curve, &bendUp, &bendDown, &octave });
    }

    NuSynthAudioProcessor& proc;
    ChoiceList mode;
    ParamKnob voices, glide, curve, bendUp, bendDown, octave;
    juce::TextButton panic;
};

// ------------------------------------------------------------------ top strip on MAIN: modulation keys + master tune + master volume
struct TopStrip final : juce::Component
{
    explicit TopStrip (APVTS& a, juce::String hint = "Drag a pink key (LFO, ENV, KEY, MOD, M1-M8) onto any knob to modulate it")
        : macros (a), text (std::move (hint)) { addAndMakeVisible (macros); }

    void paint (juce::Graphics& g) override
    {
        gfx::panelBackground (g, getLocalBounds().toFloat(), 9.0f, 0.38f);
        g.setColour (colours::text.withAlpha (0.8f));
        g.setFont (12.0f);
        g.drawText (text, 14, 0, getWidth() / 2 - 20, getHeight(), juce::Justification::centredLeft, true);
    }

    void resized() override { macros.setBounds (getLocalBounds().removeFromRight (getWidth() / 2).reduced (14, 2)); }

    MacroBar macros;
    juce::String text;
};
}
