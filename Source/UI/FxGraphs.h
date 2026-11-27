#pragma once
#include "Displays.h"
#include "Parameters/FxSpecs.h"
#include "Parameters/ModAssign.h"
#include "DSP/Fx.h"
#include "DSP/Shapers.h"
#include <complex>

namespace nusynth::ui
{
inline juce::Colour fxColour (int fx)
{
    // (no reds / oranges: pink-violet is the modulation colour of the whole plugin)
    static const juce::uint32 c[12] = { 0xff5aa9ff, 0xff9d7bff, 0xff3ddc97, 0xff2bd4d4, 0xffe07be0, 0xffc060e8, 0xfff2c94c, 0xffb58cff, 0xffb0bec5, 0xff7be0ff, 0xff19e3ee, 0xffa6e86a };
    return juce::Colour (c[juce::jlimit (0, 11, fx)]);
}

/** Graphical view of one effect (Serum-style). The EQ view is interactive: drag the three nodes. */
struct FxGraph final : LiveDisplay
{
    /** `slotIdx` = rack slot (type + 12 * copy): it selects the parameters; the effect type selects the drawing. */
    FxGraph (APVTS& a, int slotIdx) : apvts (a), slot (slotIdx), fx (fxTypeOf (slotIdx)), col (fxColour (fxTypeOf (slotIdx)))
    {
        const int n = (int) fxSpecs (fx).size();
        for (int k = 0; k < kMaxFxParams; ++k) p[k] = k < n ? a.getRawParameterValue (ids::fxp (slot, k)) : nullptr;
        mix = a.getRawParameterValue (ids::fx (slot, "mix"));
        on = a.getRawParameterValue (ids::fx (slot, "on"));
    }

    float v (int k) const { return p[k] != nullptr ? p[k]->load() : 0.0f; }
    float hostBpm() const { auto* sd = sharedFor (&apvts); return sd != nullptr ? std::max (20.0f, sd->bpm.load()) : 120.0f; }

    float signature() const override
    {
        float s = mix->load() * 3.1f + on->load() * 17.0f;
        for (int k = 0; k < kMaxFxParams; ++k) s += v (k) * (float) (k + 1) * 0.37f;
        if (fx == 1 && v (5) >= 0.5f) s += hostBpm() * 0.011f;      // tempo-synced delay: redraw when the tempo changes
        return s;
    }

    // ------------------------------------------------------------------ painting
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawDisplayBackground (g, b);
        b = b.reduced (5.0f);
        g.saveState();
        g.reduceClipRegion (getLocalBounds().reduced (2));
        const float alpha = on->load() > 0.5f ? 1.0f : 0.35f;
        g.setOpacity (alpha);

        switch (fx)
        {
            case 0: paintReverb (g, b); break;
            case 1: paintDelay (g, b); break;
            case 2: case 3: paintMod (g, b); break;
            case 4: paintPhaser (g, b); break;
            case 5: paintDist (g, b); break;
            case 6: paintEq (g, b); break;
            case 7: if (v (6) >= 0.5f) paintOtt (g, b); else paintComp (g, b); break;      // Compressor: OTT / Single mode
            case 8: paintLimiter (g, b); break;
            case 10: paintFilterFx (g, b); break;
            default: paintHyper (g, b); break;
        }
        g.restoreState();
    }

