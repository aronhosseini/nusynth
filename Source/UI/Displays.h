#pragma once
#include "Widgets.h"
#include "Parameters/ParameterLayout.h"
#include "DSP/LfoShape.h"
#include "DSP/Wavetables.h"
#include "DSP/UserContent.h"
#include <functional>
#include <memory>
#include <cmath>
#include <array>
#include <vector>

namespace nusynth::ui
{
inline void drawDisplayBackground (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (colours::panelDark);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
}

/** ys: values 0..1 (0 = bottom). Draws a filled curve. */
inline void drawCurve (juce::Graphics& g, juce::Rectangle<float> b, const std::vector<float>& ys)
{
    if (ys.size() < 2) return;
    juce::Path line;
    for (size_t i = 0; i < ys.size(); ++i)
    {
        const float x = b.getX() + b.getWidth() * (float) i / (float) (ys.size() - 1);
        const float y = b.getBottom() - b.getHeight() * juce::jlimit (0.0f, 1.0f, ys[i]);
        if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    juce::Path fill (line);
    fill.lineTo (b.getRight(), b.getBottom());
    fill.lineTo (b.getX(), b.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.45f), 0.0f, b.getY(),
                                             colours::accent.withAlpha (0.03f), 0.0f, b.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colours::accent);
    g.strokePath (line, juce::PathStrokeType (1.6f));
}

/** Repaints itself only when its signature value changes. */
struct LiveDisplay : juce::Component, private juce::Timer
{
    LiveDisplay() { startTimerHz (20); }
    ~LiveDisplay() override { stopTimer(); }
protected:
    virtual float signature() const = 0;
private:
    void timerCallback() override { const float s = signature(); if (s != last) { last = s; repaint(); } }
    float last = -1.0e9f;
};

// ------------------------------------------------------------------ oscillator waveform (draw on User tables, right-click for more)
struct WaveDisplay final : LiveDisplay
{
    WaveDisplay (APVTS& a, int o)
        : apvts (a), osc (o),
          tbl (a.getRawParameterValue (ids::osc (o, "wavetable"))),
          pos (a.getRawParameterValue (ids::osc (o, "position"))),
          warp (a.getRawParameterValue (ids::osc (o, "warp"))),
          wmode (a.getRawParameterValue (ids::osc (o, "warpMode"))) {}

    float posNow() const { return liveValue (apvts, D_OscA + osc * 8 + OD_Position, pos->load()); }
    float warpNow() const { return liveValue (apvts, D_OscA + osc * 8 + OD_Warp, warp->load()); }
    float signature() const override { return tbl->load() * 1000.0f + posNow() * 100.0f + warpNow() * 10.0f + wmode->load() + (float) editStamp; }

    int tableIndex() const { return std::min (WavetableBank::kTables - 1, std::max (0, (int) tbl->load())); }
    bool isUser() const { return tableIndex() >= WavetableBank::kFirstUser; }
    int userSlot() const { return tableIndex() - WavetableBank::kFirstUser; }

    static float fold (float x) { float m = (x + 1.0f) * 0.25f; m -= std::floor (m); return 1.0f - std::abs (m * 4.0f - 2.0f); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawDisplayBackground (g, b);
        b = b.reduced (4.0f);
        const auto& bank = WavetableBank::get();
        const int t = tableIndex(), mode = (int) wmode->load();
        const float w = warpNow();
        const float frameF = std::min (1.0f, std::max (0.0f, posNow())) * (float) (WavetableBank::kFrames - 1);
        const int f0 = std::min ((int) frameF, WavetableBank::kFrames - 2);
        const float fw = frameF - (float) f0;
        const float* a0 = bank.frame (t, 0, f0);
        const float* a1 = bank.frame (t, 0, f0 + 1);

        std::vector<float> ys (200);
        float peak = 1.0e-4f;
        std::vector<float> vals (ys.size());
        for (size_t i = 0; i < vals.size(); ++i)
        {
            float x = std::fmod ((float) i / 199.0f * 2.0f, 1.0f);    // two cycles
            float rp = x;
            switch (mode)
            {
                case 1:  rp = std::fmod (x * (1.0f + w * 7.0f), 1.0f); break;                       // sync
                case 2:  rp = x + (x * x * (3.0f - 2.0f * x) - x) * w; break;                       // remap
                case 3:  rp = std::pow (x, 1.0f + w * 3.0f); break;                                 // bend +
                case 4:  rp = 1.0f - std::pow (1.0f - x, 1.0f + w * 3.0f); break;                   // bend -
                case 5:  rp = x < 0.5f ? 0.5f * std::pow (2.0f * x, 1.0f + w * 3.0f) : 1.0f - 0.5f * std::pow (2.0f - 2.0f * x, 1.0f + w * 3.0f); break;
                case 6:  { const float d = 0.5f - 0.45f * w; rp = x < d ? 0.5f * x / d : 0.5f + 0.5f * (x - d) / (1.0f - d); break; }   // PWM
                case 11: rp = x + ((x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x) - x) * w; break;         // mirror
                default: break;
            }
            rp = std::min (0.99999f, std::max (0.0f, rp));
            const float idx = rp * (float) WavetableBank::kSize;
            const int ii = (int) idx; const float fr = idx - (float) ii;
            const float va = a0[ii] + (a0[ii + 1] - a0[ii]) * fr, vc = a1[ii] + (a1[ii + 1] - a1[ii]) * fr;
            float val = va + (vc - va) * fw;
            switch (mode)
            {
                case 7:  if (val < 0.0f) val *= 1.0f - 0.9f * w; break;
                case 8:  if (val > 0.0f) val *= 1.0f - 0.9f * w; break;
                case 9:  val = val + 0.5f * w * val * std::abs (val); break;
                case 10: { const float L = (2.0f + (1.0f - w) * 62.0f) * 0.5f; val = std::floor (val * L + 0.5f) / L; break; }
                case 12: val = fold (val * (1.0f + w * 5.0f)); break;
                default: break;
            }
            vals[i] = val;
            peak = std::max (peak, std::abs (val));
        }
        for (size_t i = 0; i < ys.size(); ++i) ys[i] = 0.5f + 0.44f * vals[i] / peak;
        drawCurve (g, b, ys);

        if (isUser())
        {
            g.setColour (colours::text.withAlpha (0.55f));
            g.setFont (9.5f);
            g.drawText ("drag to draw  -  right-click: sample / reset  -  or drop an audio file", getLocalBounds().reduced (6, 2), juce::Justification::topLeft);
        }
        if (mode >= 13)
        {
            g.setColour (colours::orange);
            g.setFont (9.5f);
            g.drawText (juce::String (warpModeNames()[mode]) + " (see source)", getLocalBounds().reduced (6, 2), juce::Justification::bottomRight);
        }
    }

