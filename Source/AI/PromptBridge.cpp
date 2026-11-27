#include "PromptBridge.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Presets/SoundDesigner.h"
#include <algorithm>
#include <cmath>

namespace nusynth
{
namespace
{
    // Built-in provider presets. NO keys here: a preset with a key is only used when YOUR key is in the named environment variable.
    // (model names change over time: override them in ai.json if a provider renames its free models)
    struct Preset { const char* name; const char* url; const char* model; const char* keyEnv; const char* fixedKey; };
    const Preset kKeyedPresets[] =
    {
        { "OpenRouter free", "https://openrouter.ai/api/v1",                           "openrouter/free",         "OPENROUTER_API_KEY", nullptr },
        { "Groq",            "https://api.groq.com/openai/v1",                         "llama-3.3-70b-versatile", "GROQ_API_KEY",       nullptr },
        { "Cerebras",        "https://api.cerebras.ai/v1",                             "llama-3.3-70b",           "CEREBRAS_API_KEY",   nullptr },
        { "Mistral",         "https://api.mistral.ai/v1",                              "mistral-small-latest",    "MISTRAL_API_KEY",    nullptr },
        { "Gemini",          "https://generativelanguage.googleapis.com/v1beta/openai","gemini-2.0-flash",        "GEMINI_API_KEY",     nullptr },
    };
    // Keyless public endpoints (anonymous free tiers; rate limited per IP). fixedKey = the public placeholder token a service documents for anonymous use.
    const Preset kKeylessPresets[] =
    {
        { "Kilo free",    "https://ai-gateway.kilo.ai/api/v1",                 "kilo-auto/free", nullptr, nullptr },
        { "Pollinations", "https://text.pollinations.ai/v1/chat/completions",  "openai",         nullptr, nullptr },
        { "ch.at",        "https://ch.at/v1/chat/completions",                  "gpt-4o",         nullptr, nullptr },
    };

    constexpr int     kConnectTimeoutMs = 7000;
    constexpr double  kProviderTimeoutMs = 20000.0;     // hard limit per provider (a hung request is abandoned)
    constexpr double  kChainBudgetMs    = 60000.0;      // all providers together
    constexpr int64_t kMaxResponseBytes = 512 * 1024;

    struct HttpResult
    {
        juce::WaitableEvent done;
        int status = 0;
        juce::String body, error;
    };

    /** One HTTP request on its own thread, so the bridge can give up on a hung connection (no platform timeout is fully reliable).
        It owns copies of everything it needs and never touches the PromptBridge. */
    class HttpWorker final : public juce::Thread
    {
    public:
        HttpWorker (juce::String u, juce::String h, juce::String post, std::shared_ptr<HttpResult> r)
            : juce::Thread ("NuSynth AI http"), url (std::move (u)), headers (std::move (h)), postData (std::move (post)), result (std::move (r)) {}

        void run() override
        {
            int status = 0;
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                            .withHttpRequestCmd ("POST")
                            .withExtraHeaders (headers)
                            .withConnectionTimeoutMs (kConnectTimeoutMs)
                            .withStatusCode (&status)
                            .withNumRedirectsToFollow (2);
            auto stream = juce::URL (url).withPOSTData (postData).createInputStream (opts);
            result->status = status;
            if (stream == nullptr) { result->error = status > 0 ? "HTTP " + juce::String (status) : "network error"; result->done.signal(); return; }
            if (status != 0 && (status < 200 || status >= 300)) { result->error = "HTTP " + juce::String (status); result->done.signal(); return; }

            juce::MemoryOutputStream out;
            const double deadline = juce::Time::getMillisecondCounterHiRes() + kProviderTimeoutMs;
            char buf[4096];
            for (;;)
            {
                if (threadShouldExit() || juce::Time::getMillisecondCounterHiRes() > deadline) { result->error = "timeout"; break; }
                const int n = stream->read (buf, (int) sizeof (buf));
                if (n < 0) { result->error = "read error"; break; }
                if (n == 0) break;
                out.write (buf, (size_t) n);
                if ((int64_t) out.getDataSize() > kMaxResponseBytes) { result->error = "response too large"; break; }
            }
            if (result->error.isEmpty()) result->body = out.toString();
            result->done.signal();
        }

    private:
        juce::String url, headers, postData;
        std::shared_ptr<HttpResult> result;
    };

