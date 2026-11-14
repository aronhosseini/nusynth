#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <vector>

namespace nusynth
{
/** Snapshot based undo / redo of the whole plugin state. Any change (knob, preset, LFO point, FX...) that is followed by
    ~0.35 s without further changes becomes one undo step. Message thread. */
class UndoHistory final : private juce::Timer, private juce::AudioProcessorListener
{
public:
    UndoHistory (juce::AudioProcessorValueTreeState& a, std::function<void()> afterRestoreCb)
        : apvts (a), afterRestore (std::move (afterRestoreCb)) {}

    ~UndoHistory() override { stopTimer(); apvts.processor.removeListener (this); }

    void start()
    {
        snapshots.clear();
        snapshots.push_back (apvts.copyState());
        index = 0;
        apvts.processor.addListener (this);
        startTimerHz (10);
    }

    void markDirty() { lastChange.store (juce::Time::getMillisecondCounter()); dirty.store (true); }

    bool canUndo() const { return index > 0 || dirty.load(); }
    bool canRedo() const { return index + 1 < (int) snapshots.size(); }

    void undo()
    {
        if (dirty.load()) { push(); }                      // make sure the latest edit is recorded first
        if (index > 0) restore (index - 1);
    }
    void redo() { if (index + 1 < (int) snapshots.size()) restore (index + 1); }

private:
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { if (! restoring.load()) markDirty(); }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

    void timerCallback() override
    {
        if (dirty.load() && juce::Time::getMillisecondCounter() - lastChange.load() > 350) push();
    }

    void push()
    {
        dirty.store (false);
        auto s = apvts.copyState();
        if (! snapshots.empty() && s.isEquivalentTo (snapshots[(size_t) index])) return;
        snapshots.resize ((size_t) index + 1);             // drop the redo branch
        snapshots.push_back (s);
        ++index;
        if (snapshots.size() > 100) { snapshots.erase (snapshots.begin()); --index; }
    }

    void restore (int i)
    {
        restoring.store (true);
        index = i;
        apvts.replaceState (snapshots[(size_t) i].createCopy());
        if (afterRestore) afterRestore();
        dirty.store (false);
        restoring.store (false);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::function<void()> afterRestore;
    std::vector<juce::ValueTree> snapshots;
    int index = 0;
    std::atomic<bool> dirty { false }, restoring { false };
    std::atomic<juce::uint32> lastChange { 0 };
};
}