    void grid4 (juce::Graphics& g, juce::Rectangle<float> b) const
    {
        g.setColour (colours::outline.withAlpha (0.5f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) i / 4.0f), b.getX(), b.getRight());
    }

    void paintFilterFx (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        const auto names = filterTypeNames();
        const juce::String name = names[juce::jlimit (0, names.size() - 1, (int) v (0))];
        const float fc = v (1), r = v (2);
        std::vector<float> ys (140);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 139.0f);
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, FilterDisplay::magnitude (name, f, fc, r)));
            ys[i] = (db + 36.0f) / 54.0f;
        }
        g.setColour (col);
        drawCurve (g, b, ys);
    }

    void paintReverb (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float size = v (0), damp = v (1), width = v (2), pre = v (3) * 0.001f;
        static const float typeMul[6] = { 1.0f, 0.4f, 0.7f, 1.6f, 1.25f, 1.0f };
        const float t60 = std::min ((0.3f + size * 5.7f) * typeMul[std::min (5, std::max (0, (int) v (6)))], std::max (0.1f, v (7))), tMax = 4.0f;
        std::vector<float> ys (140);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float t = (float) i / 139.0f * tMax;
            float y = 0.0f;
            if (t > pre)
            {
                const float tau = t60 / 6.9f * (1.0f - 0.45f * damp);
                y = std::exp (-(t - pre) / tau) * (0.55f + 0.45f * width);
                y *= std::min (1.0f, (t - pre) / 0.03f);
            }
            ys[i] = 0.05f + 0.9f * y;
        }
        g.setColour (col);
        drawCurve (g, b, ys);
    }

    void paintDelay (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        float tL = std::max (0.001f, v (0) * 0.001f), tR = std::max (0.001f, v (1) * 0.001f);
        const float fb = v (2);
        if (v (5) >= 0.5f)   // tempo sync
        {
            const float bpmNow = hostBpm();
            tL = delaySyncBeats ((int) v (6)) * 60.0f / bpmNow;
            tR = delaySyncBeats ((int) v (7)) * 60.0f / bpmNow;
        }
        const int dtype = (int) v (4);
        const bool ping = dtype == 1;
        const float tMax = 2.0f;
        auto tap = [&] (float t, float amp, bool left)
        {
            const float x = b.getX() + b.getWidth() * t / tMax;
            if (x > b.getRight()) return;
            const float h = b.getHeight() * 0.9f * amp;
            g.setColour (left ? col : colours::orange);
            g.fillRect (x - 1.5f, b.getBottom() - h, 3.0f, h);
        };
        float amp = 1.0f;
        if (ping)
        {
            float t = 0.0f;
            for (int k = 1; k < 40 && amp > 0.03f; ++k)
            {
                const bool left = (k % 2) == 1;
                t += left ? tL : tR;
                tap (t, amp, left);
                amp *= fb;
            }
        }
        else
        {
            for (int k = 1; k < 40 && amp > 0.03f; ++k) { tap ((float) k * tL, amp, true); tap ((float) k * tR, amp * 0.8f, false); amp *= fb; }
        }
        g.setColour (colours::text);
        g.fillRect (b.getX(), b.getBottom() - b.getHeight() * 0.9f, 3.0f, b.getHeight() * 0.9f);   // dry
    }

    void paintMod (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        const float rate = v (0), depth = v (1), width = v (4);
        const float cycles = 1.0f + std::min (5.0f, rate * (fx == 2 ? 2.0f : 4.0f));
        for (int ch = 0; ch < 2; ++ch)
        {
            std::vector<float> ys (140);
            for (size_t i = 0; i < ys.size(); ++i)
            {
                const float ph = (float) i / 139.0f * cycles + (ch == 1 ? width * 0.25f : 0.0f);
                ys[i] = 0.5f + 0.42f * depth * std::sin (6.28318530718f * ph);
            }
            juce::Path line;
            for (size_t i = 0; i < ys.size(); ++i)
            {
                const float x = b.getX() + b.getWidth() * (float) i / 139.0f, y = b.getBottom() - b.getHeight() * ys[i];
                if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
            }
            g.setColour (ch == 0 ? col : colours::orange);
            g.strokePath (line, juce::PathStrokeType (1.8f));
        }
    }

    void paintPhaser (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        static const int st[5] = { 2, 4, 6, 8, 12 };
        const int stages = st[juce::jlimit (0, 4, (int) v (4))];
        const float fc = v (2), depth = v (1);
        // sweep range
        const auto xOf = [&b] (float f) { return b.getX() + b.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
        g.setColour (col.withAlpha (0.18f));
        const float lo = xOf (juce::jlimit (20.0f, 20000.0f, fc * std::pow (2.0f, -2.0f * depth)));
        const float hi = xOf (juce::jlimit (20.0f, 20000.0f, fc * std::pow (2.0f, 2.0f * depth)));
        g.fillRect (lo, b.getY(), std::max (2.0f, hi - lo), b.getHeight());

        std::vector<float> ys (160);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 159.0f);
            const float phi = -2.0f * std::atan (f / std::max (20.0f, fc)) * (float) stages;
            const float mag = std::abs (0.5f * (1.0f + std::polar (1.0f, phi)));
            ys[i] = (20.0f * std::log10 (std::max (mag, 1.0e-3f)) + 40.0f) / 40.0f;
        }
        drawCurve (g, b, ys);
    }

    void paintDist (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const int type = (int) v (0);
        const float gain = std::pow (10.0f, v (1) / 20.0f), levels = std::pow (2.0f, std::max (1.0f, v (3) - 1.0f));
        g.setColour (colours::outline);
        g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());
        g.drawVerticalLine ((int) b.getCentreX(), b.getY(), b.getBottom());
        g.setColour (colours::outline.brighter (0.3f));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);   // identity

        juce::Path line;
        for (int i = 0; i < 160; ++i)
        {
            const float x = -1.0f + 2.0f * (float) i / 159.0f, vv = x * std::min (gain, 12.0f);
            float y;
            if (type == 11) y = std::floor (juce::jlimit (-1.0f, 1.0f, vv) * 6.0f + 0.5f) / 6.0f;
            else            y = distShape (type, vv, levels);
            y = juce::jlimit (-1.0f, 1.0f, y);
            const float px = b.getX() + b.getWidth() * (x * 0.5f + 0.5f), py = b.getCentreY() - y * b.getHeight() * 0.5f;
            if (i == 0) line.startNewSubPath (px, py); else line.lineTo (px, py);
        }
        g.setColour (col);
        g.strokePath (line, juce::PathStrokeType (2.0f));
    }

    // ---- EQ ---------------------------------------------------------------
    static float biquadMag (const Biquad& q, float w)
    {
        const std::complex<float> z1 = std::polar (1.0f, -w), z2 = std::polar (1.0f, -2.0f * w);
        const auto num = q.b0 + q.b1 * z1 + q.b2 * z2;
        const auto den = 1.0f + q.a1 * z1 + q.a2 * z2;
        return std::abs (num / den);
    }

    float xForFreq (juce::Rectangle<float> b, float f) const { return b.getX() + b.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
    float yForGain (juce::Rectangle<float> b, float db) const { return b.getCentreY() - (db / 18.0f) * b.getHeight() * 0.5f; }

    void paintEq (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        const float sr = 48000.0f;
        Biquad lowS, midP, highS;
        lowS.lowShelf (sr, v (0), v (1));
        midP.peak (sr, v (2), v (4), v (3));
        highS.highShelf (sr, v (5), v (6));
        Biquad hp, lpf;
        hp.highpass (sr, v (7), 0.7071f);
        lpf.lowpass (sr, v (8), 0.7071f);

        std::vector<float> ys (200);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 199.0f), w = 6.28318530718f * f / sr;
            const float mag = biquadMag (lowS, w) * biquadMag (midP, w) * biquadMag (highS, w) * biquadMag (hp, w) * biquadMag (lpf, w);
            ys[i] = 0.5f + 0.5f * (20.0f * std::log10 (std::max (mag, 1.0e-3f)) / 18.0f);
        }
        drawCurve (g, b, ys);

        const float xs[5] = { xForFreq (b, v (0)), xForFreq (b, v (2)), xForFreq (b, v (5)), xForFreq (b, v (7)), xForFreq (b, v (8)) };
        const float gs[5] = { v (1), v (3), v (6), -14.0f, -14.0f };      // cut nodes sit on a fixed line (drag sideways)
        for (int i = 0; i < 5; ++i)
        {
            const float y = yForGain (b, gs[i]);
            g.setColour (i == dragNode ? colours::text : col);
            g.fillEllipse (xs[i] - 5.0f, y - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (xs[i] - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.0f);
        }
    }

    int hitNode (juce::Point<float> pt) const
    {
        auto b = getLocalBounds().toFloat().reduced (5.0f);
        const float xs[5] = { xForFreq (b, v (0)), xForFreq (b, v (2)), xForFreq (b, v (5)), xForFreq (b, v (7)), xForFreq (b, v (8)) };
        const float gs[5] = { v (1), v (3), v (6), -14.0f, -14.0f };
        int best = -1; float bestD = 14.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float d = pt.getDistanceFrom ({ xs[i], yForGain (b, gs[i]) });
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void setFxParam (int k, float real)
    {
        auto* par = P (apvts, ids::fxp (slot, k));
        const auto& r = par->getNormalisableRange();
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 (juce::jlimit (r.start, r.end, real)));
        par->endChangeGesture();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (auto* parent = getParentComponent()) parent->mouseDown (e.getEventRelativeTo (parent)); return; }      // right-click = "+ FX" menu
        if (fx == 6) { dragNode = hitNode (e.position); repaint(); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (fx != 6 || dragNode < 0) return;
        auto b = getLocalBounds().toFloat().reduced (5.0f);
        const float fq = 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth()));
        const float gain = juce::jlimit (-18.0f, 18.0f, (b.getCentreY() - e.position.y) / (b.getHeight() * 0.5f) * 18.0f);
        if (dragNode >= 3) { setFxParam (dragNode == 3 ? 7 : 8, fq); return; }       // low cut / high cut: frequency only
        const int kf = dragNode == 0 ? 0 : (dragNode == 1 ? 2 : 5), kg = dragNode == 0 ? 1 : (dragNode == 1 ? 3 : 6);
        setFxParam (kf, fq);
        setFxParam (kg, gain);
    }
    void mouseUp (const juce::MouseEvent&) override { dragNode = -1; repaint(); }
    // ---- OTT / limiter ----------------------------------------------------
    void paintOtt (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float depth = v (0);
        g.setColour (colours::outline.brighter (0.3f));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);
        std::vector<float> ys (160);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float in = -60.0f + 60.0f * (float) i / 159.0f;
            float gdb = 0.0f;
            if (in > -18.0f) gdb = -(in + 18.0f) * 0.75f;
            else if (in < -40.0f) gdb = std::min (24.0f, (-40.0f - in) * 0.667f);
            const float out = in + gdb * depth;
            ys[i] = (juce::jlimit (-60.0f, 0.0f, out) + 60.0f) / 60.0f;
        }
        drawCurve (g, b, ys);
        const float gains[3] = { v (2), v (3), v (4) };       // band gain bars
        const juce::Colour bc[3] = { juce::Colour (0xff3ddc97), juce::Colour (0xfff2c94c), juce::Colour (0xffff7bd0) };
        for (int i = 0; i < 3; ++i)
        {
            const float h = juce::jlimit (-1.0f, 1.0f, gains[i] / 12.0f) * b.getHeight() * 0.25f;
            g.setColour (bc[i]);
            g.fillRect (b.getRight() - 30.0f + (float) i * 9.0f, b.getBottom() - 6.0f - std::max (0.0f, h), 6.0f, std::abs (h) + 2.0f);
        }
    }

    void paintComp (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float thr = v (7), ratio = std::max (1.0f, v (8)), knee = std::max (0.01f, v (11)), gain = v (12);
        g.setColour (colours::outline.brighter (0.3f));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);
        std::vector<float> ys (160);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float in = -60.0f + 60.0f * (float) i / 159.0f, over = in - thr;
            float out;
            if (2.0f * over < -knee) out = in;
            else if (2.0f * std::abs (over) <= knee) { const float t = over + knee * 0.5f; out = in + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee); }
            else out = thr + over / ratio;
            ys[i] = (juce::jlimit (-60.0f, 0.0f, out + gain) + 60.0f) / 60.0f;
        }
        drawCurve (g, b, ys);
        g.setColour (colours::orange);
        const float tx = b.getX() + b.getWidth() * (thr + 60.0f) / 60.0f;
        g.drawVerticalLine ((int) tx, b.getY(), b.getBottom());
    }

    void paintLimiter (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float thr = v (0), gain = v (2), inGain = v (3), ceilDb = std::min (0.0f, v (4));
        g.setColour (colours::outline.brighter (0.3f));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);
        std::vector<float> ys (160);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float in = -24.0f + 30.0f * (float) i / 159.0f;       // -24 .. +6 dB
            const float out = std::min (std::min (in + inGain, thr) + gain, ceilDb);
            ys[i] = (out + 24.0f) / 30.0f;
        }
        drawCurve (g, b, ys);
        g.setColour (colours::orange);
        const float ty = b.getBottom() - b.getHeight() * (thr + 24.0f) / 30.0f;
        g.drawHorizontalLine ((int) ty, b.getX(), b.getRight());
        if (ceilDb < -0.05f)                                              // ceiling: dashed
        {
            const float cy = b.getBottom() - b.getHeight() * (ceilDb + 24.0f) / 30.0f;
            g.setColour (colours::pinkLight.withAlpha (0.7f));
            for (float x = b.getX(); x < b.getRight(); x += 8.0f) g.drawHorizontalLine ((int) cy, x, std::min (b.getRight(), x + 4.0f));
        }
    }

    // ---- Hyper / Dimension: the unison voices (left) and the stereo picture they make (right) ----------------------
    void paintHyper (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float rate = v (0), detune = v (1), width = v (3), hyper = v (4), size = v (5), dimAmt = v (6);
        const int voices = 2 + juce::jlimit (0, 5, (int) v (2));
        auto left = b.withTrimmedRight (b.getWidth() * 0.36f), right = b.withTrimmedLeft (b.getWidth() * 0.66f);

        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawHorizontalLine ((int) left.getCentreY(), left.getX(), left.getRight());
        const float cycles = 1.0f + std::min (4.0f, rate * 1.6f), amp = 0.10f + 0.34f * (0.25f + 0.75f * detune);
        for (int vi = 0; vi < voices; ++vi)
        {
            juce::Path line;
            const float rMul = 1.0f + 0.113f * (float) vi, off = (float) vi / (float) voices;
            for (int i = 0; i < 120; ++i)
            {
                const float t = (float) i / 119.0f;
                const float x = left.getX() + left.getWidth() * t;
                const float y = left.getCentreY() - left.getHeight() * amp * std::sin (6.28318530718f * (cycles * t * rMul + off));
                if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
            }
            g.setColour ((vi & 1 ? colours::orange : col).withAlpha (0.35f + 0.5f * hyper));
            g.strokePath (line, juce::PathStrokeType (1.4f));
        }

        // stereo picture: one dot per voice across the width, plus the two Dimension "lobes"
        const auto c = right.getCentre();
        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawVerticalLine ((int) c.x, right.getY(), right.getBottom());
        const float lobeW = right.getWidth() * (0.26f + 0.34f * size), off = right.getWidth() * 0.12f * dimAmt * (0.4f + size);
        g.setColour (col.withAlpha (0.10f + 0.35f * dimAmt));
        g.fillEllipse (c.x - off - lobeW * 0.5f, c.y - right.getHeight() * 0.30f, lobeW, right.getHeight() * 0.60f);
        g.setColour (colours::orange.withAlpha (0.10f + 0.35f * dimAmt));
        g.fillEllipse (c.x + off - lobeW * 0.5f, c.y - right.getHeight() * 0.30f, lobeW, right.getHeight() * 0.60f);
        for (int vi = 0; vi < voices; ++vi)
        {
            const float pos = voices > 1 ? 2.0f * (float) vi / (float) (voices - 1) - 1.0f : 0.0f;
            const float x = c.x + pos * width * right.getWidth() * 0.42f;
            const float y = c.y + (vi & 1 ? 1.0f : -1.0f) * right.getHeight() * 0.12f;
            g.setColour (colours::text.withAlpha (0.35f + 0.6f * hyper));
            g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }
    }

    APVTS& apvts;
    int slot, fx;
    juce::Colour col;
    std::atomic<float>* p[kMaxFxParams] {};
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* on = nullptr;
    int dragNode = -1;
};

