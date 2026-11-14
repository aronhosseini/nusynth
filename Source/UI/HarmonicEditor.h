#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"
#include "WaveWidgets.h"
#include "Displays.h"

namespace nusynth::ui
{
/** Small icon button for the brush column. */
struct BrushButton final : juce::Button
{
    explicit BrushButton (int iconIndex) : juce::Button ("brush"), icon (iconIndex) { setClickingTogglesState (false); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (getToggleState() ? colours::panelDark.brighter (0.25f) : (over ? colours::panelDark.brighter (0.12f) : colours::panelDark));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (getToggleState() ? colours::accent : colours::outline);
        g.drawRoundedRectangle (b, 3.0f, getToggleState() ? 1.6f : 1.0f);

        auto r = b.reduced (7.0f, 6.0f);
        juce::Path p;
        auto X = [&r] (float t) { return r.getX() + r.getWidth() * t; };
        auto Y = [&r] (float t) { return r.getBottom() - r.getHeight() * t; };
        g.setColour (colours::accent);
        switch (icon)
        {
            case 0:  p.startNewSubPath (X (0), Y (0.5f)); p.lineTo (X (1), Y (0.5f)); break;
            case 1:  for (int i = 0; i <= 20; ++i) { const float t = (float) i / 20.0f; const float y = 0.5f + 0.5f * std::sin (6.2831853f * t); if (i == 0) p.startNewSubPath (X (t), Y (y)); else p.lineTo (X (t), Y (y)); } break;
            case 2:  p.startNewSubPath (X (0), Y (0)); p.lineTo (X (1), Y (1)); break;
            case 3:  p.startNewSubPath (X (0), Y (1)); p.lineTo (X (1), Y (0)); break;
            case 4:  for (int i = 0; i <= 16; ++i) { const float t = (float) i / 16.0f; const float y = t * t; if (i == 0) p.startNewSubPath (X (t), Y (y)); else p.lineTo (X (t), Y (y)); } break;
            case 5:  for (int i = 0; i <= 16; ++i) { const float t = (float) i / 16.0f; const float y = 1.0f - (1.0f - t) * (1.0f - t); if (i == 0) p.startNewSubPath (X (t), Y (y)); else p.lineTo (X (t), Y (y)); } break;
            case 6:  for (int i = 0; i <= 16; ++i) { const float t = (float) i / 16.0f; const float y = std::sin (3.14159f * t); if (i == 0) p.startNewSubPath (X (t), Y (y)); else p.lineTo (X (t), Y (y)); } break;
            case 7:  for (int i = 0; i <= 16; ++i) { const float t = (float) i / 16.0f; const float y = 1.0f - std::sin (3.14159f * t); if (i == 0) p.startNewSubPath (X (t), Y (y)); else p.lineTo (X (t), Y (y)); } break;
            case 8:  for (int i = 0; i < 6; ++i) { const float t0 = (float) i / 6.0f, t1 = (float) (i + 1) / 6.0f, y = t0; if (i == 0) p.startNewSubPath (X (t0), Y (y)); else p.lineTo (X (t0), Y (y)); p.lineTo (X (t1), Y (y)); } break;
            case 9:  for (int i = 0; i < 12; ++i) { const float t = (float) i / 11.0f; g.fillRect (X (t) - 1.0f, Y (t) - 1.0f, 2.0f, 2.0f); } break;
            case 10: p.startNewSubPath (X (0.5f), Y (0.05f)); p.lineTo (X (0.5f), Y (0.95f));
                     p.startNewSubPath (X (0.3f), Y (0.8f)); p.lineTo (X (0.5f), Y (1.0f)); p.lineTo (X (0.7f), Y (0.8f));
                     p.startNewSubPath (X (0.3f), Y (0.2f)); p.lineTo (X (0.5f), Y (0.0f)); p.lineTo (X (0.7f), Y (0.2f)); break;
            default: { juce::Random rr (7); for (int i = 0; i <= 14; ++i) { const float t = (float) i / 14.0f; const float y = 0.5f + (rr.nextFloat() - 0.5f) * 0.9f; if (i == 0) p.startNewSubPath (X (t), Y (y)); else p.lineTo (X (t), Y (y)); } break; }
        }
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    int icon;
};

/** Serum 2 style wavetable / harmonic editor for the User wavetable slots (opened with the pencil next to each oscillator). */
struct HarmonicEditor final : juce::Component, private juce::Timer
{
    static constexpr int kH = 128, kPts = 512, kMaxFrames = WavetableBank::kFrames;