    /** Accepts a base URL (".../api/v1") or a full one (".../v1/chat/completions"). */
    juce::String chatCompletionsUrl (juce::String url)
    {
        url = url.trim();
        while (url.endsWithChar ('/')) url = url.dropLastCharacters (1);
        if (url.isEmpty()) return {};
        return url.endsWithIgnoreCase ("/chat/completions") ? url : url + "/chat/completions";
    }
}

PromptBridge::PromptBridge (juce::AudioProcessorValueTreeState& s)
    : juce::Thread ("NuSynth AI"), apvts (s)
{
    // No thread is started here: it is created on the first GENERATE, so an idle plug-in instance owns no thread at all.
    // OFFLINE (built-in designer) is the default; online is only used after setOnlineMode (true).
    loadProviders();

    // Parameter catalogue for validation + the system prompt (built here, on the message thread; read-only afterwards).
    for (auto* ap : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto& pid = rp->paramID;
            if (pid == ids::masterGainDb || pid == ids::masterTune || pid.startsWith ("micro_")) continue;
            ParamInfo info;
            info.name = rp->getName (64);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (rp)) { info.isChoice = true; info.choices = choice->choices; }
            else { info.lo = rp->getNormalisableRange().start; info.hi = rp->getNormalisableRange().end; }
            paramInfo[pid] = info;
        }
    systemPrompt = buildSystemPrompt();
}

void PromptBridge::setProviders (std::vector<Provider> list)
{
    for (auto& p : list) p.url = chatCompletionsUrl (p.url);
    const juce::ScopedLock sl (lock);
    providers = std::move (list);
    lastGood.store (-1);
}

/** Order: custom (environment) > ai.json > presets enabled by your own key variables > keyless public endpoints. */
void PromptBridge::loadProviders()
{
    std::vector<Provider> list;
    auto add = [&list] (Provider p)
    {
        p.url = chatCompletionsUrl (p.url);
        if (p.url.isEmpty()) return;
        for (const auto& e : list) if (e.url == p.url && e.model == p.model) return;       // no duplicates
        if (p.name.isEmpty()) p.name = juce::URL (p.url).getDomain();
        list.push_back (std::move (p));
    };
    auto env = [] (const char* n) { return juce::SystemStats::getEnvironmentVariable (n, {}).trim(); };

    add ({ "custom", env ("NUSYNTH_AI_ENDPOINT"), env ("NUSYNTH_AI_MODEL"), env ("NUSYNTH_AI_API_KEY") });

    const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("NuSynth").getChildFile ("ai.json");
    if (f.existsAsFile())
        if (auto* o = juce::JSON::parse (f).getDynamicObject())
        {
            auto str = [] (juce::DynamicObject* d, const char* k) { return d->getProperty (k).toString().trim(); };
            if (auto* arr = o->getProperty ("providers").getArray())
                for (const auto& v : *arr)
                    if (auto* d = v.getDynamicObject())
                        add ({ str (d, "name"), str (d, "endpoint"), str (d, "model"), str (d, "apiKey") });
            add ({ "ai.json", str (o, "endpoint"), str (o, "model"), str (o, "apiKey") });          // single-provider form
        }

    for (const auto& pr : kKeyedPresets)
    {
        const auto key = env (pr.keyEnv);
        if (key.isNotEmpty()) add ({ pr.name, pr.url, pr.model, key });
    }
    for (const auto& pr : kKeylessPresets) add ({ pr.name, pr.url, pr.model, pr.fixedKey != nullptr ? pr.fixedKey : "" });

    providers = std::move (list);
}