// ------------------------------------------------------------------ live level display of the dynamics effects
/** For Compressor (OTT) / Limiter / Compressor (Analog): scrolling history of the INPUT level (cyan) with the GAIN REDUCTION hanging from the
    top (pink); OTT's upward compression rises from the bottom (green). Bars on the right: IN, OUT and GR, with dB read-outs on top. */
struct FxMeterView final : juce::Component, private juce::Timer
{
    static constexpr int kHist = 120;

    FxMeterView (APVTS& a, int slotIdx)
        : apvts (a), fx (fxTypeOf (slotIdx)), on (a.getRawParameterValue (ids::fx (slotIdx, "on"))),
          modeParam (fxTypeOf (slotIdx) == 7 ? a.getRawParameterValue (ids::fxp (slotIdx, 6)) : nullptr)
    {
        histIn.fill (kFloor); histRed.fill (0.0f); histUp.fill (0.0f);
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }
    ~FxMeterView() override { stopTimer(); }

    static constexpr float kFloor = -72.0f;
    /** meter channel: 0 = Compressor in OTT mode, 1 = Limiter, 2 = Compressor in Single mode */
    int meterIdx() const { return fx == 8 ? 1 : ((modeParam != nullptr && modeParam->load() >= 0.5f) ? 0 : 2); }
    float redScale() const { return fx == 8 ? 12.0f : 24.0f; }                      // dB shown by the full height of the reduction display
    static float toDb (float lin) { return lin > 1.0e-4f ? std::max (kFloor, 20.0f * std::log10 (lin)) : kFloor; }
    static juce::String dbText (float db) { return db <= kFloor + 1.0f ? juce::String ("-inf") : juce::String (db, 1); }

