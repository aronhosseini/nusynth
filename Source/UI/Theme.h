#pragma once
#include <juce_graphics/juce_graphics.h>
#include <map>
#include <algorithm>
#include <cmath>

// NuSynth visual theme - dark slate panels, neon cyan accents, magenta modulation badges.
namespace nusynth::colours
{
    inline const juce::Colour background { 0xff0b0f15 };
    inline const juce::Colour panel      { 0xff161d27 };
    inline const juce::Colour panelTop   { 0xff1d2532 };
    inline const juce::Colour panelDark  { 0xff0e131b };
    inline const juce::Colour outline    { 0xff2d3a4d };
    inline const juce::Colour border     { 0xff394a63 };
    inline const juce::Colour accent     { 0xff00f2fe };      // neon cyan
    inline const juce::Colour accentDim  { 0xff0b4d57 };      // teal (active button fill)
    inline const juce::Colour text       { 0xffdbe5f0 };
    inline const juce::Colour textDim    { 0xff8191a6 };
    inline const juce::Colour orange     { 0xffd45cd6 };      // modulation pink-violet (name kept for existing code)
    inline const juce::Colour magenta    { 0xffd45cd6 };      // pink / violet: modulation rings, draggable keys, box edges
    inline const juce::Colour pinkLight  { 0xffee93ea };      // text on the pink keys
    inline const juce::Colour pinkDeep   { 0xff3a1a45 };      // dark violet fill behind the pink keys
    inline const juce::Colour hotPink    { 0xfff04ad8 };      // GENERATE / velocity end colour (same as the reference)
    inline const juce::Colour keyWhite   { 0xffe8edf2 };
}

namespace nusynth::gfx
{
    /** Soft neon stroke: two translucent halos behind a crisp line. */
    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
    {
        const auto st = [w] (float extra) { return juce::PathStrokeType (w + extra, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (c.withAlpha (0.08f)); g.strokePath (p, st (6.0f));
        g.setColour (c.withAlpha (0.16f)); g.strokePath (p, st (3.0f));
        g.setColour (c);                   g.strokePath (p, st (0.0f));
    }

    /** Rounded outline with a faint outer glow. */
    inline void neonOutline (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float corner, float strength = 1.0f)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (c.withAlpha (0.06f * strength * (float) (4 - i)));
            g.drawRoundedRectangle (r.expanded ((float) i * 1.4f), corner + (float) i * 1.4f, 1.6f);
        }
        g.setColour (c.withAlpha (0.9f * strength));
        g.drawRoundedRectangle (r, corner, 1.2f);
    }