juce::String PromptBridge::buildSystemPrompt() const
{
    // Compact on purpose: the first version listed every parameter (96 effect slots, 24 mod slots ...) = tens of KB per request, which a slow or
    // filtered connection often drops. Repeating families (oscA/B/C, lfo1-6, env1-4, mod1-24 ...) are written once as a template.
    auto num = [] (float v) { auto s = juce::String (v, 3); if (s.contains (".")) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd ("."); return s; };

    std::map<juce::String, juce::String> listNames;        // joined choice list -> "L<n>"
    juce::String lists;
    auto spec = [&] (const ParamInfo& pi) -> juce::String
    {
        if (! pi.isChoice) return "number " + num (pi.lo) + ".." + num (pi.hi);
        const auto joined = pi.choices.joinIntoString ("; ");
        if (pi.choices.size() <= 6) return "choice {" + joined + "}";
        auto it = listNames.find (joined);
        if (it == listNames.end())
        {
            it = listNames.emplace (joined, "L" + juce::String ((int) listNames.size() + 1)).first;
            lists << it->second << " = {" << joined << "}\n";
        }
        return "choice " + it->second;
    };

    // ---- templated families
    struct Group { juce::String spec; std::vector<juce::String> inst; };
    std::map<juce::String, Group> groups;                  // template -> group
    auto isFx = [] (const juce::String& id) { return id.startsWith ("fx") && id.length() > 2 && juce::CharacterFunctions::isDigit (id[2]); };
    for (const auto& kv : paramInfo)
    {
        const auto& id = kv.first;
        if (isFx (id) || id.contains ("_pt")) continue;
        juce::String key = id, inst;
        if (id.length() > 5 && id.startsWith ("osc") && id[4] == '_' && juce::String ("ABC").containsChar (id[3])) { inst = id.substring (3, 4); key = "osc*" + id.substring (4); }
        else
            for (auto* pre : { "lfo", "env", "mod", "filt" })
                if (id.startsWith (pre))
                {
                    int i = (int) juce::String (pre).length();
                    int d = i;
                    while (d < id.length() && juce::CharacterFunctions::isDigit (id[d])) ++d;
                    if (d > i && d < id.length() && id[d] == '_') { inst = id.substring (i, d); key = juce::String (pre) + "N" + id.substring (d); }
                    break;
                }
        if (id.startsWith ("macro_")) { inst = id.fromFirstOccurrenceOf ("_", false, false); key = "macro_N"; }
        auto& g = groups[key];
        g.spec = spec (kv.second);
        if (inst.isNotEmpty()) g.inst.push_back (inst);
    }
    juce::String lines;
    for (const auto& kv : groups)
    {
        lines << kv.first << " = " << kv.second.spec;
        if (! kv.second.inst.empty())
        {
            if (kv.first.contains ("osc*")) lines << "   (* = A, B, C)";
            else
            {
                int lo = 1000, hi = 0;
                for (const auto& s : kv.second.inst) { lo = juce::jmin (lo, s.getIntValue()); hi = juce::jmax (hi, s.getIntValue()); }
                lines << "   (N = " << lo << ".." << hi << ")";
            }
        }
        lines << "\n";
    }

    // ---- effects: only the first copy of each effect type (slot 1..12); parameters by index with their names
    const auto fxn = fxNames();
    juce::String fxText;
    for (int i = 0; i < kNumFxTypes; ++i)
    {
        if (i == 9 || paramInfo.count (ids::fx (i, "on")) == 0) continue;      // 9 = retired
        const auto label = fxn[i];
        fxText << "fx" << (i + 1) << " " << label << ": fx" << (i + 1) << "_on, fx" << (i + 1) << "_added (both 1 to use it), fx" << (i + 1) << "_mix 0..1";
        for (int k = 0; paramInfo.count (ids::fxp (i, k)) > 0; ++k)
        {
            const auto& pi = paramInfo.at (ids::fxp (i, k));
            fxText << "; " << ids::fxp (i, k) << " " << (pi.name.startsWith (label + " ") ? pi.name.substring (label.length() + 1) : pi.name) << " ";
            if (pi.isChoice) fxText << "{" << pi.choices.joinIntoString ("|") << "}"; else fxText << num (pi.lo) << ".." << num (pi.hi);
        }
        fxText << "\n";
    }

    juce::String t;
    t << "You are the sound designer of NuSynth, a wavetable synthesizer. Turn the user's words into a complete, musical patch.\n"
         "Reply with ONLY one JSON object, no markdown, no commentary:\n"
         "{\"params\": {\"<paramID>\": <number or choice name>, ...}, \"shapes\": {\"lfoShape1\": \"x:y;x:y;...\", \"macroName1\": \"Tone\"}, \"summary\": \"<one short sentence>\"}\n"
         "Use only the IDs below. Numbers must stay inside their range. Choice parameters take one of the listed names as a string. Omitted parameters keep their defaults.\n\n"
         "HOW NUSYNTH WORKS - use ALL of it, a good patch touches every section:\n"
         "1 OSCILLATORS: Osc A/B/C each pick a wavetable (osc*_wavetable), WT position, warp mode + amount, unison voices / detune / spread, semitone pitch, fine, level, pan. "
         "Only Osc A is on at start: for Osc B / Osc C / sub / noise set their level and (optionally) their *_on = 1. Layer 2 or 3 oscillators (octaves, detune) for fat sounds. "
         "Sub oscillator (sub_*) adds low end, noise (noise_*) adds air / texture.\n"
         "2 FILTERS: filt1_* and filt2_* (type, cutoff Hz, resonance, drive, mix, on). route_oscA / route_oscB / route_oscC / route_sub / route_noise choose where each source goes "
         "(Bypass, Filter 1, Filter 2, Parallel, Series). Cutoff is in Hz (log feel): bass 150-1200, pads 1500-6000, leads 2500-12000.\n"
         "3 ENVELOPES: env1 is the AMP envelope (attack / decay / sustain / release in seconds + curves). env2, env3, env4 are free modulators: send them anywhere through the MOD MATRIX "
         "(typical: env2 -> Filter 1 Cutoff for plucks and sweeps, env3 -> Osc A Position or Warp).\n"
         "4 LFOs: lfo1..lfo6 with shape (Custom, Sine, Triangle, Saw, Square, S&H), mode, rate Hz or sync + division, depth, phase, smooth. The shape \"Custom\" is a DRAWN, draggable curve: "
         "set lfoN_shape = \"Custom\" and give its points in \"shapes\": {\"lfoShapeN\": \"x:y;x:y;...\"} with x from 0 to 1 rising, y from 0 to 1 (optional third number = curve -1..1); "
         "first point x=0, last point x=1, 3 to 16 points. Use drawn shapes for step / gate / pump / wobble patterns, and built-in shapes for the rest. Use 2 to 4 LFOs, not just one.\n"
         "5 MOD MATRIX: over 100 slots (mod1..), fill them from mod1 upward. Each slot needs modN_src (a source name), modN_dst (a destination name), modN_amount -1..1 (usually 0.1-0.8), "
         "modN_type (Unipolar or Bipolar), modN_curve. Sources include LFO 1..6, Env 1..4, Velocity, Key, Mod Wheel, Aftertouch, Random and Macro 1..8 (see list). "
         "Destinations include filter cutoff / resonance, oscillator position / warp / pitch / level / pan, sub / noise level, LFO rates, FX amounts (see list). Use 8 to 20 slots for a rich patch.\n"
         "6 MACROS: macro_1..macro_8 (0..1) are performance knobs that can be mod sources; name them with \"shapes\": {\"macroName1\": \"Tone\"} (short names).\n"
         "7 EFFECTS - MANDATORY: every patch uses 3 to 5 effects, a patch with no effects is WRONG. Effect numbers: fx1 Reverb, fx2 Delay, fx3 Chorus, fx4 Flanger, fx5 Phaser, "
         "fx6 Distortion, fx7 Multiband EQ, fx8 Compressor, fx9 Limiter, fx11 Filter, fx12 Hyper/Dimension. For EACH effect you use write fxN_on = 1, fxN_added = 1, fxN_mix (0..1) "
         "and 1 to 3 of its own parameters (fxN_p0, fxN_p1 ... see the EFFECTS list below). Match the sound: reverb / delay for space, chorus / phaser / flanger / hyper for width and movement, "
         "distortion for grit, compressor / EQ / limiter for polish. Also route an LFO or envelope to an effect amount in the mod matrix when it fits (e.g. LFO -> Chorus Depth).\n"
         "8 PLAY: play_mode (Poly / Mono / Legato), glide_time, polyphony, octave. ARP: arp_on, arp_pattern, arp_rate, arp_octaves, arp_gate, arp_swing - use the arp for plucks and sequences when it fits.\n\n"
         "VARIETY: every request is a NEW, independent take. The same words must give a clearly different sound each time: change wavetables, warp, filter type and cutoff, "
         "envelope times, LFO shapes (draw a new custom shape), modulation routings and effects. Follow the variation hint and never repeat the choices listed as already used.\n"
         "Set 40 to 90 parameters, INCLUDING 3 to 5 effects (fxN_on / fxN_added / fxN_mix and their parameters).\n\n"
         "EXAMPLE (a pumping saw bass; yours must differ and fit the user's words):\n"
         "{\"params\":{\"oscA_wavetable\":\"Super Saw Stack\",\"oscA_level\":0.8,\"oscA_unison\":5,\"oscA_detune\":0.3,\"oscB_on\":1,\"oscB_wavetable\":\"Pulse 25\",\"oscB_level\":0.4,\"oscB_pitch\":-12,"
         "\"sub_on\":1,\"sub_level\":0.5,\"filt1_type\":\"Ladder LP 24\",\"filt1_cutoff\":900,\"filt1_res\":0.3,\"route_oscA\":\"Filter 1\",\"route_oscB\":\"Filter 1\","
         "\"env1_attack\":0.003,\"env1_decay\":0.4,\"env1_sustain\":0.6,\"env1_release\":0.2,\"env2_attack\":0.001,\"env2_decay\":0.35,\"env2_sustain\":0,"
         "\"lfo1_shape\":\"Custom\",\"lfo1_sync\":1,\"lfo1_div\":\"1/8\",\"lfo2_shape\":\"Triangle\",\"lfo2_rate\":0.2,"
         "\"mod1_src\":\"Env 2\",\"mod1_dst\":\"Filter 1 Cutoff\",\"mod1_amount\":0.6,\"mod1_type\":\"Unipolar\","
         "\"mod2_src\":\"LFO 1\",\"mod2_dst\":\"Osc A Position\",\"mod2_amount\":0.4,\"mod2_type\":\"Unipolar\","
         "\"mod3_src\":\"LFO 2\",\"mod3_dst\":\"Osc B Pan\",\"mod3_amount\":0.5,\"mod3_type\":\"Bipolar\","
         "\"macro_1\":0.5,\"fx1_on\":1,\"fx1_added\":1,\"fx1_mix\":0.25,\"fx2_on\":1,\"fx2_added\":1,\"fx2_mix\":0.18,\"fx3_on\":1,\"fx3_added\":1,\"fx3_mix\":0.3,\"fx8_on\":1,\"fx8_added\":1,\"fx8_mix\":1},"
         "\"shapes\":{\"lfoShape1\":\"0:1;0.25:0.15;0.5:0.9;0.75:0.1;1:0.6;\",\"macroName1\":\"Tone\"},\"summary\":\"Pumping saw bass with a drawn LFO and an enveloped filter\"}\n\n"
         "PARAMETERS (N and * are the numbers / letters shown in brackets):\n" << lines
      << "\nCHOICE LISTS:\n" << lists
      << "\nEFFECTS (first copy of each type):\n" << fxText;
    return t;
}