    // ---- drawing on user tables
    void paintPoint (const juce::MouseEvent& e)
    {
        if (! isUser()) return;
        auto frames = WavetableBank::getUserFrames (userSlot());
        if (frames.empty()) return;
        const int fi = frames.size() > 1 ? (int) std::lround (posNow() * (float) (frames.size() - 1)) : 0;
        auto& pts = frames[(size_t) std::min (fi, (int) frames.size() - 1)];
        if (pts.size() < 8) return;
        const auto b = getLocalBounds().toFloat().reduced (4.0f);
        const float xn = juce::jlimit (0.0f, 0.9999f, (e.position.x - b.getX()) / b.getWidth());
        const float yv = juce::jlimit (-1.0f, 1.0f, ((b.getBottom() - e.position.y) / b.getHeight() - 0.5f) / 0.44f);
        const int n = (int) pts.size();
        const int centre = (int) (std::fmod (xn * 2.0f, 1.0f) * (float) n);     // display shows two cycles
        const int half = std::max (1, n / 48);
        for (int k = -half; k <= half; ++k)
        {
            const int i = (centre + k + n) % n;
            const float wgt = 1.0f - std::abs ((float) k) / (float) (half + 1);
            pts[(size_t) i] += (yv - pts[(size_t) i]) * wgt;
        }
        WavetableBank::setUserFrames (userSlot(), frames);
        ++editStamp;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        paintPoint (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) paintPoint (e); }

    void showMenu()
    {
        if (! isUser()) return;
        juce::PopupMenu m;
        m.addItem (1, "Load audio sample into this slot...");
        m.addItem (2, "Reset to sine");
        m.addItem (3, "Smooth");
        m.addItem (4, "Invert");
        m.addItem (5, "Normalize");
        juce::Component::SafePointer<WaveDisplay> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r) { if (safe != nullptr) safe->menuResult (r); });
    }

    void menuResult (int r)
    {
        const int slot = userSlot();
        auto frames = WavetableBank::getUserFrames (slot);
        if (frames.empty()) return;
        const int fi = frames.size() > 1 ? (int) std::lround (posNow() * (float) (frames.size() - 1)) : 0;
        auto& pts = frames[(size_t) std::min (fi, (int) frames.size() - 1)];
        if (pts.size() < 8) return;
        if (r == 1)
        {
            chooser = std::make_shared<juce::FileChooser> ("Choose an audio file (single cycle works best)", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            juce::Component::SafePointer<WaveDisplay> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, slot] (const juce::FileChooser& fc)
            {
                if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
                if (safe->onLoadFile) safe->onLoadFile (slot, fc.getResult());
                ++safe->editStamp;
                safe->repaint();
            });
            return;
        }
        if (r == 2) for (size_t i = 0; i < pts.size(); ++i) pts[i] = std::sin (6.2831853f * (float) i / (float) pts.size());
        if (r == 3) { auto c = pts; for (size_t i = 0; i < pts.size(); ++i) pts[i] = 0.25f * c[(i + pts.size() - 1) % pts.size()] + 0.5f * c[i] + 0.25f * c[(i + 1) % pts.size()]; }
        if (r == 4) for (auto& v : pts) v = -v;
        if (r == 5) { float pk = 1.0e-4f; for (auto v : pts) pk = std::max (pk, std::abs (v)); for (auto& v : pts) v /= pk; }
        WavetableBank::setUserFrames (slot, frames);
        ++editStamp;
        repaint();
    }

    std::function<void (int, const juce::File&)> onLoadFile;     // set by the owner (UserContent)

    APVTS& apvts;
    int osc;
    std::atomic<float> *tbl, *pos, *warp, *wmode;
    int editStamp = 0;
    std::shared_ptr<juce::FileChooser> chooser;
};

