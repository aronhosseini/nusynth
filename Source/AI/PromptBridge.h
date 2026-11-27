#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <deque>
#include <map>
#include <vector>
#include <functional>
#include <memory>

namespace nusynth
{
/** "Type-to-Sound": turns a text prompt into a parameter payload.

    - Runs on its own background thread (never the audio thread).
    - OFFLINE (default): the built-in sound designer (keywordFallback -> sd::designSound).
    - ONLINE (opt-in, setOnlineMode (true)): an OpenAI-compatible chat-completions endpoint. The model must answer with
          { "params": { "<paramID>": <number | choice name>, ... }, "summary": "..." }
      A LIST of providers is tried in order (failover): the next one is used when a provider fails (401/402/403/429, HTTP / network error,
      timeout, invalid reply). The provider that worked last time is tried first. Sources of the list, in order:
        1. NUSYNTH_AI_ENDPOINT / NUSYNTH_AI_MODEL / NUSYNTH_AI_API_KEY (one custom provider)
        2. <user app data>/NuSynth/ai.json: {"providers":[{"name","endpoint","model","apiKey"}, ...]} (or the single "endpoint","model","apiKey")
        3. built-in presets that are enabled by an environment variable holding YOUR key (OPENROUTER_API_KEY, GROQ_API_KEY, CEREBRAS_API_KEY,
           MISTRAL_API_KEY, GEMINI_API_KEY) - no key is ever stored in the source
        4. keyless public endpoints (Kilo free, Pollinations, ch.at).
      No key = no Authorization header (Kilo, Pollinations, Ollama, Off Grid ...).
      There is NO fallback to the offline designer: when Online is on and EVERY provider fails the callback gets ok = false with a
      "Not connected ..." message and the current sound is left untouched.
    - Results are applied on the message thread through the parameter objects
      (atomic writes, host-automation aware) -> no locks, no audio dropouts. */
class PromptBridge final : private juce::Thread
{
public:
    using Callback = std::function<void (bool ok, const juce::String& message)>;

    explicit PromptBridge (juce::AudioProcessorValueTreeState& apvts);
    ~PromptBridge() override;

    std::function<void()> afterApply;      // set by the processor (reload LFO shapes etc.)
    std::function<void()> beforeApply;     // set by the processor (silence the voices of the previous sound)

    /** One online provider. An empty apiKey means "keyless": no Authorization header is sent. */
    struct Provider { juce::String name, url, model, apiKey; };

    /** Replaces the provider list (thread-safe). url may be a base URL ("https://host/api/v1") or the full ".../chat/completions". */
    void setProviders (std::vector<Provider> list);

    /** OFFLINE is the default. Safe from any thread; takes effect for jobs submitted afterwards. */
    void setOnlineMode (bool shouldBeOnline) noexcept { onlineMode.store (shouldBeOnline); }
    bool isOnlineMode() const noexcept                { return onlineMode.load(); }

    void submit (const juce::String& prompt, Callback onDone);

    /** Exposed for tests: apply a {"params": {...}} payload. Message thread only. */
    void applyPreset (const juce::var& payload);

private:
    void run() override;
    juce::var keywordFallback (const juce::String& prompt);      // background thread only
    juce::var fetchOnline (const Provider& p, const juce::String& prompt, juce::String& failureReason);          // one provider; background thread only; void var on any failure
    juce::var fetchChain (const std::vector<Provider>& list, const juce::String& prompt, juce::String& usedName, juce::String& report);   // tries them in order
    juce::var validateOnlinePayload (const juce::var& parsed, const juce::String& prompt) const;                                           // keeps only known IDs / valid values; void if unusable
    juce::String buildSystemPrompt() const;                                                                    // constructed on the message thread
    void loadProviders();
    std::vector<std::map<juce::String, juce::String>> history;          // choice parameters of the previous generation (to avoid repeating it)

    struct ParamInfo { juce::String name; bool isChoice = false; juce::StringArray choices; float lo = 0.0f, hi = 1.0f; };
    struct Job { juce::String prompt; Callback done; bool online = false; std::vector<Provider> providers; };

    juce::AudioProcessorValueTreeState& apvts;
    juce::CriticalSection lock;
    std::deque<Job> jobs;
    juce::WaitableEvent wake;
    std::vector<Provider> providers;                // online providers in priority order (guarded by `lock`)
    std::atomic<int> lastGood { -1 };
    std::vector<std::unique_ptr<juce::Thread>> workers;   // HTTP workers (bridge thread only). A hung request is abandoned after its timeout, never waited for.
    juce::StringArray recentOnline;                        // compact signatures of the last online results (bridge thread only): "do not repeat these"               // index of the provider that worked last (tried first next time)
    std::atomic<bool> onlineMode { false };
    std::map<juce::String, ParamInfo> paramInfo;    // built once in the constructor, read-only afterwards
    juce::String systemPrompt;                      // built once in the constructor, read-only afterwards
    std::shared_ptr<std::atomic<bool>> alive { std::make_shared<std::atomic<bool> > (true) };
};
}