PromptBridge::~PromptBridge()
{
    alive->store (false);
    for (auto& w : workers) w->signalThreadShouldExit();
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        wake.signal();
        stopThread (5000);
    }
    for (auto& w : workers) w->stopThread (1500);       // a worker blocked inside the network stack is stopped here at the latest
}

void PromptBridge::submit (const juce::String& prompt, Callback onDone)
{
    {
        const juce::ScopedLock sl (lock);
        Job job { prompt, std::move (onDone) };
        job.online = onlineMode.load();                                   // snapshot: a later toggle never changes a queued job
        job.providers = providers;
        jobs.push_back (std::move (job));
    }
    if (! isThreadRunning()) startThread (juce::Thread::Priority::normal);
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
            {
                const juce::ScopedLock sl (lock);
                if (jobs.empty()) break;
                job = std::move (jobs.front());
                jobs.pop_front();
            }

            // Everything below (network included) runs on this background thread, never on the audio or message thread.
            juce::var payload;
            juce::String msg = "Designed offline";

            if (job.online)
            {
                juce::String reason = "no provider configured", usedName;
                if (! job.providers.empty())
                    payload = fetchChain (job.providers, job.prompt, usedName, reason);

                if (! payload.isObject())
                {
                    // NO fallback to the offline designer: report the error and leave the current sound untouched.
                    DBG ("NuSynth AI: online request failed (" << reason << ")");
                    auto aliveFlag = alive;
                    const juce::String err = "Not connected - online AI unavailable. " + reason;
                    juce::MessageManager::callAsync ([aliveFlag, err, cb = std::move (job.done)]
                    {
                        if (! aliveFlag->load()) return;
                        if (cb) cb (false, err);
                    });
                    continue;
                }
                msg = "Generated (Online: " + usedName + ")";
            }
            else
                payload = keywordFallback (job.prompt);

            if (auto* o = payload.getDynamicObject())
            {
                const auto sm = o->getProperty ("summary").toString().trim();
                if (sm.isNotEmpty()) msg << " - " << sm;
            }

            auto aliveFlag = alive;
            juce::MessageManager::callAsync ([this, aliveFlag, payload, msg, cb = std::move (job.done)]
            {
                if (! aliveFlag->load()) return;
                if (beforeApply) beforeApply();                              // nothing of the last sound keeps ringing
                applyPreset (payload);
                apvts.state.setProperty ("presetName", "AI Sound", nullptr);
                if (afterApply) afterApply();
                if (cb) cb (true, msg);
            });
        }
    }
}