// ------------------------------------------------------------------ envelope editor (drag the handles)
struct EnvEditor final : LiveDisplay
{
    EnvEditor (APVTS& a, int e)
        : apvts (a), idx (e),
          A (a.getRawParameterValue (ids::env (e, "attack"))),  D (a.getRawParameterValue (ids::env (e, "decay"))),
          S (a.getRawParameterValue (ids::env (e, "sustain"))), R (a.getRawParameterValue (ids::env (e, "release"))),
          aC (a.getRawParameterValue (ids::env (e, "aCurve"))), dC (a.getRawParameterValue (ids::env (e, "dCurve"))),
          rC (a.getRawParameterValue (ids::env (e, "rCurve"))) {}

    static constexpr float xA1 = 0.25f, xD1 = 0.5f, xS1 = 0.65f, xR1 = 0.95f;
    enum Drag { None, Attack, Decay, Release, ACurve, DCurve, RCurve };

    float signature() const override
    {
        return A->load() * 1000.0f + D->load() * 100.0f + S->load() * 10.0f + R->load() + aC->load() * 3.1f + dC->load() * 5.3f + rC->load() * 7.7f;
    }

    static float timeToFrac (float t) { return std::sqrt (std::min (1.0f, t / 10.0f)); }
    static float fracToTime (float f) { return 10.0f * f * f; }
    static float shape (float t, float c) { const float e = std::pow (3.0f, c); return e == 1.0f ? t : std::pow (t, e); }
    static float curveFromFraction (float frac)
    {
        frac = juce::jlimit (0.02f, 0.98f, frac);
        const float e = std::log (frac) / std::log (0.5f);
        return juce::jlimit (-1.0f, 1.0f, std::log (std::max (e, 0.01f)) / std::log (3.0f));
    }

    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (10.0f, 10.0f); }
    float X (float f) const { const auto a = area(); return a.getX() + f * a.getWidth(); }
    float Y (float l) const { const auto a = area(); return a.getBottom() - l * a.getHeight(); }

    juce::Point<float> hAttack()  const { return { X (timeToFrac (A->load()) * xA1), Y (1.0f) }; }
    juce::Point<float> hDecay()   const { return { X (xA1 + timeToFrac (D->load()) * (xD1 - xA1)), Y (S->load()) }; }
    juce::Point<float> hRelease() const { return { X (xS1 + timeToFrac (R->load()) * (xR1 - xS1)), Y (0.0f) }; }
    juce::Point<float> mAttack()  const { const auto h = hAttack();  return { X (0.0f) + (h.x - X (0.0f)) * 0.5f, Y (shape (0.5f, aC->load())) }; }
    juce::Point<float> mDecay()   const { const auto h = hDecay();   const float s = S->load();
                                          return { hAttack().x + (h.x - hAttack().x) * 0.5f, Y (1.0f - (1.0f - s) * shape (0.5f, dC->load())) }; }
    juce::Point<float> mRelease() const { const auto h = hRelease(); const float s = S->load();
                                          return { X (xS1) + (h.x - X (xS1)) * 0.5f, Y (s * (1.0f - shape (0.5f, rC->load()))) }; }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        drawDisplayBackground (g, full);
        const float s = S->load();
        const auto ha = hAttack(), hd = hDecay(), hr = hRelease();

        juce::Path line;
        line.startNewSubPath (X (0.0f), Y (0.0f));
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            line.lineTo (X (0.0f) + (ha.x - X (0.0f)) * t, Y (shape (t, aC->load())));
        }
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            line.lineTo (ha.x + (hd.x - ha.x) * t, Y (1.0f - (1.0f - s) * shape (t, dC->load())));
        }
        line.lineTo (X (xS1), Y (s));
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            line.lineTo (X (xS1) + (hr.x - X (xS1)) * t, Y (s * (1.0f - shape (t, rC->load()))));
        }

        juce::Path fill (line);
        fill.lineTo (hr.x, Y (0.0f));
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.4f), 0.0f, area().getY(),
                                                 colours::accent.withAlpha (0.03f), 0.0f, area().getBottom(), false));
        g.fillPath (fill);
        g.setColour (colours::accent);
        g.strokePath (line, juce::PathStrokeType (1.8f));

        g.setColour (juce::Colours::white);
        for (auto h : { ha, hd, hr }) g.fillEllipse (h.x - 5.0f, h.y - 5.0f, 10.0f, 10.0f);
        g.setColour (colours::accent.brighter (0.4f));
        for (auto h : { mAttack(), mDecay(), mRelease() }) g.drawEllipse (h.x - 4.0f, h.y - 4.0f, 8.0f, 8.0f, 1.5f);
    }

    Drag hit (juce::Point<float> p) const
    {
        auto closeTo = [&p] (juce::Point<float> h) { return h.getDistanceFrom (p) < 9.0f; };
        if (closeTo (hAttack()))  return Attack;
        if (closeTo (hDecay()))   return Decay;
        if (closeTo (hRelease())) return Release;
        if (closeTo (mAttack()))  return ACurve;
        if (closeTo (mDecay()))   return DCurve;
        if (closeTo (mRelease())) return RCurve;
        return None;
    }

    void mouseDown (const juce::MouseEvent& e) override { dragging = hit (e.position); }
    void mouseUp (const juce::MouseEvent&) override { dragging = None; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto a = area();
        const float W = a.getWidth(), H = a.getHeight();
        const float mx = e.position.x, my = e.position.y;
        const float level = juce::jlimit (0.0f, 1.0f, (a.getBottom() - my) / H);
        const float s = S->load();
        auto setT = [this] (const char* name, float f) { setParamReal (apvts, ids::env (idx, name), juce::jmax (0.0005f, fracToTime (juce::jlimit (0.0f, 1.0f, f)))); };

        switch (dragging)
        {
            case Attack:  setT ("attack", (mx - X (0.0f)) / (W * xA1)); break;
            case Decay:   setT ("decay", (mx - X (xA1)) / (W * (xD1 - xA1))); setParamReal (apvts, ids::env (idx, "sustain"), level); break;
            case Release: setT ("release", (mx - X (xS1)) / (W * (xR1 - xS1))); break;
            case ACurve:  setParamReal (apvts, ids::env (idx, "aCurve"), curveFromFraction (level)); break;
            case DCurve:  setParamReal (apvts, ids::env (idx, "dCurve"), curveFromFraction ((1.0f - level) / std::max (1.0f - s, 0.05f))); break;
            case RCurve:  setParamReal (apvts, ids::env (idx, "rCurve"), curveFromFraction (1.0f - level / std::max (s, 0.05f))); break;
            default: break;
        }
    }

    APVTS& apvts;
    int idx;
    Drag dragging = None;
    std::atomic<float> *A, *D, *S, *R, *aC, *dC, *rC;
};