    explicit HarmonicEditor (NuSynthAudioProcessor& p) : proc (p)
    {
        auto add = [this] (juce::TextButton& b, const juce::String& text, std::function<void()> fn)
        {
            b.setButtonText (text);
            b.onClick = std::move (fn);
            addAndMakeVisible (b);
        };
        add (closeBtn, "X", [this] { setVisible (false); });
        add (fftBtn, "UPDATE FFT", [this] { analyzeSel(); repaint(); });
        for (int i = 0; i < 3; ++i)
            add (oscBtn[(size_t) i], juce::String::charToString ((juce::juce_wchar) ('A' + i)), [this, i] { open (i); });
        add (zoomBtn, "4x", [this] { zoomIdx = (zoomIdx + 1) % 4; updateZoomText(); repaint(); });
        add (importBtn, "IMPORT", [this] { importFile(); });
        add (exportBtn, "EXPORT", [this] { exportFile(); });
        add (defPrev, "<", [this] { stepDefault (-1); });
        add (defNext, ">", [this] { stepDefault (+1); });
        add (scopeBtn, "SINGLE", [this] { scopeAll = ! scopeAll; scopeBtn.setButtonText (scopeAll ? "ALL FRAMES" : "SINGLE"); });
        add (partialBtn, "ALL", [this] { partialMode = (partialMode + 1) % 3; partialBtn.setButtonText (partialMode == 0 ? "ALL" : (partialMode == 1 ? "ODD" : "EVEN")); });
        add (morphBtn, "MORPH", [this] { showMorphMenu(); });
        add (toolsBtn, "<-> EDIT", [this] { showToolsMenu(); });
        add (gxUp, "^", [this] { gridX = std::min (32, gridX + 1); updateGridText(); repaint(); });
        add (gxDn, "v", [this] { gridX = std::max (2, gridX - 1); updateGridText(); repaint(); });
        add (gyUp, "^", [this] { gridY = std::min (32, gridY + 1); updateGridText(); repaint(); });
        add (gyDn, "v", [this] { gridY = std::max (2, gridY - 1); updateGridText(); repaint(); });
        add (posMinus, "-", [this] { selectFrame (sel - 1); });
        add (posPlus, "+", [this] { selectFrame (sel + 1); });
        add (copyBtn, "COPY", [this] { if (sel < (int) frames.size()) copyBuf = frames[(size_t) sel]; });
        add (pasteBtn, "PASTE", [this] { if (! copyBuf.empty() && sel < (int) frames.size()) { frames[(size_t) sel] = copyBuf; afterTimeEdit(); } });
        add (removeBtn, "REMOVE", [this] { removeFrame(); });
        add (sortBtn, "SORT", [this] { sortFrames(); });
        add (formulaMenu, "v", [this] { showFormulaMenu(); });

        for (int i = 0; i < 12; ++i)
        {
            brush[(size_t) i] = std::make_unique<BrushButton> (i);
            brush[(size_t) i]->onClick = [this, i] { pickBrush (i); };
            addAndMakeVisible (*brush[(size_t) i]);
        }
        brush[0]->setToggleState (true, juce::dontSendNotification);

        for (auto* l : { &gxLabel, &gyLabel, &posLabel, &defLabel })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::textColourId, colours::text);
            l->setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (*l);
        }
        defLabel.setText ("Default Shapes", juce::dontSendNotification);
        formula.setTextToShowWhenEmpty ("(enter formula, e.g.  sin(x*6.2832)+0.5*sin(x*12.5664) )", colours::textDim);
        formula.onReturnKey = [this] { applyFormula(); };
        addAndMakeVisible (formula);
        updateGridText();
        updateZoomText();
        frames.assign (1, std::vector<float> ((size_t) kPts, 0.0f));
        startTimerHz (25);
    }
    ~HarmonicEditor() override { stopTimer(); }

    // ------------------------------------------------------------ open / data
    void open (int osc)
    {
        oscIndex = osc;
        auto& a = proc.apvts;
        const int cur = (int) getParamReal (a, ids::osc (osc, "wavetable"));
        if (cur >= WavetableBank::kFirstUser) slot = cur - WavetableBank::kFirstUser;
        else
        {
            slot = osc;                                           // OSC A -> User 1, B -> User 2, C -> User 3
            const auto& bank = WavetableBank::get();
            std::vector<std::vector<float>> fr;
            for (int f = 0; f < kMaxFrames; ++f)
            {
                const float* d = bank.frame (std::min (cur, WavetableBank::kProcTables - 1), 0, f);
                std::vector<float> pts ((size_t) kPts);
                for (int i = 0; i < kPts; ++i) pts[(size_t) i] = d[i * WavetableBank::kSize / kPts];
                fr.push_back (std::move (pts));
            }
            WavetableBank::setUserFrames (slot, fr);
            setParamReal (a, ids::osc (osc, "wavetable"), (float) (WavetableBank::kFirstUser + slot));
        }
        frames = WavetableBank::getUserFrames (slot);
        if (frames.empty()) frames.assign (1, std::vector<float> ((size_t) kPts, 0.0f));
        for (auto& f : frames) if ((int) f.size() != kPts) f = resampleTo (f, kPts);
        sel = 0;
        analyzeSel();
        setVisible (true);
        toFront (true);
        repaint();
    }

    static std::vector<float> resampleTo (const std::vector<float>& in, int n)
    {
        std::vector<float> out ((size_t) n, 0.0f);
        if (in.size() < 2) return out;
        for (int i = 0; i < n; ++i)
        {
            const float pos = (float) i * (float) in.size() / (float) n;
            const size_t i0 = (size_t) pos % in.size(), i1 = (i0 + 1) % in.size();
            out[(size_t) i] = in[i0] + (in[i1] - in[i0]) * (pos - std::floor (pos));
        }
        return out;
    }