    /** Glassy slate panel exactly like the reference design: a light-blue (teal) glow in the top-left corner fading through
        dark slate into a violet-pink glow in the bottom-right, with a cyan -> pink gradient edge.
        `strength` 1 = full panel; ~0.35 = the darker information strips. */
    inline void panelBackground (juce::Graphics& g, juce::Rectangle<float> b, float corner = 8.0f, float strength = 1.0f)
    {
        const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
        const juce::Colour slateTop (0xff2a313c), slateBot (0xff242a33);
        const juce::Colour dark (0xff181d24);
        g.setGradientFill (juce::ColourGradient (slateTop.interpolatedWith (dark, 1.0f - strength), x, y,
                                                 slateBot.interpolatedWith (dark, 1.0f - strength), x, b.getBottom(), false));
        g.fillRoundedRectangle (b, corner);

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (b, corner);
        g.reduceClipRegion (clip, juce::AffineTransform());

        // measured from the reference: the teal glow fades out ~250 px from the top-left corner, the violet one ~130 px from the bottom-right
        const juce::Colour teal (0xff2f6c78), violet (0xff3f2e44);
        {
            const float rad = juce::jmax (60.0f, 0.72f * juce::jmax (w, h));
            juce::ColourGradient tg (teal.withAlpha (0.90f * strength), x, y, teal.withAlpha (0.0f), x + rad, y, true);
            tg.addColour (0.42, teal.withAlpha (0.45f * strength));
            tg.addColour (0.72, teal.withAlpha (0.18f * strength));
            g.setGradientFill (tg);
            g.fillRect (b);
        }
        {
            const float rad = juce::jmax (60.0f, 0.5f * w, 0.34f * h);
            juce::ColourGradient vg (violet.withAlpha (0.90f * strength), b.getRight(), b.getBottom(), violet.withAlpha (0.0f), b.getRight() - rad, b.getBottom(), true);
            vg.addColour (0.45, violet.withAlpha (0.50f * strength));
            g.setGradientFill (vg);
            g.fillRect (b);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f * strength), x, y,                // glass sheen along the top edge
                                                 juce::Colours::white.withAlpha (0.0f), x, y + juce::jmin (26.0f, h * 0.3f), false));
        g.fillRect (b);
        g.restoreState();

        // edge: soft glow, then a cyan -> violet-pink line
        const juce::Colour edgeA (0xff59d0dc), edgeMid (0xff7d86a0), edgeB (0xffcc58d0);
        auto edge = [&] (float alpha)
        {
            juce::ColourGradient gr (edgeA.withAlpha (alpha), x, y, edgeB.withAlpha (alpha), b.getRight(), b.getBottom(), false);
            gr.addColour (0.5, edgeMid.withAlpha (alpha * 0.55f));
            g.setGradientFill (gr);
        };
        edge (0.10f); g.drawRoundedRectangle (b.expanded (1.5f), corner + 1.5f, 3.0f);
        edge (0.85f); g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.4f);

        g.setColour (juce::Colours::black.withAlpha (0.30f));                 // inner shadow line
        g.drawRoundedRectangle (b.reduced (1.7f), corner - 1.0f, 1.0f);
    }

    /** Small dark inset field (used for displays, pickers). */
    inline void insetField (juce::Graphics& g, juce::Rectangle<float> b, float corner = 5.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a1520), b.getX(), b.getY(),
                                                 juce::Colour (0xff0c1822), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, corner);
        g.setColour (juce::Colour (0xff2a3b4d));
        g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);
    }

    inline void chevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        juce::Path p;
        const auto cx = r.getCentreX(), cy = r.getCentreY(), s = juce::jmin (r.getWidth(), r.getHeight()) * 0.22f;
        p.startNewSubPath (cx - s, cy - s * 0.5f);
        p.lineTo (cx, cy + s * 0.6f);
        p.lineTo (cx + s, cy - s * 0.5f);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /** Brushed / turned-aluminium knob cap (cached per pixel size). The "conic" highlights are painted as thin wedges, then
        concentric grooves, a soft specular and a darker rim are layered on top - like the reference knobs. */
    /** The cache lives in a function-local static; it is emptied when the editor closes (see clearImageCaches) so no juce::Image
        is still alive when the host unloads the plug-in DLL (images destroyed after JUCE shut down can hang / crash the host). */
    inline std::map<int, juce::Image>& metalCapCache() { static std::map<int, juce::Image> cache; return cache; }
    inline void clearImageCaches() { metalCapCache().clear(); }

    inline juce::Image& metalCapImage (int px)
    {
        auto& cache = metalCapCache();
        px = juce::jlimit (8, 384, px);
        auto it = cache.find (px);
        if (it != cache.end()) return it->second;

        juce::Image img (juce::Image::ARGB, px, px, true);
        juce::Graphics g (img);
        const float R = (float) px * 0.5f;
        const juce::Point<float> c (R, R);

        juce::Path disc;
        disc.addEllipse (0.0f, 0.0f, (float) px, (float) px);
        g.reduceClipRegion (disc, juce::AffineTransform());

        const int N = 144;
        const float step = juce::MathConstants<float>::twoPi / (float) N;
        for (int i = 0; i < N; ++i)
        {
            const float a0 = (float) i * step - juce::MathConstants<float>::pi, am = a0 + step * 0.5f;
            const float lobe = 0.5f + 0.5f * std::cos (2.0f * (am + 1.05f));          // main anisotropic highlight axis (upper right <-> lower left)
            const float lobe2 = 0.5f + 0.5f * std::cos (4.0f * am + 0.6f);
            float v = 0.22f + 0.56f * std::pow (lobe, 2.0f) + 0.09f * lobe2 + 0.025f * std::sin (53.0f * am);
            v = juce::jlimit (0.0f, 1.0f, v);
            juce::Colour col = juce::Colour::fromFloatRGBA (v * 0.92f, v * 0.97f, juce::jmin (1.0f, v * 1.06f), 1.0f);
            const float tint = std::max (0.0f, std::cos (am - 0.75f)) * 0.16f;        // faint violet cast toward the lower right
            col = col.interpolatedWith (juce::Colour (0xff7a4a88), tint);

            juce::Path wedge;
            wedge.startNewSubPath (c);
            wedge.lineTo (c.x + std::cos (a0 - 0.002f) * R * 1.05f, c.y + std::sin (a0 - 0.002f) * R * 1.05f);
            wedge.lineTo (c.x + std::cos (a0 + step + 0.004f) * R * 1.05f, c.y + std::sin (a0 + step + 0.004f) * R * 1.05f);
            wedge.closeSubPath();
            g.setColour (col);
            g.fillPath (wedge);
        }

        for (float r = 1.2f; r < R; r += 1.7f)                                       // fine turned grooves
        {
            g.setColour ((((int) (r / 1.7f)) & 1) ? juce::Colours::white.withAlpha (0.045f) : juce::Colours::black.withAlpha (0.06f));
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.6f);
        }

        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c.x, c.y,                         // darker towards the rim
                                                 juce::Colour (0xff05080d).withAlpha (0.45f), c.x, c.y + R, true));
        g.fillEllipse (0.0f, 0.0f, (float) px, (float) px);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), c.x - R * 0.35f, c.y - R * 0.40f,   // soft specular
                                                 juce::Colours::white.withAlpha (0.0f), c.x - R * 0.35f, c.y - R * 0.40f + R * 0.75f, true));
        g.fillEllipse (0.0f, 0.0f, (float) px, (float) px);

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), 0.0f, 0.0f,                  // lit top rim, dark bottom rim
                                                 juce::Colours::black.withAlpha (0.55f), 0.0f, (float) px, false));
        g.drawEllipse (0.6f, 0.6f, (float) px - 1.2f, (float) px - 1.2f, 1.3f);

        return cache.emplace (px, img).first->second;
    }
}