// ------------------------------------------------------------------ LFO editor (free points: click = add, drag = move, double-click / right-click = delete)
struct LfoEditor final : LiveDisplay
{
    LfoEditor (APVTS& a, int l)
        : apvts (a), idx (l), shapeP (a.getRawParameterValue (ids::lfo (l, "shape"))), shared (sharedFor (&a)) {}

    LfoCustom current() const { return shared != nullptr ? shared->lfo.get (idx) : LfoCustom(); }

    float signature() const override
    {
        const auto c = current();
        float s = shapeP->load() * 100.0f + (float) c.n * 3.0f + (float) editStamp;
        for (int i = 0; i < c.n; ++i) s += c.x[i] * (float) (i + 1) * 1.3f + c.y[i] * (float) (i + 2) * 0.7f;
        return s;
    }

    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (8.0f); }
    juce::Point<float> toScreen (float x, float y) const
    {
        const auto b = area();
        return { b.getX() + b.getWidth() * x, b.getBottom() - b.getHeight() * (0.5f + 0.45f * (y * 2.0f - 1.0f)) };
    }
    void fromScreen (juce::Point<float> p, float& x, float& y) const
    {
        const auto b = area();
        x = juce::jlimit (0.0f, 1.0f, (p.x - b.getX()) / b.getWidth());
        const float v = ((b.getBottom() - p.y) / b.getHeight() - 0.5f) / 0.45f;
        y = juce::jlimit (0.0f, 1.0f, (v + 1.0f) * 0.5f);
    }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        drawDisplayBackground (g, full);
        const auto b = area();
        const auto c = current();
        const int sh = (int) shapeP->load();

        g.setColour (colours::outline);
        g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());
        for (int i = 1; i < 4; ++i) g.drawVerticalLine ((int) (b.getX() + b.getWidth() * (float) i / 4.0f), b.getY(), b.getBottom());

        std::vector<float> ys (161);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float ph = std::min ((float) i / 160.0f, 0.9999f);
            const float hold = std::sin ((float) ((int) (ph * 8.0f) * 12345 % 97));
            ys[i] = 0.5f + 0.45f * lfoWave (sh, ph, c, hold);
        }
        drawCurve (g, b, ys);

        if (sh == 0)
        {
            for (int i = 0; i < c.n; ++i)
            {
                const auto pt = toScreen (c.x[i], c.y[i]);
                g.setColour (i == dragIdx ? colours::orange : juce::Colours::white);
                g.fillEllipse (pt.x - 4.0f, pt.y - 4.0f, 8.0f, 8.0f);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.drawEllipse (pt.x - 4.0f, pt.y - 4.0f, 8.0f, 8.0f, 1.0f);
            }
        }
        g.setColour (colours::textDim);
        g.setFont (9.5f);
        g.drawText (sh == 0 ? "click: add point  -  drag: move  -  double-click: delete" : "click to draw your own shape",
                    getLocalBounds().reduced (8, 3), juce::Justification::bottomRight, true);
    }

    void commit (const LfoCustom& c)
    {
        if (shared == nullptr) return;
        shared->lfo.set (idx, c);
        apvts.state.setProperty ("lfoShape" + juce::String (idx + 1), c.toString(), nullptr);
        if (shared->onUiEdit) shared->onUiEdit();
        ++editStamp;
        repaint();
    }

    void makeCustom()
    {
        const int sh = (int) shapeP->load();
        if (sh == 0) return;
        const auto old = current();
        LfoCustom c;
        c.n = 17;
        for (int i = 0; i < c.n; ++i)
        {
            c.x[i] = (float) i / 16.0f;
            const float hold = std::sin ((float) ((int) (c.x[i] * 8.0f) * 12345 % 97));
            c.y[i] = (lfoWave (sh, std::min (c.x[i], 0.9999f), old, hold) + 1.0f) * 0.5f;
        }
        commit (c);
        setParamReal (apvts, ids::lfo (idx, "shape"), 0.0f);
    }

    int findPoint (juce::Point<float> p, const LfoCustom& c) const
    {
        int best = -1; float bestD = 10.0f;
        for (int i = 0; i < c.n; ++i)
        {
            const float d = p.getDistanceFrom (toScreen (c.x[i], c.y[i]));
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        makeCustom();
        auto c = current();
        const int hit = findPoint (e.position, c);
        if (e.mods.isPopupMenu()) { if (hit >= 0) { c.remove (hit); commit (c); } return; }
        if (hit >= 0) { dragIdx = hit; repaint(); return; }
        float x, y;
        fromScreen (e.position, x, y);
        dragIdx = c.add (juce::jlimit (0.001f, 0.999f, x), y);
        if (dragIdx >= 0) commit (c);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragIdx < 0) return;
        auto c = current();
        if (dragIdx >= c.n) return;
        float x, y;
        fromScreen (e.position, x, y);
        c.y[dragIdx] = y;
        if (dragIdx > 0 && dragIdx < c.n - 1)
            c.x[dragIdx] = juce::jlimit (c.x[dragIdx - 1] + 0.003f, c.x[dragIdx + 1] - 0.003f, x);
        commit (c);
    }

    void mouseUp (const juce::MouseEvent&) override { dragIdx = -1; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto c = current();
        const int hit = findPoint (e.position, c);
        if (hit >= 0) { c.remove (hit); commit (c); }
    }

    APVTS& apvts;
    int idx;
    std::atomic<float>* shapeP;
    SharedData* shared;
    int dragIdx = -1, editStamp = 0;
};