    void analyzeSel()
    {
        if (sel >= (int) frames.size()) return;
        const auto& pts = frames[(size_t) sel];
        float maxA = 1.0e-6f;
        for (int h = 1; h <= kH; ++h)
        {
            double c = 0.0, s = 0.0;
            for (int n = 0; n < kPts; ++n)
            {
                const double th = 6.283185307179586 * (double) h * (double) n / (double) kPts;
                c += (double) pts[(size_t) n] * std::cos (th);
                s += (double) pts[(size_t) n] * std::sin (th);
            }
            const float A = (float) (2.0 / (double) kPts * std::sqrt (c * c + s * s));
            float phi = (float) std::atan2 (c, s);                     // x = A sin(theta + phi)
            if (phi < 0.0f) phi += 6.2831853f;
            amp[(size_t) h - 1] = A;
            ph[(size_t) h - 1] = phi / 6.2831853f;
            maxA = std::max (maxA, A);
        }
        for (auto& v : amp) v = std::min (1.0f, v / maxA);
    }

    std::vector<float> synthFrom (const std::array<float, kH>& a, const std::array<float, kH>& p) const
    {
        std::vector<float> out ((size_t) kPts, 0.0f);
        float peak = 1.0e-6f;
        for (int n = 0; n < kPts; ++n)
        {
            float v = 0.0f;
            for (int h = 1; h <= kH; ++h)
                if (a[(size_t) h - 1] > 0.0005f)
                    v += a[(size_t) h - 1] * std::sin (6.2831853f * ((float) h * (float) n / (float) kPts + p[(size_t) h - 1]));
            out[(size_t) n] = v;
            peak = std::max (peak, std::abs (v));
        }
        for (auto& v : out) v /= peak;
        return out;
    }

    void markDirty() { dirty = true; }
    void timerCallback() override
    {
        if (! dirty) return;
        dirty = false;
        WavetableBank::setUserFrames (slot, frames);
        if (auto* sd = sharedFor (&proc.apvts)) if (sd->onUiEdit) sd->onUiEdit();
    }
    void afterTimeEdit() { markDirty(); analyzeSel(); repaint(); }

    void selectFrame (int f)
    {
        sel = juce::jlimit (0, (int) frames.size() - 1, f);
        analyzeSel();
        repaint();
    }

    void removeFrame()
    {
        if (frames.size() <= 1) return;
        frames.erase (frames.begin() + sel);
        sel = std::min (sel, (int) frames.size() - 1);
        markDirty(); analyzeSel(); repaint();
    }

    static float centroid (const std::vector<float>& f)
    {
        double num = 0.0, den = 1.0e-9;
        for (int h = 1; h <= 48; ++h)
        {
            double c = 0.0, s = 0.0;
            for (size_t n = 0; n < f.size(); ++n) { const double th = 6.283185307179586 * (double) h * (double) n / (double) f.size(); c += (double) f[n] * std::cos (th); s += (double) f[n] * std::sin (th); }
            const double a = std::sqrt (c * c + s * s);
            num += a * h; den += a;
        }
        return (float) (num / den);
    }
    void sortFrames()
    {
        std::stable_sort (frames.begin(), frames.end(), [] (const std::vector<float>& a, const std::vector<float>& b) { return centroid (a) < centroid (b); });
        markDirty(); analyzeSel(); repaint();
    }

    // ------------------------------------------------------------ default shapes / processing
    static float sawF (float x) { return 2.0f * x - 1.0f; }
    static float triF (float x) { return 1.0f - 4.0f * std::abs (std::fmod (x + 0.25f, 1.0f) - 0.5f); }
    static float sqF (float x, float d) { return x < d ? 1.0f : -1.0f; }

    std::vector<std::vector<float>> makeDefault (int idx) const
    {
        std::vector<std::vector<float>> fr;
        auto gen = [&fr] (auto fn) { std::vector<float> p ((size_t) kPts); for (int n = 0; n < kPts; ++n) p[(size_t) n] = fn ((float) n / (float) kPts); fr.push_back (std::move (p)); };
        constexpr float tp = 6.2831853f;
        switch (idx)
        {
            case 0: gen ([] (float x) { return sawF (x); }); gen ([] (float x) { return std::sin (tp * x); }); gen ([] (float x) { return triF (x); });
                    gen ([] (float x) { return -sawF (x); }); gen ([] (float x) { return sqF (x, 0.5f); }); gen ([] (float x) { return sqF (x, 0.25f); });
                    gen ([] (float x) { return sqF (x, 0.125f); }); gen ([] (float x) { return sqF (x, 0.0625f); }); break;
            case 1: for (int f = 0; f < 8; ++f) { const float m = (float) f / 7.0f; gen ([m] (float x) { return sawF (x) + (sqF (x, 0.5f) - sawF (x)) * m; }); } break;
            case 2: for (int f = 0; f < 8; ++f) { const float m = (float) f / 7.0f; gen ([m, tp] (float x) { return std::sin (tp * x) + (sawF (x) - std::sin (tp * x)) * m; }); } break;
            case 3: for (int f = 0; f < 8; ++f) { const float d = 0.5f - 0.45f * (float) f / 7.0f; gen ([d] (float x) { return sqF (x, d); }); } break;
            case 4: {   // vowels A E I O U (formants as harmonic numbers)
                static const float fm[5][3] = { { 8, 11.5f, 28 }, { 4, 16, 27 }, { 2.7f, 23, 29 }, { 4.5f, 8, 26 }, { 3.2f, 7, 25 } };
                for (int f = 0; f < 8; ++f)
                {
                    const float v = (float) f / 7.0f * 4.0f; const int i0 = std::min (3, (int) v); const float fr0 = v - (float) i0;
                    std::array<float, kH> a {}, p {};
                    for (int h = 1; h <= 64; ++h)
                    {
                        float acc = 0.04f;
                        for (int k = 0; k < 3; ++k)
                        {
                            const float c = fm[i0][k] + (fm[i0 + 1][k] - fm[i0][k]) * fr0, d = ((float) h - c) / (1.3f + 0.4f * (float) k);
                            acc += (k == 0 ? 1.0f : (k == 1 ? 0.7f : 0.4f)) * std::exp (-d * d);
                        }
                        a[(size_t) h - 1] = acc / std::pow ((float) h, 0.6f);
                    }
                    fr.push_back (synthFrom (a, p));
                }
                break; }
            case 5: for (int f = 0; f < 8; ++f) { const float m = (float) f / 7.0f; gen ([m, tp] (float x) { return std::sin (tp * x) + 0.8f * std::sin (2 * tp * x) + (0.2f + 0.7f * m) * std::sin (3 * tp * x) + 0.5f * m * std::sin (4 * tp * x) + 0.3f * std::sin (6 * tp * x); }); } break;
            default: for (int f = 0; f < 8; ++f) { const float g = 1.0f + 5.0f * (float) f / 7.0f; gen ([g] (float x) { float m = (triF (x) * g + 1.0f) * 0.25f; m -= std::floor (m); return 1.0f - std::abs (m * 4.0f - 2.0f); }); } break;
        }
        return fr;
    }

