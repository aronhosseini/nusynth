#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <algorithm>
#include "Parameters/ParameterIDs.h"

namespace nusynth
{
/** MIDI-in -> MIDI-out arpeggiator, sample accurate, tempo from the host. */
class Arpeggiator
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        held.reserve (64); seq.reserve (256); sounding.reserve (64); pass.reserve (128);
        reset();
    }

    void bind (juce::AudioProcessorValueTreeState& a)
    {
        on = a.getRawParameterValue (ids::arpOn);
        pattern = a.getRawParameterValue (ids::arpPattern);
        octaves = a.getRawParameterValue (ids::arpOctaves);
        gate = a.getRawParameterValue (ids::arpGate);
        swing = a.getRawParameterValue (ids::arpSwing);
        rate = a.getRawParameterValue (ids::arpRate);
    }

    void reset()
    {
        held.clear(); sounding.clear(); pass.clear();
        stepCountdown = 0.0; offCountdown = 0.0; stepCounter = 0; wasOn = false;
    }

    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, double bpm)
    {
        out.clear();
        const bool isOn = on->load() > 0.5f;

        if (! isOn)
        {
            if (wasOn)
            {
                for (int n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                sounding.clear();
                for (auto& h : held)
                {
                    out.addEvent (juce::MidiMessage::noteOn (1, h.note, (juce::uint8) h.vel), 0);
                    pass.push_back (h.note);
                }
                held.clear();
                wasOn = false;
            }
            for (const auto meta : in)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOn()) pass.push_back (m.getNoteNumber());
                else if (m.isNoteOff()) pass.erase (std::remove (pass.begin(), pass.end(), m.getNoteNumber()), pass.end());
                out.addEvent (m, meta.samplePosition);
            }
            return;
        }

        if (! wasOn)
        {
            for (int n : pass)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                held.push_back ({ n, 100, orderCounter++ });
            }
            pass.clear();
            stepCountdown = 0.0; stepCounter = 0;
            wasOn = true;
        }

        const double beatsTable[9] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
        baseStep = beatsTable[std::min (8, std::max (0, (int) rate->load()))] * 60.0 / std::max (20.0, bpm) * sampleRate;

        int pos = 0;
        for (const auto meta : in)
        {
            const int t = std::min (std::max (meta.samplePosition, pos), numSamples);
            advance (pos, t, out);
            pos = t;
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const bool wasEmpty = held.empty();
                held.push_back ({ m.getNoteNumber(), (int) m.getVelocity(), orderCounter++ });
                if (wasEmpty) { stepCountdown = 0.0; stepCounter = 0; }
            }
            else if (m.isNoteOff())
            {
                held.erase (std::remove_if (held.begin(), held.end(), [&m] (const Held& h) { return h.note == m.getNoteNumber(); }), held.end());
                if (held.empty())
                {
                    for (int n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n), t);
                    sounding.clear();
                }
            }
            else out.addEvent (m, t);
        }
        advance (pos, numSamples, out);
    }