/** Tries the providers in order (the one that worked last time first). Stops at the first valid patch; the report lists every failure. */
juce::var PromptBridge::fetchChain (const std::vector<Provider>& list, const juce::String& prompt, juce::String& usedName, juce::String& report)
{
    std::vector<int> order;
    const int good = lastGood.load();
    if (good >= 0 && good < (int) list.size()) order.push_back (good);
    for (int i = 0; i < (int) list.size(); ++i) if (i != good) order.push_back (i);

    const double start = juce::Time::getMillisecondCounterHiRes();
    juce::StringArray failures;
    for (int idx : order)
    {
        if (threadShouldExit()) break;
        if (juce::Time::getMillisecondCounterHiRes() - start > kChainBudgetMs) { failures.add ("time budget used up"); break; }

        // A dropped / reset connection is common on unstable links: one quick retry for network-level failures before moving on.
        juce::String why = "failed";
        juce::var result;
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            why = "failed";
            result = fetchOnline (list[(size_t) idx], prompt, why);
            const bool transient = why == "network error" || why == "read error" || why.startsWith ("HTTP 5");
            if (result.isObject() || ! transient || threadShouldExit()
                || juce::Time::getMillisecondCounterHiRes() - start > kChainBudgetMs * 0.6) break;
            juce::Thread::sleep (700);
        }
        if (result.isObject())
        {
            lastGood.store (idx);
            usedName = list[(size_t) idx].name;
            return result;
        }
        DBG ("NuSynth AI: " << list[(size_t) idx].name << " failed: " << why);
        failures.add (list[(size_t) idx].name + ": " + why);
    }
    lastGood.store (-1);
    report = "Tried " + juce::String (failures.size()) + " provider(s) - " + failures.joinIntoString ("; ");
    return {};
}

