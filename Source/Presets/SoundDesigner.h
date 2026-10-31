#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "Patch.h"

namespace nusynth
{
struct Design { Patch patch; juce::String summary; };

/** AI sound designer (offline). It does NOT pick a factory preset: it reads the prompt into a vector of sound traits
    (brightness, aggression, width, motion, attack, length, noise, metal, vocal, space, grit, rhythm ...), then composes a brand
    new patch from those traits - wavetables chosen by affinity, warp / filter type / envelopes / LFOs / mod matrix / macros /
    FX chain / arp all derived from the description. Every generation is different, but always close to the words you typed. */
namespace sd
{
enum Trait { BRIGHT, DARK, WARM, AGGR, WIDE, MOTION, SLOW, FAST, LENGTHY, BRIEF, HIGH, LOW, NOISE, METAL, VOCAL, PLUCK, SUB, SPACE, ECHO,
             GRIT, RHYTHM, LOFI, DIGITAL, ANALOG, GLIDE, MONO, VIB, WOBBLE, SWEEP, RESO, LOUD, DETUNE, PHASER, FLANGER, CHORUS,
             HOLLOW, TAPE, FUZZ, FOLD, TUBE, CLEAN, R_BASS, R_KEYS, R_PLUCK, R_PAD, R_LEAD, R_FX, R_ARP, NT };

struct Group { const char* words; int t[4]; float v[4]; };

inline const Group* lexicon (int& n)
{
    static const Group g[] = {
        // ---------------------------------------------------------------- roles
        { "bass basses low reese bottom subbass",                      { R_BASS, LOW, SUB, -1 },          { 1.0f, 0.6f, 0.3f, 0 } },
        { "808 boom thump kick",                                       { R_BASS, SUB, BRIEF, -1 },        { 0.8f, 0.9f, 0.4f, 0 } },
        { "sub subby",                                                 { SUB, LOW, -1, -1 },              { 0.8f, 0.4f, 0, 0 } },
        { "keys key piano rhodes electric ep clav clavinet wurly wurlitzer harpsichord organ celesta vibraphone",
                                                                       { R_KEYS, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "marimba xylophone mallet",                                  { R_KEYS, PLUCK, METAL, -1 },      { 0.7f, 0.7f, 0.3f, 0 } },
        { "pluck plucked plucky pizzicato pizz stab staccato kalimba harp koto guitar banjo",
                                                                       { R_PLUCK, PLUCK, BRIEF, -1 },     { 1.0f, 0.9f, 0.5f, 0 } },
        { "pad pads atmosphere atmospheric ambient wash drone strings ensemble soundscape texture cinematic",
                                                                       { R_PAD, SLOW, LENGTHY, SPACE },      { 1.0f, 0.5f, 0.6f, 0.4f } },
        { "lead leads solo melody melodic screamer hoover whistle brass supersaw",
                                                                       { R_LEAD, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "fx sfx riser impact laser zap glitch scifi alien whoosh swoosh siren rumble effect effects",
                                                                       { R_FX, MOTION, -1, -1 },          { 1.0f, 0.4f, 0, 0 } },
        { "arp arps arpeggio arpeggiated arpeggiator sequence sequenced sequencer pattern",
                                                                       { R_ARP, RHYTHM, -1, -1 },         { 1.0f, 0.5f, 0, 0 } },
        // ---------------------------------------------------------------- tone
        { "bright sharp crisp shiny sparkly sparkling brilliant clear glassy glass crystal icy shimmer shimmering piercing",
                                                                       { BRIGHT, -1, -1, -1 },            { 0.8f, 0, 0, 0 } },
        { "airy light thin delicate",                                  { BRIGHT, NOISE, HIGH, -1 },       { 0.4f, 0.2f, 0.3f, 0 } },
        { "dark murky muffled dull dim gloomy sinister moody",         { DARK, -1, -1, -1 },              { 0.8f, 0, 0, 0 } },
        { "deep heavy",                                                { DARK, LOW, -1, -1 },             { 0.5f, 0.5f, 0, 0 } },
        { "warm mellow smooth round gentle velvet cozy soft",          { WARM, DARK, AGGR, -1 },          { 0.8f, 0.2f, -0.3f, 0 } },
        { "aggressive harsh angry nasty brutal savage violent evil mean",
                                                                       { AGGR, GRIT, BRIGHT, -1 },        { 0.9f, 0.5f, 0.2f, 0 } },
        { "dirty gritty distorted distortion crunchy crunch overdriven overdrive filthy",
                                                                       { GRIT, AGGR, -1, -1 },            { 0.9f, 0.4f, 0, 0 } },
        { "fuzz fuzzy",                                                { FUZZ, GRIT, -1, -1 },            { 1.0f, 0.6f, 0, 0 } },
        { "tube valve",                                                { TUBE, WARM, GRIT, -1 },          { 1.0f, 0.4f, 0.3f, 0 } },
        { "fold folded wavefold",                                      { FOLD, AGGR, DIGITAL, -1 },       { 1.0f, 0.4f, 0.4f, 0 } },
        { "clean pure simple plain minimal sine",                      { CLEAN, WARM, GRIT, MOTION },     { 1.0f, 0.3f, -0.8f, -0.5f } },
        { "hollow nasal woody",                                        { HOLLOW, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "resonant squelch squelchy acid screech singing whistling",  { RESO, SWEEP, -1, -1 },           { 0.9f, 0.4f, 0, 0 } },
        { "loud powerful strong slammed compressed punchy huge impactful",
                                                                       { LOUD, FAST, -1, -1 },            { 0.8f, 0.3f, 0, 0 } },
        { "metallic metal bell bells clang clanky steel iron robotic robot mechanical inharmonic gong industrial",
                                                                       { METAL, DIGITAL, -1, -1 },        { 0.9f, 0.3f, 0, 0 } },
        { "vocal voice choir vowel human formant singing sing aah ooh talking choral angelic",
                                                                       { VOCAL, -1, -1, -1 },             { 0.9f, 0, 0, 0 } },
        { "noisy noise breath breathy hiss static wind whisper dust grain grainy",
                                                                       { NOISE, -1, -1, -1 },             { 0.8f, 0, 0, 0 } },
        { "vinyl crackle tape cassette dusty worn",                    { TAPE, NOISE, LOFI, -1 },         { 1.0f, 0.4f, 0.5f, 0 } },
        { "lofi vintage retro old 8bit bitcrushed chiptune",           { LOFI, ANALOG, GRIT, -1 },        { 0.9f, 0.3f, 0.3f, 0 } },
        { "digital modern futuristic cyber cyberpunk electronic synthetic hightech",
                                                                       { DIGITAL, ANALOG, -1, -1 },       { 0.9f, -0.5f, 0, 0 } },
        { "analog classic oldschool moog juno",                        { ANALOG, DIGITAL, WARM, -1 },     { 0.9f, -0.5f, 0.3f, 0 } },
        { "high shrill squeaky tiny small piping treble",              { HIGH, BRIGHT, -1, -1 },          { 0.8f, 0.3f, 0, 0 } },
        { "lowend rumbling rumble growl",                              { LOW, SUB, -1, -1 },              { 0.7f, 0.4f, 0, 0 } },
        // ---------------------------------------------------------------- space / width
        { "wide huge big massive fat thick epic stereo spacious broad giant",
                                                                       { WIDE, DETUNE, LOUD, -1 },        { 0.9f, 0.5f, 0.2f, 0 } },
        { "lush rich full",                                            { WIDE, CHORUS, WARM, -1 },        { 0.6f, 0.5f, 0.3f, 0 } },
        { "narrow tight focused mono monophonic",                      { WIDE, MONO, -1, -1 },            { -0.7f, 0.6f, 0, 0 } },
        { "detuned unison ensemble",                                   { DETUNE, WIDE, -1, -1 },          { 0.9f, 0.4f, 0, 0 } },
        { "chorus chorused",                                           { CHORUS, WIDE, -1, -1 },          { 1.0f, 0.3f, 0, 0 } },
        { "spacey space cathedral hall reverb reverby cavernous ethereal dreamy floating distant heavenly celestial",
                                                                       { SPACE, SLOW, LENGTHY, -1 },         { 0.9f, 0.2f, 0.3f, 0 } },
        { "echo delay echoing repeats pingpong",                       { ECHO, SPACE, -1, -1 },           { 0.9f, 0.2f, 0, 0 } },
        { "dry tight close",                                           { SPACE, ECHO, -1, -1 },           { -0.7f, -0.7f, 0, 0 } },
        { "phaser phasing swirl swirling",                             { PHASER, MOTION, -1, -1 },        { 1.0f, 0.4f, 0, 0 } },
        { "flanger flanging jet jetplane",                             { FLANGER, MOTION, -1, -1 },       { 1.0f, 0.4f, 0, 0 } },
        // ---------------------------------------------------------------- time / motion
        { "evolving moving morphing animated alive motion shifting changing dynamic living breathing shape shifting",
                                                                       { MOTION, -1, -1, -1 },            { 0.9f, 0, 0, 0 } },
        { "static frozen still",                                       { MOTION, -1, -1, -1 },            { -0.9f, 0, 0, 0 } },
        { "wobble wub womp dubstep growly",                            { WOBBLE, RHYTHM, AGGR, -1 },      { 1.0f, 0.4f, 0.3f, 0 } },
        { "pulsing gated chopped stutter pumping bouncing rhythmic trancegate",
                                                                       { RHYTHM, -1, -1, -1 },            { 0.9f, 0, 0, 0 } },
        { "sweep sweeping swept filtersweep rising falling",           { SWEEP, SLOW, -1, -1 },           { 0.9f, 0.4f, 0, 0 } },
        { "vibrato expressive trembling",                              { VIB, -1, -1, -1 },               { 0.9f, 0, 0, 0 } },
        { "glide portamento slide sliding legato",                     { GLIDE, MONO, -1, -1 },           { 0.9f, 0.4f, 0, 0 } },
        { "slow swell fade gradual creeping soft-attack",              { SLOW, -1, -1, -1 },              { 0.9f, 0, 0, 0 } },
        { "fast snappy instant percussive hit",                        { FAST, BRIEF, -1, -1 },           { 0.9f, 0.3f, 0, 0 } },
        { "long sustained lingering endless infinite ringing drawn tail",
                                                                       { LENGTHY, -1, -1, -1 },              { 0.9f, 0, 0, 0 } },
        { "short quick brief clipped tight-release",                   { BRIEF, FAST, -1, -1 },           { 0.9f, 0.3f, 0, 0 } },
        // ---------------------------------------------------------------- moods
        { "sad melancholic melancholy lonely cold",                    { DARK, WARM, SLOW, SPACE },       { 0.5f, 0.4f, 0.3f, 0.3f } },
        { "happy cheerful joyful sunny playful bouncy",                { BRIGHT, FAST, -1, -1 },          { 0.6f, 0.3f, 0, 0 } },
        { "scary creepy horror eerie spooky haunting haunted",         { DARK, METAL, NOISE, SPACE },     { 0.6f, 0.4f, 0.3f, 0.4f } },
        { "underwater ocean deep-sea",                                 { DARK, MOTION, SPACE, WARM },     { 0.5f, 0.6f, 0.5f, 0.3f } },
        { "cyberpunk blade runner futuristic-dark",                    { DIGITAL, AGGR, DARK, WIDE },     { 0.7f, 0.4f, 0.4f, 0.4f } },
        { "80s eighties synthwave outrun",                             { ANALOG, WIDE, CHORUS, LOFI },    { 0.7f, 0.6f, 0.6f, 0.2f } },
        { "angel angelic heaven",                                      { VOCAL, SPACE, BRIGHT, WIDE },    { 0.6f, 0.7f, 0.5f, 0.4f } },
        { "demonic devil",                                             { AGGR, DARK, VOCAL, GRIT },       { 0.8f, 0.7f, 0.3f, 0.5f } },
        { "organic natural acoustic",                                  { ANALOG, NOISE, WARM, -1 },       { 0.5f, 0.2f, 0.3f, 0 } },
        { "magical sparkle sparkly fairy twinkle",                     { BRIGHT, METAL, SPACE, HIGH },    { 0.6f, 0.4f, 0.4f, 0.4f } },
    };
    n = (int) (sizeof (g) / sizeof (g[0]));
    return g;
}

/** wavetable affinity: bright, soft, aggressive, metallic, vocal, evolving, digital, pure  (0-9 each) */
inline const char* tableTraits (int i)
{
    static const char* t[73] = {
        "62300010","61200021","32100029","10100009","52300021","62400021","83301100","72300110","34000041","44100021","42100031","31100029",   // classic
        "75105410","41000131","32005141","53007510","42100131","42100221","91005410","44007621","83502930","44090421","54090521","42000131",   // digital / additive
        "86009510","52000421","53200921","83807610","85809310","74005510","63007520","65007620","65906710","62009510","73509710","78909910",   // FM
        "86908810","76908720","74908910","86909810","74808700","75907720","54009510","66808900","85809910","85909810",                           // harsh / spectral
        "32001220","62101210","52102120","62100320","34006710","43097510","53305610","85809710","43007920","44005520",                           // morphs
        "34090420","34090420","34090420","34090420","34090420","34097620","44090420","54990430","33090520",                                      // vowels
        "11000009","11000009","11000009","11000009","11000009","11000009","11000009","11000009" };                                              // user
    return t[std::min (72, std::max (0, i))];
}
}

inline Design designSound (const juce::String& prompt, juce::Random& rng)
{
    using namespace sd;
    auto rnd = [&rng] (float lo, float hi) { return lo + (hi - lo) * rng.nextFloat(); };
    auto clamp01 = [] (float v) { return std::min (1.0f, std::max (0.0f, v)); };

    // ------------------------------------------------------------ 1) prompt -> trait vector
    std::array<float, NT> t {};
    juce::String clean;
    for (auto c : prompt.toLowerCase()) clean += (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : (c == '-' || c == '\'') ? juce::String() : juce::String (" "));
    juce::StringArray tokens;
    tokens.addTokens (clean, " ", "");
    tokens.removeEmptyStrings();

    static const char* strong[] = { "very", "extremely", "super", "ultra", "so", "really", "hugely", "insanely", "massively", "crazy", "totally" };
    static const char* weak[]   = { "slightly", "bit", "little", "subtle", "subtly", "touch", "mildly", "kinda", "somewhat", "hint" };
    static const char* negate[] = { "not", "no", "without", "non", "less", "never", "zero" };
    auto isIn = [] (const juce::String& s, const char* const* list, int n) { for (int i = 0; i < n; ++i) if (s == list[i]) return true; return false; };

    int nGroups = 0;
    const Group* groups = lexicon (nGroups);
    float mult = 1.0f;
    int matched = 0;
    juce::StringArray understood;
    for (const auto& tok : tokens)
    {
        if (isIn (tok, strong, 11)) { mult = 1.7f; continue; }
        if (isIn (tok, weak, 10))   { mult = 0.5f; continue; }
        if (isIn (tok, negate, 7))  { mult = -1.0f; continue; }
        if (tok == "more") { mult = 1.3f; continue; }

        bool hit = false;
        for (int gi = 0; gi < nGroups; ++gi)
        {
            juce::StringArray words;
            words.addTokens (groups[gi].words, " ", "");
            for (const auto& w : words)
            {
                const bool match = tok == w || (tok.length() >= 4 && w.startsWith (tok)) || (w.length() >= 4 && tok.startsWith (w));
                if (! match) continue;
                for (int k = 0; k < 4; ++k)
                    if (groups[gi].t[k] >= 0) t[(size_t) groups[gi].t[k]] += groups[gi].v[k] * mult;
                hit = true;
                break;
            }
        }
        if (hit) { ++matched; understood.addIfNotAlreadyThere (tok); mult = 1.0f; }
    }

    // unknown words still shape the sound: hash them into a gentle, repeatable colouring
    if (matched == 0)
    {
        const auto h = (juce::uint32) prompt.hashCode();
        t[BRIGHT] += (float) (h % 7) / 10.0f; t[MOTION] += (float) ((h >> 3) % 6) / 10.0f; t[WIDE] += (float) ((h >> 6) % 7) / 10.0f;
        t[R_PAD + (int) ((h >> 9) % 5)] += 1.0f;
    }

    for (auto& v : t) v = std::min (2.0f, v);

    // ------------------------------------------------------------ 2) role
    int role = R_PAD;
    {
        float best = 0.0f;
        for (int r = R_BASS; r <= R_ARP; ++r) if (t[(size_t) r] > best) { best = t[(size_t) r]; role = r; }
        if (best <= 0.0f)
        {
            if (t[LOW] > 0.4f || t[SUB] > 0.4f) role = R_BASS;
            else if (t[SPACE] + t[SLOW] > 0.6f) role = R_PAD;
            else if (t[PLUCK] + t[BRIEF] > 0.6f) role = R_PLUCK;
            else if (t[AGGR] + t[BRIGHT] > 0.8f) role = R_LEAD;
            else { static const int pool[5] = { R_PAD, R_LEAD, R_PLUCK, R_KEYS, R_BASS }; role = pool[rng.nextInt (5)]; }
        }
    }
    const bool isBass = role == R_BASS, isKeys = role == R_KEYS, isPluck = role == R_PLUCK, isPad = role == R_PAD, isLead = role == R_LEAD, isFx = role == R_FX, isArp = role == R_ARP;
    const bool wantArp = t[R_ARP] > 0.3f || isArp;

    const float bright = std::max (-1.0f, std::min (1.0f, t[BRIGHT] - t[DARK] - 0.3f * t[WARM] + (isBass ? -0.4f : (isLead ? 0.3f : (isPluck ? 0.1f : 0.0f)))));
    const float wide = clamp01 (t[WIDE] + (isPad ? 0.45f : 0.0f) + (isLead ? 0.15f : 0.0f) + 0.3f * t[CHORUS] + 0.3f * t[DETUNE]);
    const float motion = clamp01 (t[MOTION] + (isPad || isFx ? 0.35f : 0.0f) + 0.2f * t[PHASER]);
    const float aggr = clamp01 (t[AGGR] + 0.4f * t[GRIT] + 0.4f * t[FUZZ] + 0.3f * t[FOLD]);
    const float space = clamp01 (t[SPACE] + (isPad ? 0.35f : (isFx ? 0.4f : (isPluck || isKeys ? 0.15f : 0.1f))));
    const float echo = clamp01 (t[ECHO] + (isLead || isArp ? 0.25f : (isPluck ? 0.15f : 0.0f)));

    juce::StringArray notes;
    static const char* roleNames[7] = { "bass", "keys", "pluck", "pad", "lead", "fx", "arp" };
    notes.add (juce::String ("role: ") + roleNames[role - R_BASS]);
    if (understood.size() > 0) notes.add ("heard: " + understood.joinIntoString (" "));

    Patch p;

    // ------------------------------------------------------------ 3) oscillators (chosen by affinity with the traits)
    const int nTables = 65;
    float want[8] = { std::max (0.0f, bright) + 0.5f * t[HIGH], t[WARM] + std::max (0.0f, -bright) + 0.5f * t[CLEAN], aggr + 0.3f * t[FOLD], t[METAL] + 0.2f * t[DIGITAL],
                      t[VOCAL], motion + (isPad ? 0.3f : 0.0f), std::max (0.0f, t[DIGITAL] - t[ANALOG]), (isBass ? 0.5f : 0.0f) + t[CLEAN] + 0.4f * t[SUB] };
    struct Scored { int idx; float s; };
    std::vector<Scored> ranked;
    for (int i = 0; i < nTables; ++i)
    {
        const char* tr = tableTraits (i);
        float s = 0.0f;
        for (int k = 0; k < 8; ++k) s += want[k] * (float) (tr[k] - '0') / 9.0f;
        if (isBass && (i == 3 || i == 0 || i == 1 || i == 26)) s += 0.25f;
        if (isKeys && (i == 25 || i == 13 || i == 23 || i == 24)) s += 0.25f;
        if (isPluck && (i == 33 || i == 4 || i == 5)) s += 0.25f;
        if (isPad && (i == 8 || i == 19 || i == 54 || i == 9)) s += 0.2f;
        if (isLead && (i == 6 || i == 0 || i == 1)) s += 0.2f;
        if (t[VOCAL] > 0.4f && (i >= 56 || i == 14 || i == 21 || i == 22)) s += 0.5f;
        ranked.push_back ({ i, s + rnd (0.0f, 0.55f) });
    }
    std::sort (ranked.begin(), ranked.end(), [] (const Scored& a, const Scored& b) { return a.s > b.s; });
    const auto names = wavetableNames();
    const int wtA = ranked[0].idx;
    int wtB = ranked[1 + rng.nextInt (3)].idx;
    if (wtB == wtA) wtB = ranked[4].idx;
    const int wtC = ranked[2 + rng.nextInt (4)].idx;

    const int octShift = (t[HIGH] > 0.5f ? 12 : 0) + (isBass ? -12 : 0) + (t[LOW] > 0.7f && ! isBass ? -12 : 0);
    const int uni = std::max (1, std::min (9, 1 + (int) std::lround (wide * 6.0f + t[DETUNE] * 2.0f - (isBass ? 2.0f : 0.0f) - (t[CLEAN] > 0.4f ? 2.0f : 0.0f))));
    const float det = 0.12f + 0.45f * wide + rnd (0.0f, 0.12f);
    const float spreadAmt = 0.35f + 0.6f * wide;

    p.osc (0, names[wtA].toRawUTF8(), 0.8f, octShift, rnd (-3.0f, 3.0f), uni, det, spreadAmt, rnd (0.25f, 0.9f), 0.0f);
    p.osc (1, names[wtB].toRawUTF8(), isBass ? 0.35f : rnd (0.3f, 0.55f), octShift + (isBass ? 0 : (rng.nextBool() ? 12 : 0)), rnd (-9.0f, 9.0f),
           std::max (1, uni / 2), det, spreadAmt, rnd (0.1f, 0.9f), 0.0f);
    if (wide > 0.55f || isPad || t[METAL] > 0.5f || t[VOCAL] > 0.5f)
        p.osc (2, names[wtC].toRawUTF8(), rnd (0.2f, 0.4f), octShift + (isPad && rng.nextBool() ? 12 : (isBass ? 0 : -12 * (int) rng.nextInt (2))), rnd (-9.0f, 9.0f), std::max (1, uni / 2), det, spreadAmt, rnd (0.1f, 0.9f), 0.0f);
    notes.add ("waves: " + names[wtA] + " / " + names[wtB]);

    // warp
    if (t[METAL] > 0.5f)                       { p.warp (0, rng.nextBool() ? "FM" : "RM", rnd (0.2f, 0.55f)); p.n (ids::osc (0, "warpSrc"), 1.0f); notes.add ("FM warp"); }
    else if (aggr > 0.55f || t[FOLD] > 0.4f)   { const char* m[3] = { "Wavefold", "Asym +/-", "Quantize" }; p.warp (0, m[rng.nextInt (3)], rnd (0.25f, 0.65f) * (0.5f + aggr)); notes.add ("warp drive"); }
    else if (t[LOFI] > 0.5f || t[DIGITAL] > 0.6f) { p.warp (0, "Quantize", rnd (0.3f, 0.7f)); }
    else if (motion > 0.45f)                   { const char* m[4] = { "Bend +/-", "PWM", "Mirror", "Remap" }; p.warp (0, m[rng.nextInt (4)], rnd (0.15f, 0.45f)); }

    // sub / noise
    const float subLvl = clamp01 (t[SUB] * 0.7f + (isBass ? 0.55f : 0.0f) + (t[LOW] > 0.5f ? 0.2f : 0.0f));
    if (subLvl > 0.05f) { p.sub (isBass && aggr > 0.5f ? "SAW" : "SIN", subLvl, -1, 0.0f); if (isBass) p.route ("sub", "Bypass"); }
    const float noiseAmt = clamp01 (t[NOISE] * 0.45f + (isFx ? 0.25f : 0.0f) + t[TAPE] * 0.2f);
    if (noiseAmt > 0.03f)
    {
        const char* nt = t[TAPE] > 0.4f ? "Analog Tape" : (t[METAL] + t[GRIT] > 1.0f ? "Circuit Noise" : (t[LOW] > 0.5f ? "Brown" : "Pink"));
        p.noise (nt, noiseAmt, t[BRIGHT] > 0.4f ? 9000.0f : 5000.0f, true, t[LOW] < 0.3f, (int) std::lround (rnd (-5.0f, 5.0f)), rnd (-0.4f, 0.4f));
        p.route ("noise", "Filter 1");
        notes.add ("noise");
    }

    // ------------------------------------------------------------ 4) filter
    const float cutoff = 220.0f * std::pow (2.0f, 6.2f * (0.5f + 0.5f * bright)) * rnd (0.9f, 1.12f);
    const char* ftype = "Ladder LP 24";
    if (t[VOCAL] > 0.45f)              ftype = rng.nextBool() ? "Formant I" : "Formant II";
    else if (t[METAL] > 0.55f)         ftype = rng.nextBool() ? "Comb +" : "Comb -";
    else if (t[PHASER] > 0.5f)         ftype = "Phaser 8";
    else if (aggr > 0.85f)             ftype = rng.nextBool() ? "Dist Comb" : "Diode Ladder";
    else if (aggr > 0.5f)              ftype = "Diode Ladder";
    else if (t[HOLLOW] > 0.4f)         ftype = "Notch 12";
    else if (t[WARM] > 0.5f || t[DARK] > 0.5f) ftype = rng.nextBool() ? "MG Low" : "Low Pass 24";
    else if (bright > 0.4f)            ftype = rng.nextBool() ? "Low Pass 12" : "SVF Low Pass";
    const bool formantLike = t[VOCAL] > 0.45f;
    const float fcut = formantLike ? rnd (700.0f, 2200.0f) : std::min (19000.0f, cutoff);
    const float res = clamp01 (0.08f + 0.5f * t[RESO] + 0.15f * aggr + rnd (0.0f, 0.08f));
    p.filter (0, ftype, fcut, res, aggr * 0.45f);
    p.routeAll ("Filter 1");
    if (isBass) p.route ("sub", "Bypass");
    if (isLead || isPluck || isKeys || wantArp) p.n (ids::filter (0, "keytrack"), 1.0f);
    notes.add (juce::String ("filter: ") + ftype);

    // ------------------------------------------------------------ 5) envelopes
    const float atkAxis = std::max (-1.0f, std::min (1.0f, t[SLOW] - t[FAST] + (isPad ? 0.45f : (isPluck ? -0.9f : (isBass ? -0.55f : (isKeys ? -0.6f : (isFx ? 0.2f : -0.2f)))))));
    const float relAxis = std::max (-1.0f, std::min (1.0f, t[LENGTHY] - t[BRIEF] + (isPad ? 0.55f : (isPluck ? -0.2f : (isBass ? -0.3f : (isKeys ? 0.1f : 0.0f))))));
    const float aTime = 0.0015f * std::pow (2000.0f, 0.5f * (atkAxis + 1.0f));
    const float rTime = 0.06f * std::pow (80.0f, 0.5f * (relAxis + 1.0f));
    float sustain = isPluck ? rnd (0.0f, 0.12f) : std::max (0.0f, std::min (1.0f, 0.65f + 0.3f * t[LENGTHY] - 0.5f * t[BRIEF] + (isPad ? 0.15f : 0.0f)));
    float decay = isPluck ? rnd (0.15f, 0.45f) * (1.0f + 0.8f * t[LENGTHY] - 0.5f * t[BRIEF]) : rnd (0.35f, 1.1f);
    if (isKeys) { sustain = std::min (sustain, 0.45f); decay = rnd (0.7f, 1.8f); }
    if (isFx && t[SWEEP] > 0.3f) sustain = 1.0f;
    p.env (0, std::min (aTime, 6.0f), std::max (0.05f, decay), sustain, std::min (rTime, 8.0f));

    const float envAmt = clamp01 (0.12f + 0.45f * t[PLUCK] + 0.4f * t[SWEEP] + 0.25f * aggr + (isPluck || isKeys ? 0.15f : 0.0f));
    p.env (1, t[SWEEP] > 0.3f ? rnd (1.5f, 3.5f) : 0.002f, std::max (0.12f, decay * 0.8f), t[SWEEP] > 0.3f ? 0.6f : 0.12f, std::min (rTime, 3.0f));
    p.mod ("Env 2", "Filter 1 Cutoff", envAmt, false);
    p.mod ("Velocity", "Filter 1 Cutoff", rnd (0.08f, 0.2f), false);
    p.mod ("Velocity", "Osc A Level", rnd (0.05f, 0.15f), false);
    if (motion > 0.4f || isPad)
    {   // slow envelope opens the wavetable
        p.env (2, rnd (0.8f, 3.0f), rnd (1.0f, 4.0f), rnd (0.2f, 0.6f), 2.0f);
        p.mod ("Env 3", "Osc A Position", rnd (0.25f, 0.55f), false);
    }
    if (aggr > 0.5f) { p.env (3, 0.002f, rnd (0.1f, 0.4f), 0.0f, 0.2f); p.mod ("Env 4", "Filter 1 Drive", rnd (0.2f, 0.45f), false); }

    // ------------------------------------------------------------ 6) LFOs (up to 6) + mod matrix
    if (t[WOBBLE] > 0.3f || t[RHYTHM] > 0.4f)
    {
        const char* div = t[FAST] > 0.4f ? "1/16" : (t[SLOW] > 0.4f ? "1/4" : (rng.nextBool() ? "1/8" : "1/16"));
        p.lfo (0, t[WOBBLE] > 0.3f ? "Sine" : (rng.nextBool() ? "Square" : "Saw"), 2.0f, 1.0f, true, div, t[WOBBLE] > 0.3f ? 0.0f : 0.1f);
        p.mod ("LFO 1", "Filter 1 Cutoff", rnd (0.45f, 0.8f), true);
        if (t[WOBBLE] > 0.5f) p.mod ("LFO 1", "Osc A Warp", rnd (0.15f, 0.4f), true);
        notes.add ("synced LFO " + juce::String (div));
    }
    if (motion > 0.25f)
    {
        p.lfo (1, "Triangle", rnd (0.07f, 0.4f));
        p.mod ("LFO 2", "Osc A Position", rnd (0.3f, 0.65f) * (0.4f + motion), true);
        p.lfo (2, "Sine", rnd (0.12f, 0.7f));
        p.mod ("LFO 3", "Filter 1 Cutoff", rnd (0.12f, 0.3f), true);
        p.mod ("LFO 3", "Osc B Position", rnd (-0.4f, 0.4f), true);
        if (motion > 0.6f) { p.lfo (3, "S&H", rnd (0.4f, 2.0f), 1.0f, false, "1/4", 0.35f); p.mod ("LFO 4", "Osc C Position", rnd (0.2f, 0.5f), true); }
        notes.add ("motion LFOs");
    }
    if (t[VIB] > 0.3f || isLead || isKeys)
    {
        p.lfo (4, "Sine", rnd (4.8f, 6.5f));
        const float vib = t[VIB] > 0.3f ? rnd (0.0025f, 0.005f) : rnd (0.0008f, 0.0018f);
        p.mod ("LFO 5", "Osc A Pitch", vib, true);
        p.mod ("LFO 5", "Osc B Pitch", vib, true);
    }
    if (wide > 0.4f)
    {
        p.lfo (5, "Triangle", rnd (0.15f, 0.5f));
        p.mod ("LFO 6", "Osc A Pan", rnd (0.3f, 0.6f), true);
        p.mod ("LFO 6", "Osc B Pan", -rnd (0.3f, 0.6f), true);
    }
    // macros: instant performance controls for the most important destinations
    p.mod ("Macro 1", "Filter 1 Cutoff", 0.6f, false);
    p.mod ("Macro 2", "Osc A Position", 0.5f, false);
    p.mod ("Macro 3", "Filter 1 Resonance", 0.4f, false);
    p.mod ("Macro 4", "Osc B Level", 0.5f, false);

    // ------------------------------------------------------------ 7) FX chain
    const float revMix = clamp01(0.08f + 0.5f * space);
    if (space > 0.2f || isPad || isFx || isPluck || isKeys)
        p.fx (0, revMix, { { "Size", 0.3f + 0.65f * space }, { "Damping", t[WARM] > 0.4f ? 0.65f : 0.4f }, { "Width", 1.0f }, { "Pre-Delay", rnd (5.0f, 40.0f) },
                           { "High Cut", t[WARM] > 0.4f ? 7000.0f : 13000.0f } });
    if (echo > 0.12f)
        p.fx (1, 0.14f + 0.36f * echo, { { "Time L", rng.nextBool() ? 375.0f : 250.0f }, { "Time R", rng.nextBool() ? 500.0f : 375.0f }, { "Feedback", 0.22f + 0.4f * echo },
                                         { "Tone", t[WARM] > 0.4f ? 4500.0f : 9000.0f }, { "Ping-Pong", (echo > 0.5f || wide > 0.5f) ? 1.0f : 0.0f } });
    if (wide > 0.25f || t[CHORUS] > 0.3f || isPad)
        p.fx (2, 0.25f + 0.35f * std::max (wide, t[CHORUS]), { { "Rate", rnd (0.3f, 0.9f) }, { "Depth", rnd (0.35f, 0.65f) }, { "Width", 1.0f } });
    if (t[FLANGER] > 0.3f) p.fx (3, 0.5f, { { "Rate", rnd (0.1f, 0.4f) }, { "Depth", 0.75f }, { "Feedback", 0.6f } });
    if (t[PHASER] > 0.3f)  p.fx (4, 0.5f, { { "Rate", rnd (0.15f, 0.6f) }, { "Depth", 0.8f }, { "Feedback", 0.5f } });
    if (aggr > 0.35f || t[GRIT] > 0.3f || t[LOFI] > 0.4f || t[FUZZ] > 0.3f || t[TUBE] > 0.3f || t[FOLD] > 0.3f)
    {
        const char* dt = t[FUZZ] > 0.3f ? "Fuzz" : (t[FOLD] > 0.3f ? "Fold" : (t[TUBE] > 0.3f ? "Tube" : (t[LOFI] > 0.4f ? (t[TAPE] > 0.3f ? "Tape" : (rng.nextBool() ? "Bit Crush" : "Downsample"))
                         : (aggr > 0.75f ? (rng.nextBool() ? "Hard Clip" : "Diode") : (rng.nextBool() ? "Overdrive" : "Soft Clip")))));
        p.fx (5, 1.0f, { { "Drive", 5.0f + 22.0f * std::max (aggr, t[GRIT]) }, { "Tone", t[WARM] > 0.4f ? 6000.0f : 13000.0f }, { "Bits", rnd (7.0f, 11.0f) }, { "Output", -5.0f } });
        p.fxChoice (5, "Type", dt);
        notes.add (juce::String ("distortion: ") + dt);
    }
    p.fx (6, 1.0f, { { "Low Cut", isBass ? 25.0f : rnd (30.0f, 55.0f) }, { "High Cut", (t[WARM] > 0.45f || t[LOFI] > 0.4f) ? rnd (7500.0f, 11000.0f) : 20000.0f },
                     { "High Gain", bright > 0.5f ? 2.5f : (t[WARM] > 0.4f ? -3.0f : 0.0f) }, { "Low Gain", t[SUB] > 0.5f ? 2.5f : 0.0f } });
    if (t[LOUD] > 0.4f || aggr > 0.6f || isBass) p.fx (7, clamp01 (0.2f + 0.35f * std::max (t[LOUD], aggr)), { { "Depth", 0.55f + 0.3f * t[LOUD] }, { "Time", 0.4f } });
    p.fx (8, 1.0f, { { "Threshold", -1.0f }, { "Release", 60.0f } });
    p.standardChain();

    // ------------------------------------------------------------ 8) play mode / arp
    if (isBass || isLead || t[MONO] > 0.4f || t[GLIDE] > 0.3f)
        p.play (t[GLIDE] > 0.5f && t[MONO] > 0.2f ? "Legato" : "Mono", t[GLIDE] > 0.3f ? rnd (70.0f, 220.0f) : rnd (15.0f, 45.0f));
    else p.play ("Poly", 0.0f);

    if (wantArp)
    {
        juce::String text = " " + prompt.toLowerCase() + " ";
        const char* pat = text.contains ("down") ? "Down" : (text.contains ("random") ? "Random" : (text.contains ("chord") ? "Chord" : (text.contains ("converge") ? "Converge" : (text.contains ("diverge") ? "Diverge"
                          : (text.contains ("zigzag") ? "Zigzag" : (text.contains ("cascade") ? "Cascade" : (text.contains ("octave") ? "Octave Jump" : (text.contains ("trance") ? "Up/Down" : nullptr))))))));
        static const char* pool[8] = { "Up", "Up/Down", "Pinky Up", "Thumb Up", "Zigzag", "Cascade", "Double Up", "Octave Jump" };
        if (pat == nullptr) pat = pool[rng.nextInt (8)];
        const char* rate = t[FAST] > 0.4f ? "1/32" : (t[SLOW] > 0.4f ? "1/8" : (text.contains ("triplet") ? "1/8T" : "1/16"));
        p.arp (true, pat, rate, (wide > 0.5f || isPad) ? 3 : 2, 0.45f + 0.2f * rng.nextFloat(), text.contains ("swing") ? 0.35f : 0.05f);
        if (sustain > 0.5f) p.env (0, 0.002f, 0.25f, 0.3f, 0.25f);
        notes.add (juce::String ("arp: ") + pat + " " + rate);
    }

    Design d;
    d.patch = std::move (p);
    d.summary = notes.joinIntoString (" | ");
    return d;
}
}
