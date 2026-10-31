#pragma once
#include "PluginProcessor.h"
#include "Panels.h"

namespace nusynth::ui
{
/** Wraps a panel with an outer margin so it can fill a whole page. */
template <class P>
struct Padded : juce::Component
{
    template <class... Args>
    explicit Padded (Args&&... args) : inner (std::forward<Args> (args)...) { addAndMakeVisible (inner); }
    void resized() override { inner.setBounds (getLocalBounds().reduced (8)); }
    P inner;
};

// ------------------------------------------------------------------ MAIN
struct MainPage final : juce::Component
{
    explicit MainPage (NuSynthAudioProcessor& p)
        : strip (p.apvts), oscA (p, 0), oscB (p, 1), oscC (p, 2), noise (p), sub (p.apvts), filter (p.apvts),
          lfo (p.apvts), env (p.apvts), play (p.apvts), ai (p)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &strip, &oscA, &oscB, &oscC, &noise, &sub, &filter, &lfo, &env, &play, &ai })
            addAndMakeVisible (c);
    }

    void resized() override
    {
        const int gap = 6;
        auto area = getLocalBounds().reduced (8);

        strip.setBounds (area.removeFromTop (54));
        area.removeFromTop (gap);

        auto bottom = area.removeFromBottom ((int) ((float) area.getHeight() * 0.36f));
        area.removeFromBottom (gap);

        const int W = area.getWidth(), X = area.getX();
        auto seg = [&] (juce::Rectangle<int> r, float f0, float f1)
        {
            const int x0 = X + (int) ((float) W * f0) + (f0 > 0.0f ? gap / 2 : 0);
            const int x1 = X + (int) ((float) W * f1) - (f1 < 1.0f ? gap / 2 : 0);
            return juce::Rectangle<int> (x0, r.getY(), x1 - x0, r.getHeight());
        };

        // top row: OSC A | OSC B | OSC C | NOISE over SUB | FILTER
        oscA.setBounds (seg (area, 0.00f, 0.205f));
        oscB.setBounds (seg (area, 0.205f, 0.41f));
        oscC.setBounds (seg (area, 0.41f, 0.615f));
        auto mid = seg (area, 0.615f, 0.79f);
        noise.setBounds (mid.removeFromTop ((int) ((float) mid.getHeight() * 0.66f)));
        mid.removeFromTop (gap);
        sub.setBounds (mid);
        filter.setBounds (seg (area, 0.79f, 1.00f));

        // bottom row: LFOs | ENVELOPES | PLAY + ARP | AI
        lfo.setBounds  (seg (bottom, 0.00f, 0.30f));
        env.setBounds  (seg (bottom, 0.30f, 0.56f));
        play.setBounds (seg (bottom, 0.56f, 0.84f));
        ai.setBounds   (seg (bottom, 0.84f, 1.00f));
    }

    TopStrip strip;
    OscPanel oscA, oscB, oscC;
    NoisePanel noise;
    SubPanel sub;
    FilterPanel filter;
    LfoPanel lfo;
    EnvPanel env;
    PlayPanel play;
    AiPanel ai;
};

// ------------------------------------------------------------------ OTHER = mod matrix (top) + microtuning | voicing (bottom halves)
struct OtherPage final : juce::Component
{
    explicit OtherPage (NuSynthAudioProcessor& p)
        : matrix (p.apvts, kNumModSlots, true, "MOD MATRIX"), micro (p.apvts), voicing (p)
    {
        addAndMakeVisible (matrix);
        addAndMakeVisible (micro);
        addAndMakeVisible (voicing);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        matrix.setBounds (r.removeFromTop ((int) ((float) r.getHeight() * 0.51f)));
        r.removeFromTop (6);
        micro.setBounds (r.removeFromLeft (r.getWidth() / 2 - 3));
        r.removeFromLeft (6);
        voicing.setBounds (r);
    }

    ModPanel matrix;
    MicrotunePanel micro;
    VoicePanel voicing;
};
}