juce::var PromptBridge::fetchOnline (const Provider& cfg, const juce::String& prompt, juce::String& failureReason)
{
    auto makeMsg = [] (const char* role, const juce::String& content)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("role", role);
        m->setProperty ("content", content);
        return juce::var (m);
    };

    // Variety: every request carries a fresh number + a random creative direction + what the last results already used, so neither the model
    // nor a caching gateway can hand back the same sound for the same words.
    static const char* directions[] =
    {
        "favour unusual wavetables and a strongly moving filter", "use heavy unison and detune with a wide stereo image", "keep it clean, dry and simple",
        "use FM or metallic wavetables with warp", "build it around LFO and envelope modulation in the mod matrix", "make it dark, gritty and distorted",
        "make it bright, airy and shimmering with long reverb", "use a short plucky envelope with a rhythmic LFO", "use a slow evolving attack and release",
        "use two filters in series or parallel", "pick a filter type you would not normally use", "layer Osc B, Osc C, sub and noise"
    };
    juce::Random rng (juce::Random::getSystemRandom().nextInt64() ^ juce::Time::currentTimeMillis());
    juce::String user = prompt;
    user << "\n\n[Variation #" << juce::String (rng.nextInt (1000000)) << ": a NEW take - " << directions[rng.nextInt ((int) (sizeof (directions) / sizeof (directions[0])))] << ".";
    if (! recentOnline.isEmpty()) user << " Already used, do NOT reuse these choices: " << recentOnline.joinIntoString (" | ") << ".";
    user << "]";

    auto* body = new juce::DynamicObject();
    if (cfg.model.isNotEmpty()) body->setProperty ("model", cfg.model);
    body->setProperty ("messages", juce::Array<juce::var> { makeMsg ("system", systemPrompt), makeMsg ("user", user) });
    body->setProperty ("temperature", 1.0);
    body->setProperty ("stream", false);

    // OpenAI-compatible. The Authorization header is sent ONLY when a key was configured (keyless: Kilo, Pollinations, Ollama, Off Grid ...).
    juce::String headers = "Content-Type: application/json\r\nAccept: application/json\r\n";
    if (cfg.apiKey.isNotEmpty()) headers << "Authorization: Bearer " << cfg.apiKey << "\r\n";

    workers.erase (std::remove_if (workers.begin(), workers.end(), [] (const std::unique_ptr<juce::Thread>& w) { return ! w->isThreadRunning(); }), workers.end());
    auto res = std::make_shared<HttpResult>();
    workers.push_back (std::make_unique<HttpWorker> (cfg.url, headers, juce::JSON::toString (juce::var (body), true), res));
    workers.back()->startThread();

    if (! res->done.wait ((int) kProviderTimeoutMs + 1500))
    {
        workers.back()->signalThreadShouldExit();                     // abandoned: it ends by itself, its result is simply ignored
        failureReason = "timeout";
        return {};
    }
    const int status = res->status;
    if (status == 401 || status == 403) { failureReason = "auth rejected, HTTP " + juce::String (status); return {}; }
    if (status == 429 || status == 402) { failureReason = "rate limit / quota, HTTP " + juce::String (status); return {}; }
    if (res->error.isNotEmpty())        { failureReason = res->error; return {}; }

    const auto response = juce::JSON::parse (res->body);
    auto content = response["choices"][0]["message"]["content"].toString().trim();
    if (content.isEmpty()) { failureReason = "empty or non-JSON response"; return {}; }

    // Models sometimes wrap the JSON in ``` fences or add prose: take the outermost {...}.
    const int first = content.indexOfChar ('{'), last = content.lastIndexOfChar ('}');
    if (first < 0 || last <= first) { failureReason = "no JSON object in reply"; return {}; }

    auto result = validateOnlinePayload (juce::JSON::parse (content.substring (first, last + 1)), prompt);
    if (! result.isObject()) { failureReason = "invalid or empty params"; return {}; }

    // remember the key choices of this sound (last 3) for the next request
    if (auto* params = result.getDynamicObject()->getProperty ("params").getDynamicObject())
    {
        juce::String sig;
        for (auto* id : { "oscA_wavetable", "oscB_wavetable", "filt1_type", "filt1_cutoff", "env1_attack", "env1_decay", "lfo1_shape", "lfo1_rate" })
            if (params->hasProperty (id))
            {
                const auto v = params->getProperty (id);
                sig << id << "=" << (v.isString() ? v.toString() : juce::String ((double) v, 2)) << " ";
            }
        recentOnline.add (sig.trim());
        while (recentOnline.size() > 3) recentOnline.remove (0);
    }
    return result;
}