// ------------------------------------------------------------------ filter response
struct FilterDisplay final : LiveDisplay
{
    FilterDisplay (APVTS& a, int f)
        : apvts (a), fi (f), type (a.getRawParameterValue (ids::filter (f, "type"))), cutoff (a.getRawParameterValue (ids::filter (f, "cutoff"))),
          res (a.getRawParameterValue (ids::filter (f, "res"))) {}

    float cutNow() const { return liveValue (apvts, D_F1 + fi * 4 + FD_Cutoff, cutoff->load()); }
    float resNow() const { return liveValue (apvts, D_F1 + fi * 4 + FD_Res, res->load()); }
    float signature() const override { return type->load() * 1.0e5f + cutNow() + resNow() * 10.0f; }

    static float magnitude (const juce::String& name, float f, float fc, float res)
    {
        constexpr float tp = juce::MathConstants<float>::twoPi;
        const float x = f / fc, q = 0.55f + res * res * 12.0f;
        auto lp2 = [q] (float xx) { const float a = 1.0f - xx * xx; return 1.0f / std::sqrt (a * a + (xx / q) * (xx / q)); };
        const bool order4 = name.contains ("24") || name.contains ("Ladder") || name.contains ("Diode");

        if (name.contains ("Comb"))
        {
            const float gain = 0.3f + 0.65f * res, ph = tp * f / fc;
            const float sgn = name.contains ("-") ? -1.0f : 1.0f;
            return std::sqrt (juce::jmax (1.0e-4f, 1.0f + gain * gain + sgn * 2.0f * gain * std::cos (ph))) / (1.0f + gain);
        }
        if (name.contains ("Formant") || name.contains ("Vowel"))
        {
            const float sc = 0.5f + fc / 20000.0f * 1.5f;
            const float cs[3] = { 500.0f * sc, 1500.0f * sc, 2500.0f * sc };
            float m = 0.05f;
            for (float c : cs) { const float l = std::log2 (f / c) * 3.0f; m += std::exp (-l * l); }
            return m;
        }
        if (name.contains ("Phaser"))  return std::sqrt (0.55f + 0.45f * std::cos (4.0f * juce::MathConstants<float>::pi * std::log2 (f / fc)));
        if (name.contains ("Low Shelf"))  return 1.0f + (0.5f + res * 3.0f) / (1.0f + x * x);
        if (name.contains ("High Shelf")) return 1.0f + (0.5f + res * 3.0f) * (x * x) / (1.0f + x * x);

        float m;
        if (name.contains ("High") || name.contains ("HP"))      m = x * x * lp2 (x);
        else if (name.contains ("Band") || name.contains ("BP")) m = (x / q) * lp2 (x);
        else if (name.contains ("Notch"))                        m = std::abs (1.0f - x * x) * lp2 (x);
        else if (name.contains ("Peak"))                         { const float bp = (x / q) * lp2 (x); return std::sqrt (1.0f + q * 0.4f * bp * bp); }
        else                                                     m = lp2 (x);
        return order4 ? m * m : m;
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawDisplayBackground (g, b);
        b = b.reduced (4.0f);
        const auto names = filterTypeNames();
        const juce::String name = names[juce::jlimit (0, names.size() - 1, (int) type->load())];
        const float fc = cutNow(), r = resNow();

        g.setColour (colours::outline.withAlpha (0.6f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = b.getX() + b.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
        }

        std::vector<float> ys (160);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 159.0f);
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, magnitude (name, f, fc, r)));
            ys[i] = (db + 36.0f) / 54.0f;
        }
        drawCurve (g, b, ys);
    }

    APVTS& apvts;
    int fi;
    std::atomic<float> *type, *cutoff, *res;
};

