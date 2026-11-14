#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>
#include <functional>
#include <memory>

namespace nusynth
{
/** "Type-to-Sound": turns a text prompt into a parameter payload.

    - Runs on its own background thread (never the audio thread).
    - Backend: local HTTP bridge (e.g. an ONNX / LLM server on localhost) that returns
          { "params": { "<paramID>": <number | choice name>, ... } }
      If no endpoint is configured (or it fails) a small offline keyword fallback is used.
    - Results are applied on the message thread through the parameter objects
      (atomic writes, host-automation aware) -> no locks, no audio dropouts. */
class PromptBridge final : private juce::Thread
{
public:
    using Callback = std::function<void (bool ok, const juce::String& message)>;

    explicit PromptBridge (juce::AudioProcessorValueTreeState& apvts);
    ~PromptBridge() override;

    std::function<void()> afterApply;      // set by the processor (reload LFO shapes etc.)

    void setEndpoint (const juce::String& url) { const juce::ScopedLock sl (lock); endpoint = url; }
    void submit (const juce::String& prompt, Callback onDone);

    /** Exposed for tests: apply a {"params": {...}} payload. Message thread only. */
    void applyPreset (const juce::var& payload);

private:
    void run() override;
    juce::var fetchFromEndpoint (const juce::String& url, const juce::String& prompt);
    static juce::var keywordFallback (const juce::String& prompt);

    struct Job { juce::String prompt; Callback done; };

    juce::AudioProcessorValueTreeState& apvts;
    juce::CriticalSection lock;
    std::deque<Job> jobs;
    juce::WaitableEvent wake;
    juce::String endpoint { "http://127.0.0.1:8765/generate" };
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool> > (true) };
};
}