/** Keeps only known parameter IDs with valid values (numbers clamped, choice names checked) and needs a minimum of usable entries. */
juce::var PromptBridge::validateOnlinePayload (const juce::var& parsed, const juce::String& prompt) const
{
    auto* root = parsed.getDynamicObject();
    if (root == nullptr) return {};
    auto* params = root->getProperty ("params").getDynamicObject();
    if (params == nullptr) return {};

    auto* clean = new juce::DynamicObject();
    int kept = 0;
    for (const auto& nv : params->getProperties())
    {
        auto it = paramInfo.find (nv.name.toString());
        if (it == paramInfo.end()) continue;
        const auto& info = it->second;

        if (info.isChoice)
        {
            if (nv.value.isString())
            {
                const int idx = info.choices.indexOf (nv.value.toString());
                if (idx < 0) continue;
                clean->setProperty (nv.name, info.choices[idx]);
            }
            else if (nv.value.isInt() || nv.value.isInt64() || nv.value.isDouble())
                clean->setProperty (nv.name, info.choices[juce::jlimit (0, info.choices.size() - 1, (int) nv.value)]);
            else continue;
        }
        else
        {
            double v;
            if (nv.value.isBool())                                                   v = (bool) nv.value ? 1.0 : 0.0;
            else if (nv.value.isInt() || nv.value.isInt64() || nv.value.isDouble())  v = (double) nv.value;
            else continue;
            if (! std::isfinite (v)) continue;
            clean->setProperty (nv.name, juce::jlimit ((double) info.lo, (double) info.hi, v));
        }
        ++kept;
    }

    if (kept < 3) { delete clean; return {}; }          // applyPatchVar resets the whole patch first -> never apply a near-empty one

    // Start-up state: only Osc A is on. A source the model gives a level to is switched on (unless the model set its switch itself).
    struct Src { const char* level; const char* on; };
    const Src sources[] = { { "oscA_level", "oscA_on" }, { "oscB_level", "oscB_on" }, { "oscC_level", "oscC_on" }, { "sub_level", "sub_on" }, { "noise_level", "noise_on" } };
    for (const auto& sc : sources)
        if ((double) clean->getProperty (sc.level) > 0.0 && ! clean->hasProperty (sc.on) && paramInfo.count (sc.on) > 0)
            clean->setProperty (sc.on, 1.0);

    // ---- effects --------------------------------------------------------------------------------------------------------------------
    // Online models often forget the effects (or set only fxN_mix without fxN_on). Make sure the patch always uses them:
    //   1. an effect that got a mix / parameter but no switch is switched on,
    //   2. fewer than 3 effects on -> the built-in designer adds the ones that fit the prompt (the model's own choices are kept),
    //   3. "on" without "in rack" is shown in the rack too.
    auto fxSlotOf = [] (const juce::String& id) -> int
    {
        if (id.length() < 4 || ! id.startsWith ("fx") || ! juce::CharacterFunctions::isDigit (id[2])) return -1;
        int d = 2;
        while (d < id.length() && juce::CharacterFunctions::isDigit (id[d])) ++d;
        if (d >= id.length() || id[d] != '_') return -1;
        const int slot = id.substring (2, d).getIntValue() - 1;
        return (slot >= 0 && slot < kNumFxSlots) ? slot : -1;
    };
    {
        std::vector<bool> touched ((size_t) kNumFxSlots, false), hasOn ((size_t) kNumFxSlots, false);
        bool modelOrdered = false;
        for (const auto& nv : clean->getProperties())
        {
            const auto key = nv.name.toString();
            const int slot = fxSlotOf (key);
            if (slot < 0) continue;
            if (key.endsWith ("_on")) hasOn[(size_t) slot] = true;
            else if (key.endsWith ("_order")) modelOrdered = true;
            else if (! key.endsWith ("_added")) touched[(size_t) slot] = true;
        }
        for (int i = 0; i < kNumFxSlots; ++i)
        {
            const auto mixId = ids::fx (i, "mix");
            const bool mixZero = clean->hasProperty (mixId) && (double) clean->getProperty (mixId) <= 0.0;
            if (touched[(size_t) i] && ! hasOn[(size_t) i] && ! mixZero && paramInfo.count (ids::fx (i, "on")) > 0)
                clean->setProperty (ids::fx (i, "on"), 1.0);
        }

        std::vector<bool> isOn ((size_t) kNumFxSlots, false);
        int enabled = 0;
        for (int i = 0; i < kNumFxSlots; ++i)
            if ((double) clean->getProperty (ids::fx (i, "on")) > 0.5) { isOn[(size_t) i] = true; ++enabled; }

        constexpr int kWantedFx = 3;
        if (enabled < kWantedFx)
        {
            juce::Random rng (juce::Random::getSystemRandom().nextInt64() ^ (juce::int64) prompt.hashCode64());
            auto design = designSound (prompt, rng);
            const auto designed = design.patch.toVar();
            juce::DynamicObject* dp = nullptr;
            if (auto* dRoot = designed.getDynamicObject()) dp = dRoot->getProperty ("params").getDynamicObject();
            if (dp != nullptr)
            {
                std::vector<int> candidates;                              // effects the designer switched on that the model did not use
                for (const auto& nv : dp->getProperties())
                {
                    const auto key = nv.name.toString();
                    const int slot = fxSlotOf (key);
                    if (slot >= 0 && key.endsWith ("_on") && (double) nv.value > 0.5 && ! isOn[(size_t) slot]) candidates.push_back (slot);
                }
                std::sort (candidates.begin(), candidates.end());
                std::vector<bool> take ((size_t) kNumFxSlots, false);
                for (int slot : candidates)
                {
                    if (enabled >= kWantedFx) break;
                    take[(size_t) slot] = true; ++enabled;
                }
                for (const auto& nv : dp->getProperties())
                {
                    const auto key = nv.name.toString();
                    const int slot = fxSlotOf (key);
                    if (slot < 0 || paramInfo.count (key) == 0) continue;
                    const bool isOrder = key.endsWith ("_order");
                    if (isOrder ? modelOrdered : ! take[(size_t) slot]) continue;       // chain order only when the model gave none
                    if (nv.value.isString()) clean->setProperty (nv.name, nv.value);
                    else clean->setProperty (nv.name, (double) nv.value);
                }
            }
        }
    }
    for (int i = 0; i < kNumFxSlots; ++i)
    {
        const auto on = ids::fx (i, "on"), added = ids::fx (i, "added");
        if ((double) clean->getProperty (on) > 0.5 && ! clean->hasProperty (added) && paramInfo.count (added) > 0)
            clean->setProperty (added, 1.0);
    }

    // drawn (custom) LFO shapes and macro names: lfoShape1..6 = "x:y;x:y;..." (checked by LfoCustom), macroName1..8 = short text
    auto* shapesOut = new juce::DynamicObject();
    if (auto* sh = root->getProperty ("shapes").getDynamicObject())
        for (const auto& nv : sh->getProperties())
        {
            const auto key = nv.name.toString();
            const auto val = nv.value.toString().trim();
            if (val.isEmpty()) continue;
            for (int l = 0; l < kNumLfos; ++l)
                if (key == "lfoShape" + juce::String (l + 1))
                {
                    LfoCustom c;
                    if (! c.fromString (val)) continue;
                    for (int i = 0; i < c.n; ++i)                                              // models are sloppy: clamp and keep x rising
                    {
                        c.y[i] = juce::jlimit (0.0f, 1.0f, c.y[i]);
                        c.x[i] = juce::jlimit (0.0f, 1.0f, i > 0 ? juce::jmax (c.x[i], c.x[i - 1]) : c.x[i]);
                    }
                    shapesOut->setProperty (nv.name, c.toString());
                    const auto sid = ids::lfo (l, "shape");
                    if (paramInfo.count (sid) > 0) clean->setProperty (sid, "Custom");        // a drawn shape is only used by the Custom shape type
                }
            for (int m = 0; m < kNumMacros; ++m)
                if (key == "macroName" + juce::String (m + 1))
                    shapesOut->setProperty (nv.name, val.substring (0, 16));
        }

    auto* out = new juce::DynamicObject();
    out->setProperty ("params", juce::var (clean));
    if (shapesOut->getProperties().size() > 0) out->setProperty ("shapes", juce::var (shapesOut)); else delete shapesOut;
    out->setProperty ("summary", root->getProperty ("summary").toString().trim().substring (0, 160));
    return juce::var (out);
}