private:
    struct Held { int note, vel; int order; };

    void advance (int from, int to, juce::MidiBuffer& out)
    {
        double pos = (double) from;
        const double end = (double) to;
        int guard = 0;
        while (guard++ < 256)
        {
            const double kInf = 1.0e30;
            const double nextStep = held.empty() ? kInf : pos + stepCountdown;
            const double nextOff = sounding.empty() ? kInf : pos + offCountdown;
            const double nxt = std::min (nextStep, nextOff);
            if (nxt >= end)
            {
                const double dt = end - pos;
                if (! held.empty()) stepCountdown -= dt;
                if (! sounding.empty()) offCountdown -= dt;
                break;
            }
            const double dt = nxt - pos;
            if (! held.empty()) stepCountdown -= dt;
            if (! sounding.empty()) offCountdown -= dt;
            pos = nxt;
            const int sp = std::min (std::max ((int) pos, from), std::max (from, to - 1));

            if (! sounding.empty() && offCountdown <= 1.0e-9)
            {
                for (int n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n), sp);
                sounding.clear();
            }
            if (! held.empty() && stepCountdown <= 1.0e-9)
            {
                for (int n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n), sp);
                sounding.clear();
                triggerStep (out, sp);
            }
        }
    }

    void triggerStep (juce::MidiBuffer& out, int sp)
    {
        const int pat = (int) pattern->load();
        std::vector<Held> sorted (held.begin(), held.end());
        if (pat == 5) std::sort (sorted.begin(), sorted.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });
        else          std::sort (sorted.begin(), sorted.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });

        seq.clear();
        const int oct = std::max (1, (int) octaves->load());
        for (int o = 0; o < oct; ++o)
            for (const auto& h : sorted)
                if (h.note + 12 * o <= 127) seq.push_back ({ h.note + 12 * o, h.vel, h.order });
        if (seq.empty()) return;

        const int n = (int) seq.size();
        if (pat == 6)   // chord
        {
            for (const auto& h : seq) { out.addEvent (juce::MidiMessage::noteOn (1, h.note, (juce::uint8) h.vel), sp); sounding.push_back (h.note); }
        }
        else
        {
            int idx = 0;
            const int k = stepCounter;
            const int period = std::max (1, 2 * n - 2);
            switch (pat)
            {
                case 1:  idx = n - 1 - (k % n); break;
                case 2:  { const int q = k % period; idx = q < n ? q : period - q; break; }
                case 3:  { const int q = k % period; idx = n - 1 - (q < n ? q : period - q); break; }
                case 4:  idx = (int) (rng() % (unsigned) n); break;
                case 7:  { const int j = k % n; idx = (j % 2 == 0) ? j / 2 : n - 1 - j / 2; break; }                         // converge
                case 8:  { const int j = k % n; const int c = n / 2; const int off = (j + 1) / 2; idx = c + ((j % 2) ? off : -off); break; }   // diverge
                case 9:  { const int j = k % (2 * std::max (1, n - 1)); idx = (j % 2 == 1 || n == 1) ? n - 1 : (j / 2) % std::max (1, n - 1); break; }   // pinky up
                case 10: { const int j = k % (2 * std::max (1, n - 1)); idx = (j % 2 == 1 || n == 1) ? 0 : 1 + (j / 2) % std::max (1, n - 1); break; }   // thumb up
                case 11: idx = (k / 2) % n; break;                                                                           // double up
                case 12: idx = n - 1 - ((k / 2) % n); break;                                                                 // double down
                case 13: idx = (k / 2 + (k % 2) * 2) % n; break;                                                             // zigzag
                case 14: { const int stepDir = (int) (rng() % 3u) - 1; walkIdx = std::min (n - 1, std::max (0, walkIdx + stepDir)); idx = walkIdx; break; }   // random walk
                case 15: idx = k % n; break;                                                                                 // octave jump (see below)
                case 16: idx = (k + k / n) % n; break;                                                                       // cascade
                default: idx = k % n; break;
            }
            idx = std::min (n - 1, std::max (0, idx));
            auto h = seq[(size_t) idx];
            if (pat == 15 && (k % 2) == 1 && h.note + 12 <= 127) h.note += 12;
            out.addEvent (juce::MidiMessage::noteOn (1, h.note, (juce::uint8) h.vel), sp);
            sounding.push_back (h.note);
        }

        const double sw = (double) swing->load() * 0.5;
        const double len = baseStep * ((stepCounter % 2 == 0) ? (1.0 + sw) : (1.0 - sw));
        stepCountdown += len;
        offCountdown = std::max (1.0, baseStep * (double) gate->load());
        ++stepCounter;
    }

    unsigned rng() { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

    std::atomic<float>*on = nullptr, *pattern = nullptr, *octaves = nullptr, *gate = nullptr, *swing = nullptr, *rate = nullptr;
    double sampleRate = 44100.0, baseStep = 11025.0, stepCountdown = 0.0, offCountdown = 0.0;
    std::vector<Held> held, seq;
    std::vector<int> sounding, pass;
    int stepCounter = 0, orderCounter = 0, walkIdx = 0;
    bool wasOn = false;
    unsigned seed = 7u;
};
}
