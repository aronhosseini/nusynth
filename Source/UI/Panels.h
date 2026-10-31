#pragma once
#include "Widgets.h"
#include "Displays.h"
#include "FxGraphs.h"
#include "WaveWidgets.h"
#include "BottomBar.h"
#include "Parameters/FxSpecs.h"
#include "PluginProcessor.h"

namespace nusynth::ui
{
// ------------------------------------------------------------------ OSC A / B / C
inline bool isAudioFile (const juce::String& name)
{
    static const char* ext[] = { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a" };
    for (auto* e : ext) if (name.endsWithIgnoreCase (e)) return true;
    return false;
}

struct OscPanel final : Panel, juce::FileDragAndDropTarget
{
    OscPanel (NuSynthAudioProcessor& p, int o)
        : Panel (juce::String ("OSC ") + juce::String::charToString ((juce::juce_wchar) ('A' + o))), proc (p), oscIndex (o), display (p.apvts, o),
          wave (p.apvts, o), warpMode (p.apvts, ids::osc (o, "warpMode")), warpSrc (p.apvts, ids::osc (o, "warpSrc")),
          level (p.apvts, ids::osc (o, "level"), "Level"), pan (p.apvts, ids::osc (o, "pan"), "Pan"),
          pitch (p.apvts, ids::osc (o, "pitch"), "Pitch"), fine (p.apvts, ids::osc (o, "fine"), "Fine"), position (p.apvts, ids::osc (o, "position"), "WT Pos"),
          warp (p.apvts, ids::osc (o, "warp"), "Warp"), voices (p.apvts, ids::osc (o, "unison"), "Voices"), spread (p.apvts, ids::osc (o, "spread"), "Spread"),
          detune (p.apvts, ids::osc (o, "detune"), "Detune"), phase (p.apvts, ids::osc (o, "phase"), "Phase")
    {
        display.onLoadFile = [this] (int slot, const juce::File& f) { proc.getUserContent().loadWaveFromFile (slot, f); };
        addPower (p.apvts, ids::osc (o, "on"));
        pencil.onClick = [this] { if (auto* h = findParentComponentOfClass<HarmonicHost>()) h->openHarmonicEditor (oscIndex); };
        addAndMakeVisible (pencil);
        warpSrc.setTooltip ("Source used by the FM / AM / RM warp modes (any oscillator, the sub or the noise)");
        for (juce::Component* c : std::initializer_list<juce::Component*> { &display, &wave, &warpMode, &warpSrc, &level, &pan, &pitch, &fine, &position,
                                                                           &warp, &voices, &spread, &detune, &phase })
            addAndMakeVisible (c);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override { return files.size() > 0 && isAudioFile (files[0]); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        static int rr = 0;
        const int cur = (int) getParamReal (proc.apvts, ids::osc (oscIndex, "wavetable"));
        const int slot = cur >= WavetableBank::kFirstUser ? cur - WavetableBank::kFirstUser : (rr++ % WavetableBank::kUserSlots);
        if (proc.getUserContent().loadWaveFromFile (slot, juce::File (files[0])))
        {
            setParamReal (proc.apvts, ids::osc (oscIndex, "wavetable"), (float) (WavetableBank::kFirstUser + slot));
            ++display.editStamp;
        }
    }

    void resized() override
    {
        placeHeader();
        auto r = content();
        display.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.20f)));
        pencil.setBounds (display.getRight() - 22, display.getY() + 3, 18, 18);
        r.removeFromTop (4);
        wave.setBounds (r.removeFromTop (24));
        r.removeFromTop (3);
        auto combos = r.removeFromTop (24);
        warpMode.setBounds (combos.removeFromLeft ((int) ((float) combos.getWidth() * 0.58f)));
        warpSrc.setBounds (combos.reduced (3, 0));
        r.removeFromTop (3);
        grid (r, 5, 2, { &level, &pan, &pitch, &fine, &position, &warp, &voices, &spread, &detune, &phase });
    }