// ------------------------------------------------------------------ noise oscillator view (shows the imported sample waveform)
struct NoiseDisplay final : LiveDisplay
{
    NoiseDisplay (APVTS& a, const NoiseSample& ns)
        : sample (ns), type (a.getRawParameterValue (ids::noiseType)), loop (a.getRawParameterValue (ids::noiseLoop)),
          on (a.getRawParameterValue (ids::noiseOn)) {}

    float signature() const override
    {
        int n = 0; sample.data (n);
        return type->load() * 10.0f + loop->load() + on->load() * 0.5f + (float) n * 0.001f + (float) sample.active.load() * 3.0f;
    }

    static float hashf (int i, int seed)
    {
        unsigned h = (unsigned) i * 747796405u + (unsigned) seed * 2891336453u;
        h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
        return (float) ((h >> 22u) ^ h & 0xFFFFu) / 32768.0f - 1.0f;
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawDisplayBackground (g, b);
        b = b.reduced (4.0f);
        g.saveState();
        g.setOpacity (on->load() > 0.5f ? 1.0f : 0.35f);
        const int t = (int) type->load();
        int n = 0;
        const float* d = sample.data (n);

        g.setColour (colours::outline);
        g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());

        if (t == 5 && d != nullptr && n > 1)
        {
            const int cols = std::max (8, (int) b.getWidth());
            juce::Path top, bottom;
            for (int c = 0; c < cols; ++c)
            {
                const int i0 = (int) ((double) c / cols * n), i1 = std::max (i0 + 1, (int) ((double) (c + 1) / cols * n));
                const int step = std::max (1, (i1 - i0) / 24);
                float lo = 1.0f, hi = -1.0f;
                for (int i = i0; i < i1 && i < n; i += step) { lo = std::min (lo, d[i]); hi = std::max (hi, d[i]); }
                const float x = b.getX() + (float) c;
                g.setColour (colours::accent.withAlpha (0.85f));
                g.drawVerticalLine ((int) x, b.getCentreY() - hi * b.getHeight() * 0.48f, b.getCentreY() - lo * b.getHeight() * 0.48f);
            }
            g.setColour (colours::orange);
            g.setFont (9.5f);
            g.drawText (loop->load() > 0.5f ? "LOOP" : "ONE-SHOT", getLocalBounds().reduced (6, 2), juce::Justification::topRight);
        }
        else if (t == 5)
        {
            g.setColour (colours::textDim);
            g.setFont (10.5f);
            g.drawText ("drop or LOAD an audio sample", getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            // procedural preview of the noise colour
            std::vector<float> ys (160);
            float lp = 0.0f, walk = 0.0f;
            const float a = t == 0 ? 1.0f : (t == 1 ? 0.45f : (t == 3 ? 0.3f : 0.12f));
            for (size_t i = 0; i < ys.size(); ++i)
            {
                float w = hashf ((int) i, 7);
                if (t == 4) w = hashf ((int) i / 6, 11) * 0.8f + 0.2f * w;
                if (t == 2) { walk = walk * 0.97f + w * 0.12f; w = walk * 3.0f; }
                lp += a * (w - lp);
                ys[i] = 0.5f + 0.42f * std::max (-1.0f, std::min (1.0f, lp * (t == 0 ? 1.0f : 1.6f)));
            }
            drawCurve (g, b, ys);
        }
        g.restoreState();
    }

    const NoiseSample& sample;
    std::atomic<float> *type, *loop, *on;
};

// ------------------------------------------------------------------ arpeggiator bars
struct ArpDisplay final : LiveDisplay
{
    explicit ArpDisplay (APVTS& a) : pattern (a.getRawParameterValue (ids::arpPattern)), octaves (a.getRawParameterValue (ids::arpOctaves)),
                                     on (a.getRawParameterValue (ids::arpOn)) {}