juce::var PromptBridge::keywordFallback (const juce::String& prompt)
{
    // Every generation starts from scratch: a fresh random seed and a structure that differs from the previous sound
    // (wavetables, warp, filter types, effect types, LFO shapes ...). Several candidates are designed and the least similar one wins.
    juce::Random rng (juce::Random::getSystemRandom().nextInt64() ^ juce::Time::currentTimeMillis() ^ (juce::int64) prompt.hashCode64());

    auto structureOf = [] (const juce::var& v)
    {
        std::map<juce::String, juce::String> m;
        if (auto* root = v.getDynamicObject())
            if (auto* params = root->getProperty ("params").getDynamicObject())
                for (const auto& nv : params->getProperties())
                {
                    if (nv.value.isString()) { m[nv.name.toString()] = nv.value.toString(); continue; }
                    // cutoff / envelope times / LFO rate are bucketed per 1/2 octave so "same cutoff + envelope as last time" counts as similar
                    const auto id = nv.name.toString();
                    if ((id.endsWith ("_cutoff") || id.endsWith ("_attack") || id.endsWith ("_decay") || id.endsWith ("_release") || id.endsWith ("_rate")) && (double) nv.value > 1.0e-5)
                        m[id] = "#" + juce::String (juce::roundToInt (std::log2 ((double) nv.value) * 2.0));
                    else if (id.endsWith ("_sustain") || id.endsWith ("_res"))
                        m[id] = "#" + juce::String (juce::roundToInt ((double) nv.value * 4.0));
                }
        return m;
    };
    auto similarity = [] (const std::map<juce::String, juce::String>& a, const std::map<juce::String, juce::String>& b)
    {
        if (a.empty() || b.empty()) return 0.0f;
        int same = 0, total = 0;
        for (const auto& kv : a)
        {
            auto it = b.find (kv.first);
            if (it == b.end()) continue;
            ++total;
            if (it->second == kv.second) ++same;
        }
        return total > 0 ? (float) same / (float) total : 0.0f;
    };

    juce::var best; juce::String bestSummary; float bestSim = 2.0f;
    for (int attempt = 0; attempt < 14; ++attempt)
    {
        auto design = designSound (prompt, rng);
        auto v = design.patch.toVar();
        float sim = 0.0f;
        for (const auto& old : history) sim = std::max (sim, similarity (structureOf (v), old));
        if (sim < bestSim) { bestSim = sim; best = v; bestSummary = design.summary; }
        if (bestSim < 0.4f) break;
    }
    history.push_back (structureOf (best));
    if (history.size() > 3) history.erase (history.begin());
    if (auto* o = best.getDynamicObject()) o->setProperty ("summary", bestSummary);
    return best;
}

void PromptBridge::applyPreset (const juce::var& payload)
{
    JUCE_ASSERT_MESSAGE_THREAD
    applyPatchVar (apvts, payload);
}
}
