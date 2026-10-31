#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"

NuSynthAudioProcessor::NuSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NuSynthState", nusynth::createParameterLayout()),
      presets (apvts),
      undo (apvts, [this] { syncShapesFromState(); }),
      promptBridge (apvts)
{
    tuning.bind (apvts);
    engine.bind (apvts, &tuning);
    engine.setNoiseSample (&noiseSample);
    nusynth::registerShared (&apvts, &shared);
    engine.setShared (&shared);
    shared.onUiEdit = [this] { undo.markDirty(); };
    noteMap.fill (-1);
    keyboardState.addListener (&collector);
    arp.bind (apvts);
    fx.bind (apvts);
    masterGainDb = apvts.getRawParameterValue (nusynth::ids::masterGainDb);
    presets.beforeSave = [this] { userContent.saveToState (apvts.state); };
    presets.afterLoad  = [this] { userContent.loadFromState (apvts.state); syncShapesFromState(); };
    promptBridge.afterApply = [this] { syncShapesFromState(); };
    syncShapesFromState();
    undo.start();
}

void NuSynthAudioProcessor::syncShapesFromState()
{
    for (int l = 0; l < nusynth::kNumLfos; ++l)
    {
        nusynth::LfoCustom c;
        const auto str = apvts.state.getProperty ("lfoShape" + juce::String (l + 1)).toString();
        if (str.isEmpty() || ! c.fromString (str)) c.setDefault();
        shared.lfo.set (l, c);
    }
}

void NuSynthAudioProcessor::prepareToPlay (double sr, int blockSize)
{
    collector.reset (sr);
    engine.prepare (sr);
    arp.prepare (sr);
    fx.prepare (sr, blockSize);
    arpOut.ensureSize (8192);
}

bool NuSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NuSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    collector.removeNextBlockOfMessages (midi, n);      // on-screen keyboard + wheels

    if (panicRequested.exchange (false))
    {
        engine.killAll();
        arp.reset();
        noteMap.fill (-1);
        heldLo.store (0); heldHi.store (0);
    }

    // octave shift (every note-off uses the same transposition as its note-on, so nothing can hang)
    const int oct = (int) apvts.getRawParameterValue (nusynth::ids::octaveShift)->load();
    shifted.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int t = juce::jlimit (0, 127, m.getNoteNumber() + 12 * oct);
            noteMap[(size_t) m.getNoteNumber()] = t;
            lastNote.store (t); lastVelocity.store (m.getFloatVelocity());
            shifted.addEvent (juce::MidiMessage::noteOn (m.getChannel(), t, m.getVelocity()), meta.samplePosition);
        }
        else if (m.isNoteOff())
        {
            int t = noteMap[(size_t) m.getNoteNumber()];
            if (t < 0) t = juce::jlimit (0, 127, m.getNoteNumber() + 12 * oct);
            noteMap[(size_t) m.getNoteNumber()] = -1;
            shifted.addEvent (juce::MidiMessage::noteOff (m.getChannel(), t, m.getVelocity()), meta.samplePosition);
        }
        else shifted.addEvent (m, meta.samplePosition);
    }

    tuning.update();
    arp.process (shifted, arpOut, n, bpm);

    // which notes does the engine hold? (drives the on-screen keyboard highlight)
    {
        juce::uint64 lo = heldLo.load(), hi = heldHi.load();
        for (const auto meta : arpOut)
        {
            const auto m = meta.getMessage();
            const int note = m.getNoteNumber();
            if (note < 0 || note > 127) continue;
            const juce::uint64 bit = (juce::uint64) 1 << (note & 63);
            if (m.isNoteOn())       { if (note < 64) lo |= bit; else hi |= bit; }
            else if (m.isNoteOff()) { if (note < 64) lo &= ~bit; else hi &= ~bit; }
            else if (m.isAllNotesOff() || m.isAllSoundOff()) { lo = 0; hi = 0; }
        }
        heldLo.store (lo); heldHi.store (hi);
    }

    engine.process (buffer, arpOut, bpm);
    fx.process (buffer);

    buffer.applyGain (juce::Decibels::decibelsToGain (masterGainDb->load()));

    float pl = 0.0f, pr = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) { d[i] = juce::jlimit (-1.0f, 1.0f, d[i]); pk = std::max (pk, std::abs (d[i])); }
        (ch == 0 ? pl : pr) = pk;
    }
    peakL.store (std::max (peakL.load(), pl));
    peakR.store (std::max (peakR.load(), buffer.getNumChannels() > 1 ? pr : pl));
}

juce::AudioProcessorEditor* NuSynthAudioProcessor::createEditor() { return new NuSynthEditor (*this); }

void NuSynthAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    userContent.saveToState (apvts.state);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void NuSynthAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            userContent.loadFromState (apvts.state);
            syncShapesFromState();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NuSynthAudioProcessor(); }
