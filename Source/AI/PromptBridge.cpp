#include "PromptBridge.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Presets/SoundDesigner.h"

namespace nusynth
{
PromptBridge::PromptBridge (juce::AudioProcessorValueTreeState& s)
    : juce::Thread ("NuSynth AI"), apvts (s)
{
    startThread (juce::Thread::Priority::background);
}

PromptBridge::~PromptBridge()
{
    alive->store (false);
    signalThreadShouldExit();
    wake.signal();
    stopThread (3000);
}

void PromptBridge::submit (const juce::String& prompt, Callback onDone)
{
    {
        const juce::ScopedLock sl (lock);
        jobs.push_back ({ prompt, std::move (onDone) });
    }
    wake.signal();
}

void PromptBridge::run()
{
    while (! threadShouldExit())
    {
        wake.wait (500);

        for (;;)
        {
            Job job;
            juce::String url;
            {
                const juce::ScopedLock sl (lock);
                if (jobs.empty()) break;
                job = std::move (jobs.front());
                jobs.pop_front();
                url = endpoint;
            }

            juce::var payload;
            bool usedFallback = false;
            if (url.isNotEmpty())
                payload = fetchFromEndpoint (url, job.prompt);
            if (! payload.isObject())
            {
                payload = keywordFallback (job.prompt);
                usedFallback = true;
            }

            auto aliveFlag = alive;
            juce::MessageManager::callAsync ([this, aliveFlag, payload, usedFallback, cb = std::move (job.done)]
            {
                if (! aliveFlag->load()) return;
                applyPreset (payload);
                if (afterApply) afterApply();
                juce::String msg = usedFallback ? "Designed offline" : "Generated";
                if (auto* o = payload.getDynamicObject()) { const auto sm = o->getProperty ("summary").toString(); if (sm.isNotEmpty()) msg = sm; }
                if (cb) cb (true, msg);
            });
        }
    }
}

juce::var PromptBridge::fetchFromEndpoint (const juce::String& url, const juce::String& prompt)
{
    auto* body = new juce::DynamicObject();
    body->setProperty ("prompt", prompt);
    body->setProperty ("schema", "nusynth-v1");

    auto u = juce::URL (url).withPOSTData (juce::JSON::toString (juce::var (body), true));
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders ("Content-Type: application/json")
                    .withConnectionTimeoutMs (8000);

    if (auto stream = u.createInputStream (opts))
        return juce::JSON::parse (stream->readEntireStreamAsString());

    return {};
}

juce::var PromptBridge::keywordFallback (const juce::String& prompt)
{
    juce::Random rng (juce::Time::currentTimeMillis() ^ (juce::int64) prompt.hashCode64());
    auto design = designSound (prompt, rng);
    auto v = design.patch.toVar();
    if (auto* o = v.getDynamicObject()) o->setProperty ("summary", design.summary);
    return v;
}

void PromptBridge::applyPreset (const juce::var& payload)
{
    JUCE_ASSERT_MESSAGE_THREAD
    applyPatchVar (apvts, payload);
}
}
