#pragma once
#include "SoundDesigner.h"

namespace nusynth
{
/** Factory presets are written as prompts and composed by the sound designer with a fixed seed, so every preset uses the full synth:
    wavetables, warp, dual filters, 4 envelopes, 6 LFOs (incl. step shapes), 24-slot matrix, macros, effect modulation, FX types, arp. */
struct PresetDef { const char* cat; const char* name; const char* prompt; int seed; };

inline const PresetDef* presetDefs (int& count)
{
    static const PresetDef d[] = {
        // ------------------------------------------------------------------ ARP
        { "ARP", "Trance Arp",            "trance supersaw arp bright wide fast gated echo", 101 },
        { "ARP", "Neon Pluck Arp",        "plucky 80s synthwave arp chorus warm", 102 },
        { "ARP", "Dream Bell Arp",        "dreamy glass bell arp shimmer reverb slow", 103 },
        { "ARP", "Acid Random Arp",       "acid squelchy resonant arp random aggressive", 104 },
        { "ARP", "FM Cascade Arp",        "digital fm metallic arp cascade bright", 105 },
        { "ARP", "Dark Techno Arp",       "dark techno metallic arp rhythmic distorted", 106 },
        { "ARP", "Retro Chip Arp",        "chiptune 8bit arp fast bright lofi", 107 },
        { "ARP", "Cosmic Zigzag Arp",     "cosmic spaceship arp zigzag evolving wide echo", 108 },
        { "ARP", "Warm Analog Arp",       "warm analog arp tape echo soft", 109 },
        { "ARP", "Octave Jump Bass Arp",  "bass arp octave jump punchy", 110 },
        { "ARP", "Glass Harp Arp",        "glassy harp arp airy long reverb", 111 },
        { "ARP", "Future Bass Arp",       "future bass arp detuned bright wide sidechain", 112 },
        { "ARP", "Choir Arp",             "choir vocal arp ethereal slow", 113 },
        { "ARP", "Hypnotic Arp",          "hypnotic arp converge motion phaser", 114 },
        { "ARP", "Funky Pinky Arp",       "funky disco arp pinky up plucky chorus", 115 },
        // ------------------------------------------------------------------ BASS
        { "BASS", "Sub Weight 808",       "808 sub bass heavy deep clean long", 201 },
        { "BASS", "Reese Monster",        "reese bass dark detuned wide moving", 202 },
        { "BASS", "Wobble Dub",           "dubstep wobble bass aggressive growl", 203 },
        { "BASS", "FM Growl Bass",        "fm bass growl digital aggressive metallic", 204 },
        { "BASS", "Acid Squelch",         "acid bass squelchy resonant fast", 205 },
        { "BASS", "Rubber Funk Bass",     "funky rubbery bass punchy", 206 },
        { "BASS", "Neuro Machine",        "neuro dnb bass aggressive digital moving", 207 },
        { "BASS", "Warm Analog Bass",     "warm analog bass tube round", 208 },
        { "BASS", "Trap Slide 808",       "trap 808 bass glide long dark", 209 },
        { "BASS", "Hollow Square Bass",   "hollow bass tight square clean", 210 },
        { "BASS", "Distorted Saw Bass",   "distorted fuzz bass dirty loud", 211 },
        { "BASS", "Talking Bass",         "vocal formant bass growl moving", 212 },
        { "BASS", "Techno Rumble",        "techno rumble bass dark rhythmic", 213 },
        { "BASS", "Chip Bass",            "chiptune bass 8bit tight", 214 },
        { "BASS", "Laser Sub",            "sub bass glide clean sine laser", 215 },
        // ------------------------------------------------------------------ KEYS
        { "KEYS", "Glass Electric Piano", "rhodes electric piano warm chorus", 301 },
        { "KEYS", "Tine Keys",            "electric piano bell tine bright long", 302 },
        { "KEYS", "Warm Wurly",           "wurly warm tube vintage", 303 },
        { "KEYS", "Digital Grand",        "piano grand bright clean reverb", 304 },
        { "KEYS", "Drawbar Organ",        "organ hammond vibrato chorus", 305 },
        { "KEYS", "Church Organ",         "church organ cathedral huge", 306 },
        { "KEYS", "Clav Funk",            "clav funky plucky bright", 307 },
        { "KEYS", "Bell Keys",            "bell glockenspiel shimmer", 308 },
        { "KEYS", "Music Box",            "music box kalimba high bright", 309 },
        { "KEYS", "Celesta Dream",        "celesta dreamy shimmering", 310 },
        { "KEYS", "Vintage Strings Keys", "analog strings vintage chorus", 311 },
        { "KEYS", "Harpsichord",          "harpsichord bright plucky dry", 312 },
        { "KEYS", "Marimba Wood",         "marimba warm short", 313 },
        { "KEYS", "Vibraphone Lounge",    "vibraphone lounge vibrato warm", 314 },
        { "KEYS", "80s Stab Keys",        "80s keys synthwave chorus bright", 315 },
        // ------------------------------------------------------------------ PLUCK
        { "PLUCK", "Classic Pluck",       "classic pluck bright short echo", 401 },
        { "PLUCK", "Kalimba Drops",       "kalimba pluck high bright dry", 402 },
        { "PLUCK", "Harp Strum",          "harp pluck long shimmer", 403 },
        { "PLUCK", "Koto Pluck",          "koto pluck warm short", 404 },
        { "PLUCK", "Guitar Pluck",        "guitar pluck acoustic warm", 405 },
        { "PLUCK", "Banjo Bounce",        "banjo pluck bright quick", 406 },
        { "PLUCK", "Steel Drum Pluck",    "steel drum pluck metallic", 407 },
        { "PLUCK", "Bubble Pluck",        "water droplet bubble pluck resonant", 408 },
        { "PLUCK", "Glass Pluck",         "glass pluck bright airy short echo", 409 },
        { "PLUCK", "Dub Stab",            "dub chord stab warm echo detuned", 410 },
        { "PLUCK", "Rubber Pluck",        "rubbery pluck resonant punchy", 411 },
        { "PLUCK", "Pizzicato",           "pizzicato strings staccato", 412 },
        { "PLUCK", "Lute",                "lute pluck warm vintage", 413 },
        { "PLUCK", "Supersaw Stab",       "detuned supersaw pluck stab wide", 414 },
        { "PLUCK", "FM Bell Pluck",       "fm pluck digital bell short", 415 },
        // ------------------------------------------------------------------ PAD
        { "PAD", "Warm Analog Pad",       "warm analog pad slow lush chorus", 501 },
        { "PAD", "Ambient Wash",          "ambient pad wash slow evolving huge reverb", 502 },
        { "PAD", "Glass Pad",             "glass pad shimmer airy slow", 503 },
        { "PAD", "Choir Aah",             "choir pad angelic vocal slow", 504 },
        { "PAD", "String Ensemble",       "strings pad wide slow warm", 505 },
        { "PAD", "Dark Cinematic",        "dark cinematic pad evolving long deep", 506 },
        { "PAD", "Evolving Texture",      "evolving texture pad motion noisy", 507 },
        { "PAD", "Super Saw Pad",         "supersaw pad wide bright slow", 508 },
        { "PAD", "Underwater Drift",      "underwater pad dark moving warm", 509 },
        { "PAD", "Frozen Air",            "icy frozen pad bright airy shimmer", 510 },
        { "PAD", "Cathedral Drone",       "drone pad cathedral dark long", 511 },
        { "PAD", "Vaporwave Pad",         "vaporwave pad lofi chorus warm tape", 512 },
        { "PAD", "Whale Song",            "ghost voice pad eerie slow", 513 },
        { "PAD", "Space Nebula",          "cosmic pad spaceship evolving wide", 514 },
        { "PAD", "Warm Brass Pad",        "brass pad warm slow lush", 515 },
        // ------------------------------------------------------------------ LEAD
        { "LEAD", "Super Saw Lead",       "supersaw lead bright wide echo vibrato", 601 },
        { "LEAD", "Screaming Lead",       "screaming lead aggressive resonant", 602 },
        { "LEAD", "Square Mono Lead",     "square lead mono glide clean", 603 },
        { "LEAD", "Sync Lead",            "digital lead bright metallic", 604 },
        { "LEAD", "FM Lead",              "fm lead metallic bright vibrato", 605 },
        { "LEAD", "Vintage Mono Lead",    "analog moog lead mono glide warm", 606 },
        { "LEAD", "Trance Gate Lead",     "trance lead gated rhythmic wide", 607 },
        { "LEAD", "Whistle Lead",         "whistle lead soft vibrato pure", 608 },
        { "LEAD", "Brass Lead",           "brass lead warm vibrato", 609 },
        { "LEAD", "Hoover",               "hoover lead aggressive detuned", 610 },
        { "LEAD", "Pluck Lead",           "pluck lead bright short echo", 611 },
        { "LEAD", "Chip Lead",            "chiptune lead 8bit bright", 612 },
        { "LEAD", "Choir Lead",           "choir lead vocal soft", 613 },
        { "LEAD", "Flute Lead",           "flute lead breathy vibrato", 614 },
        { "LEAD", "Dirty Lead",           "dirty fuzz lead distorted", 615 },
        // ------------------------------------------------------------------ FX (incl. synthesised drum hits)
        { "FX", "Riser Sweep",            "riser sweep fx slow bright noisy", 701 },
        { "FX", "Laser Zap",              "laser zap fx fast digital", 702 },
        { "FX", "Impact Boom",            "impact fx boom deep long reverb", 703 },
        { "FX", "Alien Texture",          "alien fx metallic evolving noisy", 704 },
        { "FX", "Wind Storm",             "wind storm fx noisy slow", 705 },
        { "FX", "Glitch Hit",             "glitch fx digital stutter short", 706 },
        { "FX", "Metal Drone",            "metallic drone fx dark evolving", 707 },
        { "FX", "Underwater Rumble",      "underwater rumble fx dark", 708 },
        { "FX", "Sci-Fi Pulse",           "scifi fx rhythmic pulse phaser", 709 },
        { "FX", "Radio Static",           "radio static fx noisy lofi", 710 },
        { "FX", "Reverse Swell",          "reverse swell fx shimmer", 711 },
        { "FX", "Siren",                  "siren alarm fx fast", 712 },
        { "FX", "Kick Thump",             "kick drum punchy deep", 713 },
        { "FX", "Snare Crack",            "snare crack bright plate", 714 },
        { "FX", "Closed Hat",             "hihat closed bright", 715 },
    };
    count = (int) (sizeof (d) / sizeof (d[0]));
    return d;
}

inline Patch presetPatch (int index)
{
    int n = 0;
    const PresetDef* d = presetDefs (n);
    juce::Random rng (d[std::max (0, std::min (n - 1, index))].seed);
    return designSound (d[std::max (0, std::min (n - 1, index))].prompt, rng).patch;
}
}
