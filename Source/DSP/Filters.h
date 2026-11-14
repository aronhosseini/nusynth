#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace nusynth
{
inline float fastTanh (float x)
{
    x = std::max (-3.0f, std::min (3.0f, x));
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** One of 30 filter types (indices match filterTypeNames()). Stereo, per-voice instance. */
class FilterUnit
{
public:
    void prepare (double sr)
    {
        sampleRate = (float) sr;
        int size = 1;
        while ((double) size < sr / 20.0 + 8.0) size <<= 1;
        combMask = size - 1;
        for (auto& c : ch) c.comb.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.s1 = {}; c.s2 = {}; c.fm[0] = c.fm[1] = c.fm[2] = {};
            for (auto& v : c.ls) v = 0.0f;
            for (auto& v : c.ap) v = 0.0f;
            c.y4 = 0.0f; c.apFb = 0.0f; c.wp = 0;
            std::fill (c.comb.begin(), c.comb.end(), 0.0f);
        }
        ringPh = 0.0f; flangePh = 0.0f;
    }

    void setParams (int t, float cutoff, float res, float drv, float mixAmt)
    {
        type = t; drive = std::min (1.0f, std::max (0.0f, drv)); mix = std::min (1.0f, std::max (0.0f, mixAmt));
        res = std::min (1.0f, std::max (0.0f, res));
        const float fc = std::min (std::max (cutoff, 20.0f), 0.45f * sampleRate);
        const float pi = 3.14159265359f;
        resAmt = res;

        const float q2 = 0.707f + res * res * 16.0f;      // 12 dB/oct
        const float q4 = 0.6f + res * res * 8.0f;         // each stage of 24 dB/oct
        c1.set (fc, q2, sampleRate);
        c2.set (fc, q4, sampleRate);
        c2b.set (fc, 0.6f + res * res * 4.0f, sampleRate);

        // Ladder
        const float g = std::tan (pi * std::min (fc, 0.4f * sampleRate) / sampleRate);
        ladG = g / (1.0f + g);
        ladK = res * 3.9f;
        ladIn = (t == 4) ? 1.0f : 1.6f;

        // Peak / shelf
        peakG = res * 8.0f;
        shelfG = res * 3.0f;
        cpk.set (fc, 2.0f, sampleRate);
        cshelf.set (fc, 0.707f, sampleRate);

        // Comb
        combDelay = std::min (std::max (sampleRate / fc, 2.0f), (float) combMask - 4.0f);
        combFb = res * 0.95f;

        // Phaser (first-order allpass)
        const float tn = std::tan (pi * fc / sampleRate);
        apCoef = (tn - 1.0f) / (tn + 1.0f);
        apFbAmt = res * 0.85f;
        apStages = (t == 25) ? 8 : 4;

        // Ring mod
        ringInc = fc / sampleRate;
        ringMix = 0.3f + 0.7f * res;

        // Formants
        static const float vow[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        const float pos = std::min (1.0f, std::max (0.0f, std::log (fc / 20.0f) / std::log (1000.0f)));
        float v = pos * 4.0f;
        if (t == 18) v = pos * 2.0f;
        else if (t == 19) v = 2.0f + pos * 2.0f;
        const int i0 = std::min (3, (int) v);
        const float fr = v - (float) i0;
        const float fq = (t == 20 ? 12.0f : (t == 29 ? 4.0f : 6.0f)) + res * 20.0f;
        for (int j = 0; j < 3; ++j)
            fm[j].set (vow[i0][j] + (vow[i0 + 1][j] - vow[i0][j]) * fr, fq, sampleRate);
    }

    void process (float& l, float& r)
    {
        const float dl = l, dr = r;
        l = tick (ch[0], dl);
        r = tick (ch[1], dr);
        if (type == 28) { ringPh += ringInc; if (ringPh >= 1.0f) ringPh -= 1.0f; }
        if (type == 23) { flangePh += 0.25f / sampleRate; if (flangePh >= 1.0f) flangePh -= 1.0f; }
        l = dl + (l - dl) * mix;
        r = dr + (r - dr) * mix;
    }

private:
    struct SvfS { float ic1 = 0.0f, ic2 = 0.0f; };
    struct SvfC
    {
        float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        void set (float fc, float Q, float sr)
        {
            const float g = std::tan (3.14159265359f * std::min (fc, 0.45f * sr) / sr);
            k = 1.0f / Q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
    };
    struct Ch
    {
        SvfS s1, s2, fm[3];
        float ls[4] = { 0, 0, 0, 0 };
        float ap[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        float y4 = 0.0f, apFb = 0.0f;
        std::vector<float> comb;
        int wp = 0;
    };

    static void svf (SvfS& s, const SvfC& c, float in, float& lp, float& bp, float& hp)
    {
        const float v3 = in - s.ic2;
        const float v1 = c.a1 * s.ic1 + c.a2 * v3;
        const float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
        lp = v2; bp = v1; hp = in - c.k * v1 - v2;
    }

    float ladder (Ch& c, float in, int stageOut)
    {
        float y = fastTanh (in * ladIn - ladK * c.y4);
        float outs[4];
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - c.ls[i]) * ladG;
            const float o = v + c.ls[i];
            c.ls[i] = o + v;
            y = o;
            outs[i] = o;
        }
        c.y4 = y;
        const float comp = 1.0f + ladK * 0.3f;
        if (stageOut == 5) return (outs[0] * 0.1f + outs[1] * 0.2f + outs[2] * 0.3f + outs[3] * 0.4f) * comp;   // diode-ish
        return outs[stageOut] * comp;
    }

    float tick (Ch& c, float in)
    {
        const float x = in * (1.0f - drive) + fastTanh (in * (1.0f + drive * 8.0f)) * drive * 0.9f;
        float lp, bp, hp, lp2, bp2, hp2;

        switch (type)
        {
            case 0: case 5:  svf (c.s1, c1, x, lp, bp, hp); return lp;
            case 1:          svf (c.s1, c2, x, lp, bp, hp); svf (c.s2, c2b, lp, lp2, bp2, hp2); return lp2;
            case 6: case 9:  svf (c.s1, c1, x, lp, bp, hp); return hp;
            case 7:          svf (c.s1, c2, x, lp, bp, hp); svf (c.s2, c2b, hp, lp2, bp2, hp2); return hp2;
            case 10: case 12: svf (c.s1, c1, x, lp, bp, hp); return c1.k * bp;
            case 11:         svf (c.s1, c2, x, lp, bp, hp); svf (c.s2, c2b, c2.k * bp, lp2, bp2, hp2); return c2b.k * bp2;
            case 13:         svf (c.s1, c1, x, lp, bp, hp); return x - c1.k * bp;
            case 14:         svf (c.s1, c2, x, lp, bp, hp); { const float n1 = x - c2.k * bp; svf (c.s2, c2b, n1, lp2, bp2, hp2); return n1 - c2b.k * bp2; }
            case 15:         svf (c.s1, cpk, x, lp, bp, hp); return x + peakG * cpk.k * bp;
            case 16:         svf (c.s1, cshelf, x, lp, bp, hp); return x + shelfG * lp;
            case 17:         svf (c.s1, cshelf, x, lp, bp, hp); return x + shelfG * hp;

            case 2:  return ladder (c, x, 3);
            case 3:  return ladder (c, x, 1);
            case 4:  return ladder (c, x, 3);
            case 8:  { const float o = ladder (c, x, 3); return x - o; }
            case 26: return ladder (c, x, 5);

            case 18: case 19: case 20: case 29:
            {
                static const float w[3] = { 1.0f, 0.7f, 0.5f };
                float y = 0.0f;
                for (int j = 0; j < 3; ++j) { svf (c.fm[j], fm[j], x, lp, bp, hp); y += w[j] * fm[j].k * bp; }
                return y * 1.5f;
            }

            case 21: case 22: case 23: case 27:
            {
                float d = combDelay;
                if (type == 23) d *= 1.0f + 0.1f * std::sin (6.28318530718f * flangePh);
                float rp = (float) c.wp - d;
                while (rp < 0.0f) rp += (float) (combMask + 1);
                const int i0 = (int) rp;
                const float fr = rp - (float) i0;
                const float a = c.comb[(size_t) (i0 & combMask)], b = c.comb[(size_t) ((i0 + 1) & combMask)];
                const float del = a + (b - a) * fr;
                float wv;
                if (type == 22)      wv = x - combFb * del;
                else if (type == 27) wv = x + fastTanh (combFb * del * 2.5f);
                else                 wv = x + combFb * del;
                c.comb[(size_t) c.wp] = wv;
                c.wp = (c.wp + 1) & combMask;
                return wv * (1.0f - combFb * 0.5f);
            }

            case 24: case 25:
            {
                float y = x + apFbAmt * c.apFb;
                for (int s = 0; s < apStages; ++s)
                {
                    const float o = apCoef * y + c.ap[s];
                    c.ap[s] = y - apCoef * o;
                    y = o;
                }
                c.apFb = y;
                return 0.5f * (x + y);
            }

            case 28: return x * (1.0f - ringMix) + x * std::sin (6.28318530718f * ringPh) * ringMix;
            default: svf (c.s1, c1, x, lp, bp, hp); return lp;
        }
    }

    float sampleRate = 44100.0f;
    int type = 0, combMask = 4095, apStages = 4;
    float drive = 0.0f, mix = 1.0f, resAmt = 0.0f;
    SvfC c1, c2, c2b, cpk, cshelf, fm[3];
    float ladG = 0.1f, ladK = 0.0f, ladIn = 1.0f, peakG = 0.0f, shelfG = 0.0f;
    float combDelay = 100.0f, combFb = 0.0f, apCoef = 0.0f, apFbAmt = 0.0f, ringInc = 0.0f, ringMix = 0.5f;
    float ringPh = 0.0f, flangePh = 0.0f;
    Ch ch[2];
};
}