    void timerCallback() override
    {
        float inLin = 0.0f, outLin = 0.0f, red = 0.0f, up = 0.0f;
        if (auto* sd = sharedFor (&apvts))
        {
            auto& m = sd->fxMeter;
            const auto mi = (size_t) meterIdx();
            inLin = m.inPeak[mi].exchange (0.0f);        outLin = m.outPeak[mi].exchange (0.0f);
            red   = m.reduction[mi].exchange (0.0f);     up     = m.boost[mi].exchange (0.0f);
        }
        if (on->load() < 0.5f) { inLin = outLin = red = up = 0.0f; }

        const float inDb = toDb (inLin), outDb = toDb (outLin);
        shownIn = std::max (inDb, shownIn - 2.0f);                   // bars fall back smoothly, history keeps the true peaks
        shownOut = std::max (outDb, shownOut - 2.0f);
        shownRed = std::max (red, shownRed - 0.8f);
        shownUp = std::max (up, shownUp - 0.8f);

        histIn[(size_t) head] = inDb; histRed[(size_t) head] = red; histUp[(size_t) head] = up;
        head = (head + 1) % kHist;

        const bool quiet = inDb <= kFloor + 0.5f && red < 0.01f && up < 0.01f && shownIn <= kFloor + 0.5f && shownRed < 0.01f && shownUp < 0.01f;
        quietTicks = quiet ? quietTicks + 1 : 0;
        if (quietTicks <= kHist + 2) repaint();                       // nothing to draw once the history has scrolled empty
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        drawDisplayBackground (g, b);
        auto area = b.reduced (5.0f, 4.0f);
        const bool act = on->load() > 0.5f;
        const float alpha = act ? 1.0f : 0.45f;

        auto headRow = area.removeFromTop (12.0f);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.setColour (colours::accent.withAlpha (alpha));
        g.drawText ("IN " + dbText (shownIn), headRow.removeFromLeft (headRow.getWidth() * 0.5f), juce::Justification::centredLeft, false);
        g.setColour (colours::pinkLight.withAlpha (alpha));
        g.drawText ("GR " + juce::String (-shownRed, 1), headRow, juce::Justification::centredRight, false);
        area.removeFromTop (2.0f);

        auto bars = area.removeFromRight (38.0f);
        area.removeFromRight (5.0f);
        auto plot = area;
        const float redS = redScale();

        // grid
        g.setColour (colours::outline.withAlpha (0.45f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());

        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        const float colW = plot.getWidth() / (float) (kHist - 1);

        // input level history
        juce::Path inLine;
        for (int i = 0; i < kHist; ++i)
        {
            const float db = histIn[(size_t) ((head + i) % kHist)];
            const float x = plot.getX() + colW * (float) i, y = plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (db - kFloor) / (0.0f - kFloor));
            if (i == 0) inLine.startNewSubPath (x, y); else inLine.lineTo (x, y);
        }
        juce::Path inFill (inLine);
        inFill.lineTo (plot.getRight(), plot.getBottom()); inFill.lineTo (plot.getX(), plot.getBottom()); inFill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.30f * alpha), 0.0f, plot.getY(), colours::accent.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (inFill);
        g.setColour (colours::accent.withAlpha (alpha));
        g.strokePath (inLine, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // gain reduction (hangs from the top) and, for the OTT, upward gain (rises from the bottom)
        auto hanging = [&] (const std::array<float, kHist>& hist, float scale, bool fromTop, juce::Colour c)
        {
            juce::Path line;
            for (int i = 0; i < kHist; ++i)
            {
                const float hgt = plot.getHeight() * juce::jlimit (0.0f, 1.0f, hist[(size_t) ((head + i) % kHist)] / scale);
                const float x = plot.getX() + colW * (float) i, y = fromTop ? plot.getY() + hgt : plot.getBottom() - hgt;
                if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
            }
            juce::Path fill (line);
            const float edgeY = fromTop ? plot.getY() : plot.getBottom();
            fill.lineTo (plot.getRight(), edgeY); fill.lineTo (plot.getX(), edgeY); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.55f * alpha), 0.0f, edgeY, c.withAlpha (0.12f * alpha), 0.0f, fromTop ? plot.getBottom() : plot.getY(), false));
            g.fillPath (fill);
            g.setColour (c.withAlpha (alpha));
            g.strokePath (line, juce::PathStrokeType (1.3f));
        };
        hanging (histRed, redS, true, colours::magenta);
        if (meterIdx() == 0) hanging (histUp, redS, false, juce::Colour (0xff3ddc97));
        g.restoreState();

        // bars: IN, OUT, GR
        auto labelRow = bars.removeFromBottom (10.0f);
        const float bw = (bars.getWidth() - 4.0f) / 3.0f;
        auto bar = [&] (int i, const juce::String& name)
        {
            auto r = juce::Rectangle<float> (bars.getX() + (float) i * (bw + 2.0f), bars.getY(), bw, bars.getHeight());
            g.setColour (juce::Colour (0xff060b11)); g.fillRoundedRectangle (r, 2.0f);
            g.setColour (colours::textDim.withAlpha (alpha)); g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (name, juce::Rectangle<float> (r.getX() - 3.0f, labelRow.getY(), r.getWidth() + 6.0f, labelRow.getHeight()), juce::Justification::centred, false);
            return r;
        };
        auto level = [&] (juce::Rectangle<float> r, float db)
        {
            const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, (db - kFloor) / (0.0f - kFloor));
            juce::ColourGradient gr (colours::hotPink, 0.0f, r.getY(), colours::accent, 0.0f, r.getY() + r.getHeight() * 0.28f, false);
            gr.addColour (1.0, colours::accent.darker (0.4f));
            g.setGradientFill (gr);
            g.setOpacity (alpha);
            g.fillRoundedRectangle (r.withTop (r.getBottom() - h), 2.0f);
            g.setOpacity (1.0f);
        };
        level (bar (0, "IN"), shownIn);
        level (bar (1, "OUT"), shownOut);
        auto grBar = bar (2, "GR");
        const float rh = grBar.getHeight() * juce::jlimit (0.0f, 1.0f, shownRed / redS);
        g.setGradientFill (juce::ColourGradient (colours::magenta.withAlpha (alpha), 0.0f, grBar.getY(), colours::pinkLight.withAlpha (alpha), 0.0f, grBar.getY() + grBar.getHeight(), false));
        g.fillRoundedRectangle (grBar.withHeight (rh), 2.0f);
        if (meterIdx() == 0)
        {
            const float uh = grBar.getHeight() * juce::jlimit (0.0f, 1.0f, shownUp / redS);
            g.setColour (juce::Colour (0xff3ddc97).withAlpha (alpha));
            g.fillRoundedRectangle (grBar.withTop (grBar.getBottom() - uh), 2.0f);
        }
    }

    APVTS& apvts;
    int fx;
    std::atomic<float>* on = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::array<float, kHist> histIn {}, histRed {}, histUp {};
    int head = 0, quietTicks = 0;
    float shownIn = kFloor, shownOut = kFloor, shownRed = 0.0f, shownUp = 0.0f;
};
}