    void stepDefault (int dir)
    {
        defaultIdx = (defaultIdx + dir + 7) % 7;
        static const char* names[7] = { "Default Shapes", "Saw to Square", "Sine to Saw", "Pulse Widths", "Vowels A-E-I-O-U", "Organ Stack", "Triangle Fold" };
        defLabel.setText (names[defaultIdx], juce::dontSendNotification);
        frames = makeDefault (defaultIdx);
        sel = 0;
        markDirty(); analyzeSel(); repaint();
    }

    void processFrame (int op, std::vector<float>& f)
    {
        const int n = (int) f.size();
        switch (op)
        {
            case 1: std::reverse (f.begin(), f.end()); break;
            case 2: for (auto& v : f) v = -v; break;
            case 3: { float pk = 1.0e-5f; for (auto v : f) pk = std::max (pk, std::abs (v)); for (auto& v : f) v /= pk; break; }
            case 4: { auto c = f; for (int i = 0; i < n; ++i) f[(size_t) i] = 0.25f * c[(size_t) ((i + n - 1) % n)] + 0.5f * c[(size_t) i] + 0.25f * c[(size_t) ((i + 1) % n)]; break; }
            case 5: { double m = 0; for (auto v : f) m += v; m /= n; for (auto& v : f) v -= (float) m; break; }
            case 6: for (auto& v : f) v = std::abs (v) * 2.0f - 1.0f; break;
            case 7: for (int i = 0; i < n; ++i) { const float t = (float) i / (float) n; f[(size_t) i] *= std::min (1.0f, std::min (t, 1.0f - t) * 16.0f); } break;
            default: break;
        }
    }
    void runProcess (int op)
    {
        if (scopeAll) for (auto& f : frames) processFrame (op, f);
        else if (sel < (int) frames.size()) processFrame (op, frames[(size_t) sel]);
        afterTimeEdit();
    }