    NuSynthAudioProcessor& proc;
    int oscIndex;
    WaveDisplay display;
    PencilButton pencil;
    WaveSelector wave;
    ParamCombo warpMode, warpSrc;
    ParamKnob level, pan, pitch, fine, position, warp, voices, spread, detune, phase;
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
        loadButton.setBounds (getWidth() - 50, 5, 42, 17);
        loop.setBounds (getWidth() - 96, 5, 42, 17);
        auto r = content();
        display.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.27f)));
        r.removeFromTop (3);
        selector.setBounds (r.removeFromTop (24));
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
        : editor (a, l), shape (a, ids::lfo (l, "shape")), sync (a, ids::lfo (l, "sync"), "SYNC"), div (a, ids::lfo (l, "div")),
          rate (a, ids::lfo (l, "rate"), "Rate"), depth (a, ids::lfo (l, "depth"), "Depth"),
          phase (a, ids::lfo (l, "phase"), "Phase"), smooth (a, ids::lfo (l, "smooth"), "Smooth")
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &editor, &shape, &sync, &div, &rate, &depth, &phase, &smooth })
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
        shape.setBounds (mid.withTrimmedRight (2));
        row (r, { &rate, &depth, &phase, &smooth });
    }

    LfoEditor editor;
    ParamCombo shape;
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

        button.setColour (juce::TextButton::buttonColourId, colours::accent);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        button.onClick = [this] { generate(); };
        addAndMakeVisible (button);

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
        status.setText ("Designing...", juce::dontSendNotification);
        juce::Component::SafePointer<AiPanel> safe (this);
        proc.getPromptBridge().submit (text, [safe] (bool, const juce::String& msg)
        {
            if (safe == nullptr) return;
            safe->button.setEnabled (true);
            safe->status.setText (msg, juce::dontSendNotification);
        });
    }

    void resized() override
    {
        auto r = content();
        button.setBounds (r.removeFromBottom (26));
        r.removeFromBottom (4);
        status.setBounds (r.removeFromBottom ((int) ((float) r.getHeight() * 0.42f)));
        r.removeFromBottom (3);
        prompt.setBounds (r);
    }

    NuSynthAudioProcessor& proc;
    juce::TextEditor prompt;
    juce::TextButton button { "GENERATE" };
    juce::Label status;
};

// ------------------------------------------------------------------ Mod matrix (scrolls: 16 slots)
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
        view.setSize (std::max (100, viewport.getWidth() - viewport.getScrollBarThickness()), 18 + numRows * 28);
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

inline void swapFxOrder (APVTS& a, int fxIdx, int dir)
{
    std::array<int, kNumFxSlots> ord {};
    for (int i = 0; i < kNumFxSlots; ++i) ord[(size_t) i] = i;
    std::stable_sort (ord.begin(), ord.end(), [&a] (int x, int y) { return getParamReal (a, ids::fx (x, "order")) < getParamReal (a, ids::fx (y, "order")); });
    int pos = 0;
    for (int i = 0; i < kNumFxSlots; ++i) if (ord[(size_t) i] == fxIdx) pos = i;
    const int target = pos + dir;
    if (target < 0 || target >= kNumFxSlots) return;
    std::swap (ord[(size_t) pos], ord[(size_t) target]);
    for (int q = 0; q < kNumFxSlots; ++q) setParamReal (a, ids::fx (ord[(size_t) q], "order"), (float) q);
}

struct FxModule final : juce::Component
{
    FxModule (APVTS& a, int index, std::function<void (int, int)> onMove)
        : idx (index), on (a, ids::fx (index, "on"), fxNames()[index]), up ("UP"), down ("DN"), graph (a, index)
    {
        on.setColour (juce::TextButton::buttonOnColourId, fxColour (index));
        addAndMakeVisible (on); addAndMakeVisible (up); addAndMakeVisible (down); addAndMakeVisible (graph);
        up.onClick   = [this, onMove] { onMove (idx, -1); };
        down.onClick = [this, onMove] { onMove (idx, +1); };

        const auto& specs = fxSpecs (index);
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            const auto& sp = specs[(size_t) k];
            const juce::String id = ids::fxp (index, k);
            if (sp.kind == FxParamSpec::Float)        controls.push_back (std::make_unique<ParamKnob> (a, id, sp.name));
            else if (sp.kind == FxParamSpec::Choice)  controls.push_back (std::make_unique<LabeledCombo> (a, id, sp.name));
            else                                      controls.push_back (std::make_unique<ParamToggle> (a, id, sp.name));
        }
        if (fxHasMix (index)) controls.push_back (std::make_unique<ParamKnob> (a, ids::fx (index, "mix"), "Mix"));
        for (auto& c : controls) addAndMakeVisible (*c);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (fxColour (idx));
        g.fillRoundedRectangle (b.withWidth (5.0f), 2.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b, 6.0f, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto head = r.removeFromTop (24);
        down.setBounds (head.removeFromRight (36).reduced (2, 0));
        up.setBounds (head.removeFromRight (36).reduced (2, 0));
        on.setBounds (head.removeFromLeft (170));
        r.removeFromTop (4);
        graph.setBounds (r.removeFromLeft (juce::jmax (180, (int) ((float) r.getWidth() * 0.34f))));
        r.removeFromLeft (8);

        const int n = (int) controls.size();
        const int cols = n <= 4 ? juce::jmax (1, n) : (n + 1) / 2, rowsN = n <= 4 ? 1 : 2;
        const int cw = r.getWidth() / cols, ch = r.getHeight() / rowsN;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<int> (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch);
            if (dynamic_cast<ParamToggle*> (controls[(size_t) i].get()) != nullptr) cell = centred (cell, cw - 8, 24);
            controls[(size_t) i]->setBounds (cell);
        }
    }

    int idx;
    ParamToggle on;
    juce::TextButton up, down;
    FxGraph graph;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