    float signature() const override { return pattern->load() * 10.0f + octaves->load() + on->load() * 100.0f; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawDisplayBackground (g, b);
        b = b.reduced (6.0f);
        const int pat = (int) pattern->load(), oct = (int) octaves->load();
        const int n = 16;
        const float bw = b.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            float h;
            switch (pat)
            {
                case 0:  h = (float) (i % 8) / 7.0f; break;
                case 1:  h = 1.0f - (float) (i % 8) / 7.0f; break;
                case 2:  h = 1.0f - std::abs (((float) (i % 14) - 7.0f) / 7.0f); break;
                case 3:  h = std::abs (((float) (i % 14) - 7.0f) / 7.0f); break;
                case 4:  h = 0.25f + 0.75f * std::abs (std::sin ((float) i * 12.9898f)); break;
                case 7:  h = (i % 2 == 0) ? (float) (i / 2 % 8) / 7.0f : 1.0f - (float) (i / 2 % 8) / 7.0f; break;
                case 8:  h = 0.5f + 0.5f * (float) ((i % 2) ? 1 : -1) * (float) ((i / 2) % 8) / 7.0f; break;
                case 9:  h = (i % 2) ? 1.0f : (float) (i / 2 % 7) / 7.0f; break;
                case 10: h = (i % 2) ? 0.1f : 0.2f + (float) (i / 2 % 7) / 9.0f; break;
                case 11: h = (float) ((i / 2) % 8) / 7.0f; break;
                case 12: h = 1.0f - (float) ((i / 2) % 8) / 7.0f; break;
                case 13: h = (float) (((i / 2) + (i % 2) * 2) % 8) / 7.0f; break;
                case 14: h = 0.5f + 0.4f * std::sin ((float) i * 0.8f); break;
                case 15: h = (i % 2) ? 0.95f : 0.4f + 0.3f * (float) (i / 2 % 4) / 3.0f; break;
                case 16: h = (float) ((i + i / 8) % 8) / 7.0f; break;
                default: h = (i % 4 == 0) ? 0.9f : 0.55f; break;
            }
            h = 0.12f + 0.88f * h * (0.55f + 0.15f * (float) oct);
            h = juce::jlimit (0.1f, 1.0f, h);
            auto bar = juce::Rectangle<float> (b.getX() + (float) i * bw + 1.0f, b.getBottom() - h * b.getHeight(), bw - 2.0f, h * b.getHeight());
            g.setColour (on->load() > 0.5f ? colours::accent : colours::accentDim);
            g.fillRoundedRectangle (bar, 1.5f);
        }
    }

    std::atomic<float> *pattern, *octaves, *on;
};

// ------------------------------------------------------------------ per-key microtuning keyboard
struct MicroKeyboard final : juce::Component, private juce::Timer
{
    explicit MicroKeyboard (APVTS& a) : apvts (a)
    {
        for (int i = 0; i < kNumMicroKeys; ++i) raw[(size_t) i] = a.getRawParameterValue (ids::micro (i));
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (slider);
        selectKey (0);
        startTimerHz (15);
    }
    ~MicroKeyboard() override { stopTimer(); }

    void selectKey (int k)
    {
        selected = k;
        attach.reset();
        attach = std::make_unique<juce::SliderParameterAttachment> (*P (apvts, ids::micro (k)), slider, nullptr);
        repaint();
    }

    void resetAll()
    {
        for (int i = 0; i < kNumMicroKeys; ++i)
        {
            auto* p = P (apvts, ids::micro (i));
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
            p->endChangeGesture();
        }
    }

    static bool isBlack (int n) { return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }

    juce::Rectangle<float> keyRect (int note) const
    {
        static const int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
        auto kr = keyArea().toFloat();
        const float ww = kr.getWidth() / 7.0f;
        if (! isBlack (note)) return { kr.getX() + (float) whiteIndex[note] * ww, kr.getY(), ww, kr.getHeight() };
        static const int boundary[12] = { 0, 1, 0, 2, 0, 0, 4, 0, 5, 0, 6, 0 };
        const float bw = ww * 0.62f;
        return { kr.getX() + (float) boundary[note] * ww - bw * 0.5f, kr.getY(), bw, kr.getHeight() * 0.62f };
    }

    juce::Rectangle<int> keyArea() const { return getLocalBounds().withTrimmedTop (34); }

    void resized() override { slider.setBounds (0, 12, getWidth(), 22); }

    void paint (juce::Graphics& g) override
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        g.setColour (colours::textDim);
        g.setFont (10.0f);
        const char* ticks[5] = { "-100", "-50", "0", "+50", "+100" };
        for (int i = 0; i < 5; ++i)
            g.drawText (ticks[i], (int) ((float) (getWidth() - 40) * (float) i / 4.0f), 0, 40, 12, juce::Justification::centred);