    void showToolsMenu()
    {
        juce::PopupMenu m;
        const char* names[7] = { "Flip horizontally", "Invert", "Normalize", "Smooth", "Remove DC offset", "Full-wave rectify", "Fade edges" };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, names[i]);
        juce::Component::SafePointer<HarmonicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolsBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->runProcess (r); });
    }

    void showMorphMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Morph first -> last frame (waveform)");
        m.addItem (2, "Morph first -> last frame (harmonics)");
        m.addItem (3, "Fill 8 frames from the first one");
        juce::Component::SafePointer<HarmonicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&morphBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->doMorph (r); });
    }
    void doMorph (int mode)
    {
        if (frames.empty()) return;
        const auto first = frames.front(), last = frames.back();
        std::vector<std::vector<float>> out;
        for (int f = 0; f < kMaxFrames; ++f)
        {
            const float m = (float) f / (float) (kMaxFrames - 1);
            std::vector<float> fr ((size_t) kPts);
            for (int n = 0; n < kPts; ++n) fr[(size_t) n] = mode == 3 ? first[(size_t) n] : first[(size_t) n] + (last[(size_t) n] - first[(size_t) n]) * m;
            out.push_back (std::move (fr));
        }
        if (mode == 2)
        {   // interpolate the harmonic amplitudes / phases instead of the samples
            std::array<float, kH> aA {}, pA {}, aB {}, pB {};
            auto grab = [this] (const std::vector<float>& f, std::array<float, kH>& a, std::array<float, kH>& p)
            {
                float mx = 1.0e-6f;
                for (int h = 1; h <= kH; ++h)
                {
                    double c = 0, s = 0;
                    for (int n = 0; n < kPts; ++n) { const double th = 6.283185307179586 * h * n / kPts; c += f[(size_t) n] * std::cos (th); s += f[(size_t) n] * std::sin (th); }
                    a[(size_t) h - 1] = (float) std::sqrt (c * c + s * s);
                    float ph0 = (float) std::atan2 (c, s); if (ph0 < 0) ph0 += 6.2831853f;
                    p[(size_t) h - 1] = ph0 / 6.2831853f;
                    mx = std::max (mx, a[(size_t) h - 1]);
                }
                for (auto& v : a) v /= mx;
            };
            grab (first, aA, pA); grab (last, aB, pB);
            for (int f = 0; f < kMaxFrames; ++f)
            {
                const float m = (float) f / (float) (kMaxFrames - 1);
                std::array<float, kH> a {}, p {};
                for (int h = 0; h < kH; ++h) { a[(size_t) h] = aA[(size_t) h] + (aB[(size_t) h] - aA[(size_t) h]) * m; p[(size_t) h] = pA[(size_t) h] + (pB[(size_t) h] - pA[(size_t) h]) * m; }
                out[(size_t) f] = synthFrom (a, p);
            }
        }
        frames = out;
        sel = 0;
        markDirty(); analyzeSel(); repaint();
    }

    // ------------------------------------------------------------ formula
    struct XScope final : juce::Expression::Scope
    {
        double x = 0.0;
        juce::Expression getSymbolValue (const juce::String& symbol) const override
        {
            if (symbol == "x") return juce::Expression (x);
            if (symbol == "pi") return juce::Expression (juce::MathConstants<double>::pi);
            return juce::Expression::Scope::getSymbolValue (symbol);
        }
    };

    void applyFormula()
    {
        juce::String err;
        juce::Expression e (formula.getText(), err);
        if (err.isNotEmpty()) { formula.setTooltip (err); return; }
        XScope scope;
        auto run = [&] (std::vector<float>& f)
        {
            for (int n = 0; n < kPts; ++n)
            {
                scope.x = (double) n / (double) kPts;
                juce::String ev;
                const double v = e.evaluate (scope, ev);
                if (ev.isNotEmpty()) { formula.setTooltip (ev); return false; }
                f[(size_t) n] = (float) juce::jlimit (-1.0, 1.0, v);
            }
            return true;
        };
        if (scopeAll) { for (auto& f : frames) if (! run (f)) return; }
        else if (sel < (int) frames.size()) { if (! run (frames[(size_t) sel])) return; }
        formula.setTooltip ("ok");
        afterTimeEdit();
    }
    void showFormulaMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "sin(x*6.2832)");
        m.addItem (2, "sin(x*6.2832) + 0.5*sin(x*12.5664) + 0.25*sin(x*18.8496)");
        m.addItem (3, "x*2 - 1");
        m.addItem (4, "sin(x*6.2832*3) * (1 - x)");
        m.addItem (5, "sin(x*6.2832 + sin(x*6.2832*2)*2)");
        juce::Component::SafePointer<HarmonicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&formulaMenu), [safe] (int r)
        {
            static const char* f[5] = { "sin(x*6.2832)", "sin(x*6.2832) + 0.5*sin(x*12.5664) + 0.25*sin(x*18.8496)", "x*2 - 1", "sin(x*6.2832*3) * (1 - x)", "sin(x*6.2832 + sin(x*6.2832*2)*2)" };
            if (safe != nullptr && r > 0) { safe->formula.setText (f[r - 1]); safe->applyFormula(); }
        });
    }

    // ------------------------------------------------------------ file
    void importFile()
    {
        chooser = std::make_shared<juce::FileChooser> ("Import a sample as a wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        juce::Component::SafePointer<HarmonicEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
            if (safe->proc.getUserContent().loadWaveFromFile (safe->slot, fc.getResult()))
            {
                safe->frames = WavetableBank::getUserFrames (safe->slot);
                safe->sel = 0; safe->analyzeSel(); safe->repaint();
            }
        });
    }
    void exportFile()
    {
        chooser = std::make_shared<juce::FileChooser> ("Export wavetable", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("NuSynth wavetable.wav"), "*.wav");
        juce::Component::SafePointer<HarmonicEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult() != juce::File()) safe->proc.getUserContent().exportWave (safe->slot, fc.getResult());
        });
    }

    // ------------------------------------------------------------ layout
    juce::Rectangle<int> barsRect() const   { auto r = body(); r.removeFromLeft (84); r.removeFromRight (84); return r.removeFromTop (topH()).withTrimmedBottom (4); }
    juce::Rectangle<int> body() const       { return getLocalBounds().reduced (8).withTrimmedTop (26); }
    int topH() const                        { return (int) ((float) body().getHeight() * 0.27f); }
    int midH() const                        { return (int) ((float) body().getHeight() * 0.47f); }
    juce::Rectangle<int> midRect() const    { auto r = body(); r.removeFromTop (topH()); return r.removeFromTop (midH()); }
    juce::Rectangle<int> waveRect() const   { auto r = midRect(); r.removeFromLeft (128); r.removeFromRight (158); return r.reduced (6, 0); }
    juce::Rectangle<int> stripRect() const  { auto r = body(); r.removeFromTop (topH() + midH() + 6); return r.removeFromTop ((int) ((float) body().getHeight() * 0.13f)); }
    juce::Rectangle<int> ampRect() const    { auto r = barsRect(); return r.removeFromTop ((int) ((float) r.getHeight() * 0.68f)); }
    juce::Rectangle<int> phaseRect() const  { auto r = barsRect(); r.removeFromTop ((int) ((float) r.getHeight() * 0.68f) + 4); return r; }

    void resized() override
    {
        auto full = getLocalBounds().reduced (8);
        closeBtn.setBounds (full.getRight() - 30, full.getY() - 4, 28, 22);

        auto b = body();
        auto top = b.removeFromTop (topH());
        auto left = top.removeFromLeft (76);
        left.removeFromTop (16);
        for (int i = 0; i < 3; ++i) { oscBtn[(size_t) i].setBounds (left.removeFromTop (20).reduced (14, 0)); left.removeFromTop (3); }
        left.removeFromTop (4);
        fftBtn.setBounds (left.removeFromTop (34));
        auto right = top.removeFromRight (80);
        right.removeFromTop (14);
        zoomBtn.setBounds (right.removeFromTop (22).reduced (8, 0));

        auto mid = b.removeFromTop (midH());
        auto bl = mid.removeFromLeft (122);
        bl.removeFromTop (10);
        const int bw = bl.getWidth() / 2, bh = std::min (36, (bl.getHeight() - 40) / 6);
        for (int i = 0; i < 12; ++i)
            brush[(size_t) i]->setBounds (bl.getX() + (i % 2) * bw, bl.getY() + (i / 2) * (bh + 4), bw - 4, bh);
        auto br = mid.removeFromRight (152);
        auto rr = br.reduced (4, 0);
        auto name = rr.removeFromTop (22);
        defPrev.setBounds (name.removeFromLeft (20)); defNext.setBounds (name.removeFromRight (20)); defLabel.setBounds (name);
        rr.removeFromTop (6);
        auto ie = rr.removeFromTop (26);
        importBtn.setBounds (ie.removeFromLeft (ie.getWidth() / 2 - 2)); ie.removeFromLeft (4); exportBtn.setBounds (ie);
        rr.removeFromTop (18);
        scopeBtn.setBounds (rr.removeFromTop (24)); rr.removeFromTop (4);
        partialBtn.setBounds (rr.removeFromTop (24)); rr.removeFromTop (6);
        morphBtn.setBounds (rr.removeFromTop (24)); rr.removeFromTop (6);
        toolsBtn.setBounds (rr.removeFromTop (24)); rr.removeFromTop (16);
        auto g1 = rr.removeFromTop (24);
        gxUp.setBounds (g1.removeFromLeft (20)); gxLabel.setBounds (g1.removeFromLeft (40)); gxDn.setBounds (g1.removeFromLeft (20));
        auto g2 = rr.removeFromTop (24);
        gyUp.setBounds (g2.removeFromLeft (20)); gyLabel.setBounds (g2.removeFromLeft (40)); gyDn.setBounds (g2.removeFromLeft (20));

        b.removeFromTop (6 + (int) ((float) body().getHeight() * 0.13f));
        auto bot = b.removeFromTop (32);
        posMinus.setBounds (bot.removeFromLeft (26)); posLabel.setBounds (bot.removeFromLeft (80)); posPlus.setBounds (bot.removeFromLeft (26));
        bot.removeFromLeft (14);
        for (auto* bt : { &copyBtn, &pasteBtn, &removeBtn, &sortBtn }) { bt->setBounds (bot.removeFromLeft (82)); bot.removeFromLeft (6); }
        bot.removeFromLeft (10);
        formulaMenu.setBounds (bot.removeFromRight (26));
        formula.setBounds (bot.reduced (0, 2));
    }

    void updateGridText() { gxLabel.setText (juce::String (gridX), juce::dontSendNotification); gyLabel.setText (juce::String (gridY), juce::dontSendNotification); }
    int visibleH() const { static const int z[4] = { 128, 64, 32, 16 }; return z[zoomIdx]; }
    void updateZoomText() { static const char* t[4] = { "1x", "2x", "4x", "8x" }; zoomBtn.setButtonText (t[zoomIdx]); }
    void pickBrush (int i)
    {
        if (i == 10) { runProcess (2); return; }                // invert is an action, not a tool
        tool = i;
        for (int k = 0; k < 12; ++k) brush[(size_t) k]->setToggleState (k == i, juce::dontSendNotification);
    }

    // ------------------------------------------------------------ painting
    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::background);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText ("OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + oscIndex)) + "  -  wavetable editor  (User " + juce::String (slot + 1) + ")",
                    12, 6, 480, 20, juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (11.0f);
        g.drawText ("OSC", 10, 36, 70, 14, juce::Justification::centred);
        g.drawText ("ZOOM", getWidth() - 92, 38 + 22, 80, 12, juce::Justification::centred);

        // ---- harmonics: amplitude (green) on top, phase (grey) below
        auto ar = ampRect().toFloat(), pr = phaseRect().toFloat();
        for (auto* rr : { &ar, &pr }) { g.setColour (colours::panelDark); g.fillRect (*rr); }
        const int vis = visibleH();
        const float bw = ar.getWidth() / (float) vis;
        for (int h = 1; h <= vis; ++h)
        {
            const float x = ar.getX() + (float) (h - 1) * bw;
            const float av = amp[(size_t) h - 1], pv = ph[(size_t) h - 1];
            const bool masked = (partialMode == 1 && h % 2 == 0) || (partialMode == 2 && h % 2 == 1);
            g.setColour (masked ? juce::Colour (0xff3a5a46) : juce::Colour (0xff3ddc6b));
            g.fillRect (x + 0.5f, ar.getBottom() - ar.getHeight() * av, std::max (1.0f, bw - 1.0f), ar.getHeight() * av + 0.5f);
            g.setColour (juce::Colour (0xff8aa9a0));
            g.fillRect (x + 0.5f, pr.getBottom() - pr.getHeight() * std::max (0.08f, pv), std::max (1.0f, bw - 1.0f), pr.getHeight() * std::max (0.08f, pv));
        }

        // ---- big waveform
        auto wr = waveRect().toFloat();
        g.setColour (colours::panelDark.darker (0.2f));
        g.fillRect (wr);
        g.setColour (colours::outline.withAlpha (0.7f));
        for (int i = 1; i < gridX; ++i) g.drawVerticalLine ((int) (wr.getX() + wr.getWidth() * (float) i / (float) gridX), wr.getY(), wr.getBottom());
        for (int i = 1; i < gridY; ++i) g.drawHorizontalLine ((int) (wr.getY() + wr.getHeight() * (float) i / (float) gridY), wr.getX(), wr.getRight());
        g.setColour (colours::outline.brighter (0.4f));
        g.drawHorizontalLine ((int) wr.getCentreY(), wr.getX(), wr.getRight());
        if (sel < (int) frames.size())
        {
            const auto& f = frames[(size_t) sel];
            juce::Path line, fill;
            for (int i = 0; i <= kPts; ++i)
            {
                const float v = f[(size_t) (i % kPts)];
                const float x = wr.getX() + wr.getWidth() * (float) i / (float) kPts, y = wr.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * wr.getHeight() * 0.46f;
                if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, wr.getCentreY()); fill.lineTo (x, y); }
                else { line.lineTo (x, y); fill.lineTo (x, y); }
            }
            fill.lineTo (wr.getRight(), wr.getCentreY()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0x66ff4fd8), 0.0f, wr.getY(), juce::Colour (0x331fb6d8), 0.0f, wr.getBottom(), false));
            g.fillPath (fill);
            g.setColour (juce::Colour (0xffb8f060));
            g.strokePath (line, juce::PathStrokeType (1.8f));
        }

        // ---- frame strip
        auto sr = stripRect();
        g.setColour (colours::panelDark);
        g.fillRect (sr);
        const int cells = kMaxFrames + 1;
        const float cw = (float) sr.getWidth() / (float) cells;
        for (int i = 0; i < cells; ++i)
        {
            auto cell = juce::Rectangle<float> ((float) sr.getX() + (float) i * cw, (float) sr.getY(), cw, (float) sr.getHeight()).reduced (2.0f);
            if (i < (int) frames.size())
            {
                g.setColour (i == sel ? colours::panelDark.brighter (0.3f) : colours::panelDark.brighter (0.1f));
                g.fillRect (cell);
                g.setColour (i == sel ? colours::accent : colours::outline);
                g.drawRect (cell, i == sel ? 2.0f : 1.0f);
                juce::Path p;
                for (int k = 0; k <= 48; ++k)
                {
                    const float v = frames[(size_t) i][(size_t) (k * (kPts - 1) / 48)];
                    const float x = cell.getX() + 3.0f + (cell.getWidth() - 6.0f) * (float) k / 48.0f, y = cell.getCentreY() - 0.34f * cell.getHeight() * v;
                    if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (juce::Colour (0xffb8f060));
                g.strokePath (p, juce::PathStrokeType (1.3f));
                g.setColour (colours::textDim);
                g.setFont (10.0f);
                g.drawText (juce::String (i + 1), cell.toNearestInt().removeFromBottom (12), juce::Justification::centred);
            }
            else if (i == (int) frames.size())
            {
                g.setColour (colours::textDim);
                g.setFont (22.0f);
                g.drawText ("+", cell.toNearestInt(), juce::Justification::centred);
            }
        }
        posLabel.setText (juce::String (sel + 1) + " / " + juce::String ((int) frames.size()), juce::dontSendNotification);
    }

    // ------------------------------------------------------------ mouse
    float valueAt (juce::Point<float> p) const
    {
        const auto wr = waveRect().toFloat();
        return juce::jlimit (-1.0f, 1.0f, (wr.getCentreY() - p.y) / (wr.getHeight() * 0.46f));
    }
    float xNorm (juce::Point<float> p) const
    {
        const auto wr = waveRect().toFloat();
        return juce::jlimit (0.0f, 0.9999f, (p.x - wr.getX()) / wr.getWidth());
    }

    void applyPrimitive (std::vector<float>& f, const std::vector<float>& base, float xa, float ya, float xb, float yb)
    {
        f = base;
        if (xa > xb) { std::swap (xa, xb); std::swap (ya, yb); }
        const int i0 = (int) (xa * kPts), i1 = std::max (i0 + 1, (int) (xb * kPts));
        for (int i = i0; i <= i1 && i < kPts; ++i)
        {
            const float t = (float) (i - i0) / (float) std::max (1, i1 - i0);
            float y = ya;
            switch (tool)
            {
                case 1:  y = ya + (yb - ya) * (0.5f - 0.5f * std::cos (3.14159265f * t)); break;
                case 2:  y = ya + (yb - ya) * t; break;
                case 3:  y = yb + (ya - yb) * t; break;
                case 4:  y = ya + (yb - ya) * t * t; break;
                case 5:  y = ya + (yb - ya) * (1.0f - (1.0f - t) * (1.0f - t)); break;
                case 6:  y = ya + (yb - ya) * std::sin (3.14159265f * t); break;
                case 7:  y = ya - (yb - ya) * std::sin (3.14159265f * t); break;
                case 8:  y = ya + (yb - ya) * std::floor (t * 8.0f) / 8.0f; break;
                default: y = ya + (yb - ya) * std::floor (t * 16.0f) / 16.0f; break;
            }
            f[(size_t) i] = juce::jlimit (-1.0f, 1.0f, y);
        }
    }

    void paintFree (juce::Point<float> pos)
    {
        auto& f = frames[(size_t) sel];
        const int i = juce::jlimit (0, kPts - 1, (int) (xNorm (pos) * (float) kPts));
        const float y = valueAt (pos);
        const int a = lastIdx < 0 ? i : lastIdx, b = i;
        const float ya = lastIdx < 0 ? y : lastY;
        for (int k = std::min (a, b); k <= std::max (a, b); ++k)
        {
            const float t = a == b ? 1.0f : (float) (k - a) / (float) (b - a);
            float v = ya + (y - ya) * t;
            if (tool == 11) v += (rng.nextFloat() - 0.5f) * 0.5f;
            f[(size_t) k] = juce::jlimit (-1.0f, 1.0f, v);
        }
        lastIdx = i; lastY = y;
        markDirty();
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastIdx = -1;
        // frame strip
        if (stripRect().contains (e.getPosition()))
        {
            const float cw = (float) stripRect().getWidth() / (float) (kMaxFrames + 1);
            const int i = (int) ((float) (e.x - stripRect().getX()) / cw);
            if (i < (int) frames.size()) selectFrame (i);
            else if (i == (int) frames.size() && (int) frames.size() < kMaxFrames) { frames.push_back (frames[(size_t) sel]); selectFrame ((int) frames.size() - 1); markDirty(); }
            return;
        }
        // harmonic bars
        if (ampRect().expanded (0, 2).contains (e.getPosition()) || phaseRect().expanded (0, 2).contains (e.getPosition())) { paintBars (e.position); return; }
        // waveform
        if (waveRect().contains (e.getPosition()) && sel < (int) frames.size())
        {
            base = frames[(size_t) sel];
            startPt = e.position;
            drawing = true;
            if (tool == 0 || tool == 11) paintFree (e.position);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drawing && sel < (int) frames.size())
        {
            if (tool == 0 || tool == 11) paintFree (e.position);
            else { applyPrimitive (frames[(size_t) sel], base, xNorm (startPt), valueAt (startPt), xNorm (e.position), valueAt (e.position)); markDirty(); repaint(); }
            return;
        }
        if (barsDrag) paintBars (e.position);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drawing)
        {
            drawing = false;
            if (scopeAll && sel < (int) frames.size()) for (size_t i = 0; i < frames.size(); ++i) if ((int) i != sel) { /* scope ALL only affects process / formula */ }
            afterTimeEdit();
        }
        barsDrag = false; lastBar = 0;
    }

    void paintBars (juce::Point<float> pos)
    {
        barsDrag = true;
        const auto ar = ampRect().toFloat(), pr = phaseRect().toFloat();
        const bool inPhase = pos.y > ar.getBottom() + 1.0f;
        const auto& rr = inPhase ? pr : ar;
        const int vis = visibleH();
        const float bw = ar.getWidth() / (float) vis;
        auto hAt = [&] (float x) { return juce::jlimit (1, vis, 1 + (int) ((x - ar.getX()) / bw)); };
        const float v = juce::jlimit (0.0f, 1.0f, (rr.getBottom() - pos.y) / rr.getHeight());
        int h0 = hAt (pos.x), h1 = lastBar > 0 ? lastBar : h0;
        if (h0 > h1) std::swap (h0, h1);
        for (int h = h0; h <= h1; ++h)
        {
            if ((partialMode == 1 && h % 2 == 0) || (partialMode == 2 && h % 2 == 1)) continue;
            (inPhase ? ph : amp)[(size_t) h - 1] = v;
        }
        lastBar = hAt (pos.x);
        if (sel < (int) frames.size()) { frames[(size_t) sel] = synthFrom (amp, ph); markDirty(); }
        repaint();
    }

    NuSynthAudioProcessor& proc;
    int oscIndex = 0, slot = 0, sel = 0, tool = 0, zoomIdx = 2, partialMode = 0, gridX = 8, gridY = 8, defaultIdx = 0, lastIdx = -1, lastBar = 0;
    float lastY = 0.0f;
    bool scopeAll = false, dirty = false, drawing = false, barsDrag = false;
    juce::Point<float> startPt;
    std::vector<float> base, copyBuf;
    std::vector<std::vector<float>> frames;
    std::array<float, kH> amp {}, ph {};
    juce::Random rng;
    juce::TextButton closeBtn, fftBtn, zoomBtn, importBtn, exportBtn, defPrev, defNext, scopeBtn, partialBtn, morphBtn, toolsBtn,
                     gxUp, gxDn, gyUp, gyDn, posMinus, posPlus, copyBtn, pasteBtn, removeBtn, sortBtn, formulaMenu;
    std::array<juce::TextButton, 3> oscBtn;
    std::array<std::unique_ptr<BrushButton>, 12> brush;
    juce::Label gxLabel, gyLabel, posLabel, defLabel;
    juce::TextEditor formula;
    std::shared_ptr<juce::FileChooser> chooser;
};
}