struct FxRack final : juce::Component, private juce::Timer
{
    static constexpr int kModuleH = 156, kGap = 6;

    explicit FxRack (APVTS& a) : apvts (a)
    {
        for (int i = 0; i < kNumFxSlots; ++i)
        {
            modules[(size_t) i] = std::make_unique<FxModule> (a, i, [this] (int fxIdx, int dir) { swapFxOrder (apvts, fxIdx, dir); layoutModules(); });
            addAndMakeVisible (*modules[(size_t) i]);
        }
        startTimerHz (6);
    }
    ~FxRack() override { stopTimer(); }

    static int totalHeight() { return kNumFxSlots * (kModuleH + kGap); }

    void layoutModules()
    {
        std::array<int, kNumFxSlots> ord {};
        for (int i = 0; i < kNumFxSlots; ++i) ord[(size_t) i] = i;
        std::stable_sort (ord.begin(), ord.end(), [this] (int x, int y) { return getParamReal (apvts, ids::fx (x, "order")) < getParamReal (apvts, ids::fx (y, "order")); });
        int y = 0;
        for (int q = 0; q < kNumFxSlots; ++q)
        {
            modules[(size_t) ord[(size_t) q]]->setBounds (0, y, getWidth(), kModuleH);
            y += kModuleH + kGap;
        }
    }

    void resized() override { layoutModules(); }

    void timerCallback() override
    {
        float sig = 0.0f;
        for (int i = 0; i < kNumFxSlots; ++i) sig += getParamReal (apvts, ids::fx (i, "order")) * (float) (i + 1);
        if (sig != lastSig) { lastSig = sig; layoutModules(); }
    }

    APVTS& apvts;
    std::array<std::unique_ptr<FxModule>, kNumFxSlots> modules;
    float lastSig = -1.0f;
};

struct FxPanel final : Panel
{
    explicit FxPanel (APVTS& a) : Panel ("FX RACK"), rack (a)
    {
        viewport.setViewedComponent (&rack, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void paintExtra (juce::Graphics& g) override
    {
        g.setColour (colours::textDim);
        g.setFont (10.5f);
        g.drawText ("Click an effect name to switch it on  -  UP / DN re-orders the chain  -  EQ: drag the nodes (wheel over the middle node = Q)",
                    getWidth() - 760, 6, 750, 16, juce::Justification::centredRight);
    }

    void resized() override
    {
        viewport.setBounds (content());
        rack.setSize (std::max (200, viewport.getWidth() - viewport.getScrollBarThickness()), FxRack::totalHeight());
    }

    FxRack rack;
    juce::Viewport viewport;
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
        g.drawText ("click a key - drag the slider (-100 / +100 cents) - applies to ALL octaves - double-click a key to reset it",
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
    explicit TopStrip (APVTS& a) : macros (a) { addAndMakeVisible (macros); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (colours::textDim);
        g.setFont (12.0f);
        g.drawText ("Drag a pink key (LFO, ENV, KEY, MOD, M1-M8) onto any knob to modulate it", 14, 0, getWidth() / 2 - 20, getHeight(), juce::Justification::centredLeft);
    }

    void resized() override { macros.setBounds (getLocalBounds().removeFromRight (getWidth() / 2).reduced (14, 2)); }

    MacroBar macros;
};
}