        auto draw = [&] (int n)
        {
            auto r = keyRect (n);
            const bool black = isBlack (n), sel = (n == selected);
            juce::Colour fill = black ? colours::background : colours::keyWhite;
            if (sel) fill = black ? colours::accentDim : colours::accent.interpolatedWith (juce::Colours::white, 0.45f);
            g.setColour (fill);
            g.fillRoundedRectangle (r.reduced (0.5f), 3.0f);
            g.setColour (colours::outline);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

            const float c = raw[(size_t) n]->load();
            if (std::abs (c) >= 0.5f)
            {
                g.setColour (black ? colours::accent : colours::accentDim);
                g.setFont (10.0f);
                g.drawText ((c > 0 ? "+" : "") + juce::String ((int) std::round (c)), r.withHeight (14.0f).translated (0, 3), juce::Justification::centred);
            }
            if (! black)
            {
                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.setFont (11.0f);
                g.drawText (names[n], r.removeFromBottom (18.0f), juce::Justification::centred);
            }
        };
        for (int n = 0; n < 12; ++n) if (! isBlack (n)) draw (n);
        for (int n = 0; n < 12; ++n) if (isBlack (n))  draw (n);
    }

    int keyAt (juce::Point<int> p) const
    {
        for (int n = 0; n < 12; ++n) if (isBlack (n) && keyRect (n).contains (p.toFloat())) return n;
        for (int n = 0; n < 12; ++n) if (! isBlack (n) && keyRect (n).contains (p.toFloat())) return n;
        return -1;
    }

    void mouseDown (const juce::MouseEvent& e) override { const int k = keyAt (e.getPosition()); if (k >= 0) selectKey (k); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int k = keyAt (e.getPosition());
        if (k < 0) return;
        auto* p = P (apvts, ids::micro (k));
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        p->endChangeGesture();
    }

    void timerCallback() override
    {
        float sum = 0.0f;
        for (auto* r : raw) sum += r->load() * 1.37f + sum * 0.01f;
        if (sum != lastSum) { lastSum = sum; repaint(); }
    }

    APVTS& apvts;
    std::array<std::atomic<float>*, kNumMicroKeys> raw {};
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attach;
    int selected = 0;
    float lastSum = -1.0e9f;
};

// ------------------------------------------------------------------ mod matrix table
struct ModMatrixView final : juce::Component
{
    struct Row
    {
        std::unique_ptr<ParamCombo> src, dst, type;
        juce::Slider amount, curve;
        std::unique_ptr<juce::SliderParameterAttachment> amountAtt, curveAtt;
    };

    ModMatrixView (APVTS& a, int numRows, bool withCurve) : showCurve (withCurve)
    {
        for (int r = 0; r < numRows; ++r)
        {
            auto row = std::make_unique<Row>();
            row->src  = std::make_unique<ParamCombo> (a, ids::mod (r, "src"));
            row->dst  = std::make_unique<ParamCombo> (a, ids::mod (r, "dst"));
            row->type = std::make_unique<ParamCombo> (a, ids::mod (r, "type"));
            bar (row->amount);
            row->amountAtt = std::make_unique<juce::SliderParameterAttachment> (*P (a, ids::mod (r, "amount")), row->amount, nullptr);
            addAndMakeVisible (*row->src); addAndMakeVisible (*row->dst); addAndMakeVisible (*row->type); addAndMakeVisible (row->amount);
            if (showCurve)
            {
                bar (row->curve);
                row->curveAtt = std::make_unique<juce::SliderParameterAttachment> (*P (a, ids::mod (r, "curve")), row->curve, nullptr);
                addAndMakeVisible (row->curve);
            }
            rowsV.push_back (std::move (row));
        }
    }

    static void bar (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 60, 18);
        s.setColour (juce::Slider::trackColourId, colours::accentDim);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    std::vector<float> fractions() const
    {
        return showCurve ? std::vector<float> { 0.22f, 0.28f, 0.20f, 0.14f, 0.16f }
                         : std::vector<float> { 0.26f, 0.32f, 0.24f, 0.18f };
    }

    void paint (juce::Graphics& g) override
    {
        const char* heads[5] = { "Sources", "Destinations", "Amount", "Curve", "Type" };
        g.setColour (colours::textDim);
        g.setFont (11.0f);
        const auto f = fractions();
        float x = 0.0f;
        for (size_t i = 0; i < f.size(); ++i)
        {
            const char* h = showCurve ? heads[i] : (i < 3 ? heads[i] : heads[4]);
            g.drawText (h, (int) x + 4, 0, (int) (f[i] * (float) getWidth()), 16, juce::Justification::centredLeft);
            x += f[i] * (float) getWidth();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (18);
        const int n = (int) rowsV.size();
        if (n == 0) return;
        const int rh = std::min (30, r.getHeight() / n);
        const auto f = fractions();
        for (auto& row : rowsV)
        {
            auto rr = r.removeFromTop (rh).reduced (0, 2);
            const int W = rr.getWidth();
            auto take = [&] (float fr) { return rr.removeFromLeft ((int) ((float) W * fr)).reduced (2, 0); };
            row->src->setBounds (take (f[0]));
            row->dst->setBounds (take (f[1]));
            row->amount.setBounds (take (f[2]));
            if (showCurve) row->curve.setBounds (take (f[3]));
            row->type->setBounds (rr.reduced (2, 0));
        }
    }

    bool showCurve;
    std::vector<std::unique_ptr<Row>> rowsV;
};
}
