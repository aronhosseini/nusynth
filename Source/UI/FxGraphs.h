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
    static const juce::uint32 c[9] = { 0xff5aa9ff, 0xff9d7bff, 0xff3ddc97, 0xff2bd4d4, 0xffff7bd0, 0xffff6b4a, 0xfff2c94c, 0xffff9f43, 0xffb0bec5 };
    return juce::Colour (c[juce::jlimit (0, 8, fx)]);
}

/** Graphical view of one effect (Serum-style). The EQ view is interactive: drag the three nodes. */
struct FxGraph final : LiveDisplay
{
    FxGraph (APVTS& a, int effect) : apvts (a), fx (effect), col (fxColour (effect))
    {
        const int n = (int) fxSpecs (fx).size();
        for (int k = 0; k < kMaxFxParams; ++k) p[k] = k < n ? a.getRawParameterValue (ids::fxp (fx, k)) : nullptr;
        mix = a.getRawParameterValue (ids::fx (fx, "mix"));
        on = a.getRawParameterValue (ids::fx (fx, "on"));
    }

    float v (int k) const { return p[k] != nullptr ? p[k]->load() : 0.0f; }

    float signature() const override
    {
        float s = mix->load() * 3.1f + on->load() * 17.0f;
        for (int k = 0; k < kMaxFxParams; ++k) s += v (k) * (float) (k + 1) * 0.37f;
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
            case 7: paintOtt (g, b); break;
            default: paintLimiter (g, b); break;
        }
        g.restoreState();
    }

    void grid4 (juce::Graphics& g, juce::Rectangle<float> b) const
    {
        g.setColour (colours::outline.withAlpha (0.5f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) i / 4.0f), b.getX(), b.getRight());
    }

    void paintReverb (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        const float size = v (0), damp = v (1), width = v (2), pre = v (3) * 0.001f;
        const float t60 = 0.3f + size * 5.7f, tMax = 4.0f;
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
        // early reflections
        static const float er[6] = { 0.011f, 0.023f, 0.034f, 0.047f, 0.061f, 0.079f };
        g.setColour (col.brighter (0.5f));
        for (int i = 0; i < 6; ++i)
        {
            const float x = b.getX() + b.getWidth() * (pre + er[i]) / tMax;
            g.drawVerticalLine ((int) x, b.getBottom() - b.getHeight() * (0.85f - 0.1f * (float) i), b.getBottom());
        }
    }

    void paintDelay (juce::Graphics& g, juce::Rectangle<float> b)
    {
        grid4 (g, b);
        const float tL = std::max (0.001f, v (0) * 0.001f), tR = std::max (0.001f, v (1) * 0.001f), fb = v (2);
        const bool ping = v (4) > 0.5f;
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
        auto* par = P (apvts, ids::fxp (fx, k));
        const auto& r = par->getNormalisableRange();
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 (juce::jlimit (r.start, r.end, real)));
        par->endChangeGesture();
    }

    void mouseDown (const juce::MouseEvent& e) override { if (fx == 6) { dragNode = hitNode (e.position); repaint(); } }
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
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (fx != 6) return;
        const int n = hitNode (e.position);
        if (n == 1) setFxParam (4, v (4) * std::pow (1.25f, w.deltaY * 4.0f));     // wheel over the mid node = Q
    }

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

    void paintLimiter (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float thr = v (0);
        g.setColour (colours::outline.brighter (0.3f));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);
        std::vector<float> ys (160);
        for (size_t i = 0; i < ys.size(); ++i)
        {
            const float in = -24.0f + 30.0f * (float) i / 159.0f;       // -24 .. +6 dB
            ys[i] = (std::min (in, thr) + 24.0f) / 30.0f;
        }
        drawCurve (g, b, ys);
        g.setColour (colours::orange);
        const float ty = b.getBottom() - b.getHeight() * (thr + 24.0f) / 30.0f;
        g.drawHorizontalLine ((int) ty, b.getX(), b.getRight());
    }

    APVTS& apvts;
    int fx;
    juce::Colour col;
    std::atomic<float>* p[kMaxFxParams] {};
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* on = nullptr;
    int dragNode = -1;
};
}
