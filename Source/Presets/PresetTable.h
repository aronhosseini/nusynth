#pragma once
#include <algorithm>
#include "Patch.h"

namespace nusynth
{
struct PresetRow
{
    const char* cat; const char* name;
    const char* wtA; const char* wtB; const char* wtC;     // nullptr = oscillator off
    int uni; float det; float sub; float noise;
    const char* ft; float cut; float res;
    float a, d, s, r;                                      // amp envelope
    int lfo; float lfoRate; float lfoAmt;                  // 0 none 1 cutoff 2 wt-pos 3 vibrato 4 rhythmic 5 motion 6 s&h 7 pitch sweep
    int fxk; float envAmt;                                 // fx kind (see addFxKind), filter-envelope amount
    int arp;                                               // 0 off, else pattern index + 1
};

inline void addFxKind (Patch& p, int k)
{
    switch (k)
    {
        case 1:  p.fx (0, 0.30f, { { "Size", 0.75f }, { "Damping", 0.45f }, { "Width", 1.0f } }); break;
        case 2:  p.fx (1, 0.28f, { { "Time L", 375.0f }, { "Time R", 500.0f }, { "Feedback", 0.35f }, { "Tone", 6000.0f } });
                 p.fx (0, 0.22f, { { "Size", 0.6f } }); break;
        case 3:  p.fx (2, 0.50f, { { "Rate", 0.7f }, { "Depth", 0.5f }, { "Delay", 12.0f } });
                 p.fx (0, 0.28f, { { "Size", 0.7f } }); break;
        case 4:  p.fx (5, 1.0f, { { "Drive", 14.0f }, { "Output", -4.0f } });
                 p.fx (0, 0.15f, { { "Size", 0.4f } }); break;
        case 5:  p.fx (4, 0.50f, { { "Rate", 0.35f }, { "Depth", 0.8f }, { "Feedback", 0.5f } });
                 p.fx (0, 0.30f, { { "Size", 0.7f } }); break;
        case 6:  p.fx (3, 0.50f, { { "Rate", 0.25f }, { "Depth", 0.7f }, { "Feedback", 0.6f } });
                 p.fx (1, 0.25f, { { "Time L", 300.0f }, { "Time R", 450.0f }, { "Feedback", 0.35f } }); break;
        case 7:  p.fx (0, 0.50f, { { "Size", 0.95f }, { "Damping", 0.4f }, { "Pre-Delay", 30.0f } });
                 p.fx (1, 0.25f, { { "Time L", 500.0f }, { "Time R", 750.0f }, { "Feedback", 0.45f }, { "Ping-Pong", 1.0f } });
                 p.fx (2, 0.35f, { { "Rate", 0.4f }, { "Depth", 0.6f } }); break;
        case 8:  p.fx (5, 1.0f, { { "Drive", 8.0f }, { "Bits", 9.0f }, { "Output", -3.0f } }); p.fxChoice (5, "Type", "Bit Crush");
                 p.fx (6, 1.0f, { { "High Gain", -9.0f }, { "Low Gain", 3.0f } });
                 p.fx (0, 0.18f, { { "Size", 0.35f } }); break;
        case 9:  p.fx (7, 0.60f, { { "Depth", 0.8f }, { "Time", 0.5f } });
                 p.fx (0, 0.25f, { { "Size", 0.7f } }); break;
        case 10: p.fx (5, 1.0f, { { "Drive", 20.0f }, { "Output", -6.0f } }); p.fxChoice (5, "Type", "Hard Clip");
                 p.fx (7, 0.50f, { { "Depth", 0.7f } }); break;
        default: break;
    }
}

inline const PresetRow* presetRows (int& count)
{
    static const PresetRow rows[] = {
        // cat   name                  wtA                  wtB                  wtC             uni det   sub   noise filter          cut    res   a      d     s     r     lfo rate  amt  fx env arp
        // ------------------------------------------------------------------ BASS
        { "BASS", "Sub Pressure",       "Basic Sine",        nullptr,             nullptr,         1, 0.2f, 0.9f, 0.0f, "Low Pass 24",   400.f, 0.10f, 0.003f, 0.30f, 0.90f, 0.15f, 0, 0.0f, 0.0f, 0, 0.00f, 0 },
        { "BASS", "Reese Monster",      "Basic Saw",         "Basic Saw",         nullptr,         3, 0.45f,0.5f, 0.0f, "Ladder LP 24",  900.f, 0.25f, 0.005f, 0.40f, 0.80f, 0.20f, 1, 0.3f, 0.25f, 4, 0.20f, 0 },
        { "BASS", "Wobble Dub",         "Super Saw Stack",   nullptr,             nullptr,         1, 0.2f, 0.6f, 0.0f, "Low Pass 24",   700.f, 0.40f, 0.003f, 0.30f, 0.80f, 0.20f, 4, 2.0f, 0.55f, 4, 0.00f, 0 },
        { "BASS", "FM Growl",           "FM Growl",          "FM Bass",           nullptr,         1, 0.3f, 0.5f, 0.0f, "Low Pass 24",  1400.f, 0.20f, 0.003f, 0.25f, 0.70f, 0.12f, 2, 1.5f, 0.40f,10, 0.30f, 0 },
        { "BASS", "Acid Squelch",       "Basic Saw",         nullptr,             nullptr,         1, 0.0f, 0.3f, 0.0f, "Ladder LP 24",  600.f, 0.70f, 0.002f, 0.22f, 0.10f, 0.10f, 0, 0.0f, 0.0f, 4, 0.55f, 0 },
        { "BASS", "808 Boom",           "Basic Sine",        nullptr,             nullptr,         1, 0.0f, 1.0f, 0.0f, "Low Pass 12",   300.f, 0.00f, 0.001f, 0.90f, 0.00f, 0.40f, 0, 0.0f, 0.0f, 0, 0.00f, 0 },
        { "BASS", "Rubber Bass",        "Pulse 25",          "Vintage PWM",       nullptr,         2, 0.2f, 0.5f, 0.0f, "MG Low",        800.f, 0.30f, 0.004f, 0.30f, 0.60f, 0.15f, 2, 1.0f, 0.35f, 0, 0.25f, 0 },
        { "BASS", "Distorted Saw Bass", "Distorted Digital", "Basic Saw",         nullptr,         2, 0.3f, 0.4f, 0.05f,"Ladder LP 24", 1800.f, 0.20f, 0.003f, 0.30f, 0.80f, 0.20f, 0, 0.0f, 0.0f,10, 0.25f, 0 },
        { "BASS", "Hollow Square Bass", "Basic Square",      nullptr,             nullptr,         1, 0.1f, 0.6f, 0.0f, "Low Pass 12",   700.f, 0.20f, 0.005f, 0.30f, 0.70f, 0.20f, 0, 0.0f, 0.0f, 0, 0.20f, 0 },
        { "BASS", "Digital Edge Bass",  "Digital Edge",      "Aliased Edge",      nullptr,         1, 0.2f, 0.4f, 0.0f, "Ladder LP 24", 1200.f, 0.30f, 0.002f, 0.30f, 0.60f, 0.15f, 6, 4.0f, 0.30f, 9, 0.30f, 0 },
        { "BASS", "Sub Wobble",         "Basic Sine",        "Basic Triangle",    nullptr,         1, 0.0f, 0.8f, 0.0f, "Low Pass 24",   500.f, 0.30f, 0.003f, 0.30f, 0.90f, 0.20f, 1, 1.2f, 0.45f, 0, 0.00f, 0 },
        { "BASS", "Funky Pluck Bass",   "Pulse 12",          "Basic Saw",         nullptr,         1, 0.1f, 0.4f, 0.0f, "Ladder LP 24",  500.f, 0.40f, 0.001f, 0.20f, 0.20f, 0.10f, 0, 0.0f, 0.0f, 0, 0.45f, 0 },
        { "BASS", "Growl Stack",        "Harsh Fold",        "Super Saw Stack",   nullptr,         3, 0.35f,0.5f, 0.0f, "Low Pass 24",  1500.f, 0.30f, 0.003f, 0.35f, 0.70f, 0.20f, 2, 2.5f, 0.50f,10, 0.20f, 0 },
        { "BASS", "Warm Analog Bass",   "Analog Brass",      "Basic Saw",         nullptr,         2, 0.2f, 0.5f, 0.0f, "Ladder LP 12", 1000.f, 0.20f, 0.010f, 0.30f, 0.70f, 0.25f, 0, 0.0f, 0.0f, 3, 0.15f, 0 },
        { "BASS", "Dark Drone Bass",    "Pad Evolve",        "Basic Sine",        nullptr,         2, 0.3f, 0.7f, 0.05f,"Low Pass 24",   450.f, 0.30f, 0.200f, 0.60f, 0.90f, 0.80f, 1, 0.15f,0.30f, 1, 0.00f, 0 },
        // ------------------------------------------------------------------ KEYS
        { "KEYS", "Glass EP",           "FM EP",             "Basic Sine",        nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  5000.f, 0.10f, 0.002f, 1.20f, 0.40f, 0.60f, 0, 0.0f, 0.0f, 3, 0.10f, 0 },
        { "KEYS", "Tine Keys",          "FM EP",             "FM Bell",           nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  6000.f, 0.10f, 0.002f, 1.50f, 0.30f, 0.70f, 0, 0.0f, 0.0f, 3, 0.00f, 0 },
        { "KEYS", "Warm Rhodes",        "FM EP",             "Basic Triangle",    nullptr,         1, 0.05f,0.1f, 0.0f, "Low Pass 24",  3500.f, 0.10f, 0.002f, 1.00f, 0.50f, 0.50f, 0, 0.0f, 0.0f, 3, 0.10f, 0 },
        { "KEYS", "Digital Piano",      "Piano Partials",    "Basic Triangle",    nullptr,         1, 0.05f,0.0f, 0.0f, "Low Pass 12",  7000.f, 0.05f, 0.001f, 1.40f, 0.35f, 0.60f, 0, 0.0f, 0.0f, 1, 0.15f, 0 },
        { "KEYS", "Organ Drawbar",      "Additive Organ",    "Basic Sine",        nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  9000.f, 0.00f, 0.005f, 0.10f, 1.00f, 0.08f, 3, 6.0f, 0.20f, 3, 0.00f, 0 },
        { "KEYS", "Church Organ",       "Additive Organ",    "Odd Harmonics",     nullptr,         2, 0.1f, 0.0f, 0.0f, "Low Pass 12",  5000.f, 0.05f, 0.020f, 0.20f, 1.00f, 0.30f, 0, 0.0f, 0.0f, 7, 0.00f, 0 },
        { "KEYS", "Clav Funk",          "Pulse 12",          "Basic Saw",         nullptr,         1, 0.0f, 0.0f, 0.02f,"Band Pass 12", 1800.f, 0.20f, 0.001f, 0.20f, 0.00f, 0.10f, 0, 0.0f, 0.0f, 5, 0.40f, 0 },
        { "KEYS", "Bell Keys",          "FM Bell",           "Digital Bell",      nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  8000.f, 0.10f, 0.001f, 1.80f, 0.20f, 1.20f, 0, 0.0f, 0.0f, 2, 0.00f, 0 },
        { "KEYS", "Soft Pluck Keys",    "Basic Triangle",    "Soft Saw",          nullptr,         1, 0.1f, 0.1f, 0.0f, "Low Pass 24",  3000.f, 0.10f, 0.003f, 0.80f, 0.30f, 0.50f, 0, 0.0f, 0.0f, 1, 0.20f, 0 },
        { "KEYS", "Vintage Strings Keys","Analog Strings",   "Analog Pad",        nullptr,         2, 0.3f, 0.0f, 0.0f, "Low Pass 12",  3500.f, 0.10f, 0.200f, 0.50f, 0.80f, 0.60f, 1, 0.3f, 0.15f, 3, 0.00f, 0 },
        { "KEYS", "Wurly Bark",         "FM EP",             "Pulse 25",          nullptr,         1, 0.1f, 0.0f, 0.0f, "Ladder LP 12", 3000.f, 0.20f, 0.002f, 0.80f, 0.30f, 0.40f, 0, 0.0f, 0.0f, 4, 0.25f, 0 },
        { "KEYS", "Celesta",            "Glass Harmonic",    "FM Bell",           nullptr,         1, 0.05f,0.0f, 0.0f, "Low Pass 12",  9000.f, 0.00f, 0.001f, 1.20f, 0.00f, 0.90f, 0, 0.0f, 0.0f, 1, 0.00f, 0 },
        { "KEYS", "Harpsichord",        "Pulse 12",          "Odd Harmonics",     nullptr,         1, 0.1f, 0.0f, 0.02f,"Band Pass 12", 2500.f, 0.20f, 0.001f, 0.50f, 0.10f, 0.20f, 0, 0.0f, 0.0f, 1, 0.20f, 0 },
        { "KEYS", "Marimba Keys",       "Even Harmonics",    "Basic Sine",        nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  4000.f, 0.05f, 0.001f, 0.60f, 0.00f, 0.35f, 0, 0.0f, 0.0f, 1, 0.00f, 0 },
        // ------------------------------------------------------------------ PLUCK
        { "PLUCK","Classic Pluck",      "Basic Saw",         "Basic Square",      nullptr,         1, 0.15f,0.1f, 0.0f, "Ladder LP 24",  900.f, 0.30f, 0.001f, 0.30f, 0.00f, 0.20f, 0, 0.0f, 0.0f, 2, 0.50f, 0 },
        { "PLUCK","Analog Pluck",       "Basic Saw",         nullptr,             nullptr,         2, 0.2f, 0.1f, 0.0f, "Low Pass 24",   800.f, 0.20f, 0.001f, 0.25f, 0.00f, 0.20f, 0, 0.0f, 0.0f, 3, 0.50f, 0 },
        { "PLUCK","Kalimba",            "Glass Harmonic",    "FM Bell",           nullptr,         1, 0.05f,0.0f, 0.0f, "Low Pass 12",  7000.f, 0.00f, 0.001f, 0.70f, 0.00f, 0.50f, 0, 0.0f, 0.0f, 1, 0.00f, 0 },
        { "PLUCK","Harp Strum",         "FM Pluck",          "Basic Triangle",    nullptr,         1, 0.05f,0.0f, 0.0f, "Low Pass 12",  6000.f, 0.10f, 0.001f, 1.00f, 0.00f, 0.60f, 0, 0.0f, 0.0f, 7, 0.00f, 0 },
        { "PLUCK","Pizzicato",          "Analog Strings",    nullptr,             nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  2500.f, 0.20f, 0.001f, 0.18f, 0.00f, 0.12f, 0, 0.0f, 0.0f, 1, 0.30f, 0 },
        { "PLUCK","Koto Pluck",         "FM Pluck",          "Pulse 25",          nullptr,         1, 0.05f,0.0f, 0.0f, "Band Pass 12", 2200.f, 0.30f, 0.001f, 0.40f, 0.00f, 0.30f, 0, 0.0f, 0.0f, 1, 0.00f, 0 },
        { "PLUCK","Pluck Pad Stab",     "Super Saw Stack",   "Basic Saw",         nullptr,         5, 0.35f,0.0f, 0.0f, "Ladder LP 24", 1500.f, 0.20f, 0.001f, 0.35f, 0.00f, 0.30f, 0, 0.0f, 0.0f, 2, 0.50f, 0 },
        { "PLUCK","Staccato Square",    "Basic Square",      nullptr,             nullptr,         1, 0.0f, 0.1f, 0.0f, "Low Pass 24",  1200.f, 0.20f, 0.001f, 0.15f, 0.00f, 0.10f, 0, 0.0f, 0.0f, 2, 0.40f, 0 },
        { "PLUCK","Bubble Pluck",       "Sine to Saw",       "Basic Sine",        nullptr,         1, 0.0f, 0.0f, 0.0f, "Ladder LP 24",  600.f, 0.50f, 0.001f, 0.20f, 0.00f, 0.15f, 0, 0.0f, 0.0f, 2, 0.60f, 0 },
        { "PLUCK","Steel Drum",         "FM Metallic",       "FM Clang",          nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  5000.f, 0.10f, 0.001f, 0.60f, 0.00f, 0.40f, 0, 0.0f, 0.0f, 1, 0.00f, 0 },
        { "PLUCK","Lute Pluck",         "Piano Partials",    "Basic Triangle",    nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  3500.f, 0.10f, 0.001f, 0.50f, 0.00f, 0.30f, 0, 0.0f, 0.0f, 1, 0.00f, 0 },
        { "PLUCK","Glass Pluck",        "Glass Harmonic",    "Digital Bell",      nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  9000.f, 0.00f, 0.001f, 0.50f, 0.00f, 0.50f, 0, 0.0f, 0.0f, 2, 0.00f, 0 },
        { "PLUCK","Dub Chord Stab",     "Analog Brass",      "Basic Saw",         nullptr,         3, 0.2f, 0.2f, 0.0f, "Ladder LP 24", 1000.f, 0.30f, 0.001f, 0.40f, 0.00f, 0.30f, 0, 0.0f, 0.0f, 2, 0.40f, 0 },
        { "PLUCK","Rubber Pluck",       "Pulse 25",          "Vintage PWM",       nullptr,         2, 0.15f,0.1f, 0.0f, "MG Low",        800.f, 0.40f, 0.001f, 0.25f, 0.00f, 0.20f, 0, 0.0f, 0.0f, 2, 0.50f, 0 },
        // ------------------------------------------------------------------ PAD
        { "PAD",  "Warm Analog Pad",    "Analog Pad",        "Analog Strings",    nullptr,         5, 0.3f, 0.1f, 0.0f, "Low Pass 24",  1800.f, 0.10f, 0.800f, 1.00f, 0.90f, 1.60f, 1, 0.25f,0.20f, 3, 0.00f, 0 },
        { "PAD",  "Ambient Wash",       "Spectral Pad",      "Pad Evolve",        nullptr,         4, 0.35f,0.05f,0.05f,"Low Pass 12",  3000.f, 0.00f, 1.500f, 1.00f, 0.90f, 3.00f, 2, 0.1f, 0.50f, 7, 0.00f, 0 },
        { "PAD",  "Glass Pad",          "Glass Harmonic",    "Additive Choir",    nullptr,         4, 0.3f, 0.0f, 0.0f, "Low Pass 12",  6000.f, 0.00f, 1.000f, 1.00f, 0.90f, 2.50f, 2, 0.2f, 0.40f, 7, 0.00f, 0 },
        { "PAD",  "Choir Aah",          "Additive Choir",    "Vocal Formant A",   nullptr,         3, 0.2f, 0.0f, 0.03f,"Low Pass 12",  4500.f, 0.10f, 0.600f, 0.80f, 0.90f, 1.50f, 3, 5.0f, 0.10f, 1, 0.00f, 0 },
        { "PAD",  "String Ensemble",    "Analog Strings",    "Super Saw Stack",   nullptr,         6, 0.3f, 0.0f, 0.0f, "Low Pass 12",  3500.f, 0.10f, 0.500f, 0.60f, 0.90f, 1.00f, 1, 0.3f, 0.15f, 3, 0.00f, 0 },
        { "PAD",  "Dark Pad",           "Soft Saw",          "Analog Pad",        nullptr,         4, 0.25f,0.3f, 0.0f, "Low Pass 24",   900.f, 0.20f, 1.200f, 1.00f, 0.80f, 2.00f, 1, 0.15f,0.30f, 1, 0.00f, 0 },
        { "PAD",  "Evolving Texture",   "Pad Evolve",        "Spectral Freeze",   nullptr,         4, 0.3f, 0.0f, 0.05f,"Band Pass 12", 2000.f, 0.20f, 2.000f, 1.00f, 0.90f, 3.00f, 5, 0.08f,0.70f, 7, 0.00f, 0 },
        { "PAD",  "Super Saw Pad",      "Super Saw Stack",   "Basic Saw",         nullptr,         7, 0.4f, 0.1f, 0.0f, "Ladder LP 12", 3500.f, 0.10f, 0.700f, 1.00f, 0.85f, 1.20f, 1, 0.4f, 0.20f, 3, 0.00f, 0 },
        { "PAD",  "Vocal Pad",          "Vocal Formant B",   "Additive Choir",    nullptr,         3, 0.2f, 0.0f, 0.03f,"Formant I",    1200.f, 0.20f, 0.800f, 1.00f, 0.90f, 2.00f, 2, 0.2f, 0.50f, 7, 0.00f, 0 },
        { "PAD",  "Sweeping Pad",       "Analog Pad",        "Basic Saw",         nullptr,         5, 0.3f, 0.1f, 0.0f, "Low Pass 24",  1000.f, 0.40f, 1.200f, 1.00f, 0.80f, 2.00f, 1, 0.12f,0.60f, 1, 0.00f, 0 },
        { "PAD",  "Pulse Pad",          "Vintage PWM",       "Pulse 25",          nullptr,         3, 0.25f,0.1f, 0.0f, "Low Pass 12",  3000.f, 0.10f, 0.500f, 0.80f, 0.80f, 1.00f, 2, 0.5f, 0.40f, 3, 0.00f, 0 },
        { "PAD",  "Cinematic Drone",    "Spectral Pad",      "Basic Sine",        "Analog Pad",    4, 0.4f, 0.4f, 0.05f,"Low Pass 24",  1200.f, 0.20f, 3.000f, 1.00f, 1.00f, 4.00f, 1, 0.05f,0.40f, 7, 0.00f, 0 },
        { "PAD",  "Frozen Air",         "Spectral Freeze",   "Glass Harmonic",    nullptr,         3, 0.3f, 0.0f, 0.10f,"Low Pass 12",  7000.f, 0.10f, 1.500f, 1.00f, 0.90f, 3.00f, 5, 0.1f, 0.50f, 7, 0.00f, 0 },
        { "PAD",  "Warm Brass Pad",     "Analog Brass",      "Soft Saw",          nullptr,         4, 0.25f,0.1f, 0.0f, "Low Pass 24",  1500.f, 0.10f, 0.400f, 0.80f, 0.90f, 1.00f, 1, 0.2f, 0.15f, 3, 0.00f, 0 },
        { "PAD",  "Lush Chords",        "Super Saw Stack",   "Analog Pad",        "Analog Strings",6, 0.35f,0.1f, 0.0f, "Low Pass 12",  3000.f, 0.10f, 0.600f, 0.90f, 0.90f, 1.50f, 1, 0.3f, 0.20f, 7, 0.00f, 0 },
        // ------------------------------------------------------------------ LEAD
        { "LEAD", "Super Saw Lead",     "Super Saw Stack",   "Basic Saw",         nullptr,         7, 0.4f, 0.1f, 0.0f, "Ladder LP 12", 5000.f, 0.20f, 0.005f, 0.30f, 0.80f, 0.20f, 3, 5.5f, 0.15f, 2, 0.00f, 0 },
        { "LEAD", "Screaming Lead",     "Basic Saw",         "Harsh Fold",        nullptr,         3, 0.3f, 0.1f, 0.02f,"Ladder LP 24", 4500.f, 0.45f, 0.003f, 0.30f, 0.80f, 0.20f, 3, 5.5f, 0.15f, 4, 0.20f, 0 },
        { "LEAD", "Square Mono Lead",   "Basic Square",      "Pulse 25",          nullptr,         2, 0.15f,0.2f, 0.0f, "Low Pass 24",  3500.f, 0.20f, 0.005f, 0.20f, 0.80f, 0.15f, 3, 5.0f, 0.15f, 2, 0.00f, 0 },
        { "LEAD", "Digital Sync Lead",  "Digital Edge",      "Basic Saw",         nullptr,         2, 0.2f, 0.1f, 0.0f, "Low Pass 12",  6000.f, 0.10f, 0.003f, 0.30f, 0.80f, 0.20f, 7, 0.4f, 0.03f, 2, 0.00f, 0 },
        { "LEAD", "FM Lead",            "FM Stack",          "FM Sweep",          nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  7000.f, 0.10f, 0.004f, 0.30f, 0.80f, 0.20f, 2, 2.0f, 0.30f, 2, 0.00f, 0 },
        { "LEAD", "Vintage Mono Lead",  "Vintage PWM",       "Basic Saw",         nullptr,         2, 0.12f,0.3f, 0.0f, "MG Low",       3500.f, 0.30f, 0.005f, 0.30f, 0.70f, 0.15f, 3, 5.0f, 0.15f, 2, 0.20f, 0 },
        { "LEAD", "Trance Gate Lead",   "Super Saw Stack",   "Basic Saw",         nullptr,         7, 0.45f,0.0f, 0.0f, "Low Pass 24",  4500.f, 0.20f, 0.003f, 0.30f, 0.80f, 0.20f, 4, 8.0f, 0.60f, 2, 0.00f, 0 },
        { "LEAD", "Soft Whistle Lead",  "Basic Sine",        "Basic Triangle",    nullptr,         1, 0.0f, 0.0f, 0.0f, "Low Pass 12",  9000.f, 0.00f, 0.040f, 0.20f, 0.90f, 0.20f, 3, 5.5f, 0.25f, 1, 0.00f, 0 },
        { "LEAD", "Brass Lead",         "Analog Brass",      "Basic Saw",         nullptr,         2, 0.15f,0.1f, 0.0f, "Ladder LP 12", 3000.f, 0.20f, 0.030f, 0.20f, 0.80f, 0.20f, 3, 5.0f, 0.15f, 1, 0.35f, 0 },
        { "LEAD", "Hoover",             "Rasp",              "Super Saw Stack",   nullptr,         5, 0.5f, 0.2f, 0.03f,"Low Pass 12",  4500.f, 0.20f, 0.010f, 0.40f, 0.80f, 0.30f, 3, 5.0f, 0.15f, 4, 0.00f, 0 },
        { "LEAD", "Pluck Lead",         "Pulse 12",          "Basic Saw",         nullptr,         2, 0.2f, 0.0f, 0.0f, "Ladder LP 24", 3500.f, 0.20f, 0.001f, 0.40f, 0.20f, 0.20f, 0, 0.0f, 0.0f, 2, 0.40f, 0 },
        { "LEAD", "Metal Lead",         "FM Metallic",       "Digital Edge",      nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  6500.f, 0.20f, 0.003f, 0.30f, 0.80f, 0.20f, 3, 5.5f, 0.15f, 4, 0.00f, 0 },
        { "LEAD", "Choir Lead",         "Additive Choir",    "Vocal Formant A",   nullptr,         2, 0.1f, 0.0f, 0.0f, "Low Pass 12",  5000.f, 0.10f, 0.050f, 0.30f, 0.90f, 0.30f, 3, 5.0f, 0.20f, 1, 0.00f, 0 },
        { "LEAD", "Glass Lead",         "Glass Harmonic",    "Basic Triangle",    nullptr,         2, 0.1f, 0.0f, 0.0f, "Low Pass 12",  9000.f, 0.00f, 0.010f, 0.30f, 0.85f, 0.30f, 3, 5.5f, 0.15f, 2, 0.00f, 0 },
        { "LEAD", "Dirty Lead",         "Distorted Digital", "Basic Saw",         nullptr,         3, 0.25f,0.1f, 0.03f,"Ladder LP 24", 4000.f, 0.30f, 0.003f, 0.30f, 0.80f, 0.20f, 3, 5.5f, 0.15f,10, 0.00f, 0 },
        // ------------------------------------------------------------------ FX
        { "FX",   "Riser Sweep",        "Harmonic Sweep",    "Spectral Shred",    nullptr,         3, 0.3f, 0.0f, 0.50f,"High Pass 12", 3000.f, 0.30f, 3.500f, 1.00f, 1.00f, 1.50f, 1, 0.1f, 0.40f, 7, 0.40f, 0 },
        { "FX",   "Laser Zap",          "FM Sweep",          "Basic Saw",         nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  8000.f, 0.20f, 0.001f, 0.40f, 0.00f, 0.30f, 7, 4.0f, 0.60f, 2, 0.30f, 0 },
        { "FX",   "Impact Boom",        "Basic Sine",        nullptr,             nullptr,         1, 0.0f, 1.0f, 0.40f,"Low Pass 24",   300.f, 0.20f, 0.001f, 1.50f, 0.00f, 1.00f, 0, 0.0f, 0.0f, 7, 0.00f, 0 },
        { "FX",   "Alien Texture",      "Spectral Shred",    "Metal Scrape",      nullptr,         1, 0.3f, 0.0f, 0.20f,"Comb +",        900.f, 0.50f, 0.500f, 1.00f, 0.90f, 2.00f, 5, 0.3f, 0.60f, 7, 0.00f, 0 },
        { "FX",   "Wind Noise",         "Noise Burst",       nullptr,             nullptr,         1, 0.0f, 0.0f, 0.80f,"Band Pass 12",  900.f, 0.30f, 1.500f, 1.00f, 0.90f, 2.00f, 6, 0.4f, 0.40f, 7, 0.00f, 0 },
        { "FX",   "Glitch Hit",         "Bit Crush",         "Harsh Fold",        nullptr,         1, 0.2f, 0.0f, 0.10f,"High Pass 12", 1500.f, 0.20f, 0.001f, 0.12f, 0.00f, 0.10f, 6, 6.0f, 0.50f, 8, 0.00f, 0 },
        { "FX",   "Metallic Drone",     "Metal Scrape",      "FM Clang",          nullptr,         2, 0.4f, 0.2f, 0.05f,"Comb -",        800.f, 0.60f, 2.000f, 1.00f, 1.00f, 3.00f, 5, 0.2f, 0.50f, 7, 0.00f, 0 },
        { "FX",   "Underwater",         "Pad Evolve",        "Basic Sine",        nullptr,         2, 0.3f, 0.3f, 0.10f,"Low Pass 24",   500.f, 0.30f, 1.000f, 1.00f, 0.90f, 2.00f, 1, 0.3f, 0.50f, 7, 0.00f, 0 },
        { "FX",   "Sci-Fi Pulse",       "Pulse 12",          "FM Chaos",          nullptr,         1, 0.2f, 0.0f, 0.00f,"Phaser 8",     1500.f, 0.50f, 0.010f, 0.30f, 0.70f, 0.50f, 4, 6.0f, 0.50f, 5, 0.00f, 0 },
        { "FX",   "Radio Static",       "Rasp",              "Noise Burst",       nullptr,         1, 0.0f, 0.0f, 0.90f,"Band Pass 12", 3000.f, 0.50f, 0.010f, 0.50f, 0.80f, 0.40f, 6, 8.0f, 0.40f, 8, 0.00f, 0 },
        { "FX",   "Ghost Voices",       "Vocal Formant B",   "Spectral Freeze",   nullptr,         3, 0.4f, 0.0f, 0.10f,"Formant II",   1500.f, 0.30f, 1.500f, 1.00f, 0.90f, 3.00f, 2, 0.2f, 0.50f, 7, 0.00f, 0 },
        { "FX",   "Reverse Swell",      "Spectral Pad",      "Super Saw Stack",   nullptr,         3, 0.3f, 0.0f, 0.10f,"Low Pass 24",  2500.f, 0.20f, 3.000f, 1.00f, 0.00f, 0.50f, 0, 0.0f, 0.0f, 7, 0.30f, 0 },
        { "FX",   "Dark Rumble",        "Basic Sine",        nullptr,             nullptr,         1, 0.0f, 1.0f, 0.50f,"Low Pass 24",   200.f, 0.30f, 0.500f, 1.00f, 1.00f, 2.00f, 1, 0.1f, 0.30f, 1, 0.00f, 0 },
        { "FX",   "Siren",              "Basic Sine",        "Basic Triangle",    nullptr,         1, 0.0f, 0.0f, 0.00f,"Low Pass 12",  9000.f, 0.00f, 0.010f, 0.30f, 1.00f, 0.30f, 7, 0.6f, 0.08f, 2, 0.00f, 0 },
        // ------------------------------------------------------------------ ARP
        { "ARP",  "Arp Classic",        "Basic Saw",         "Basic Square",      nullptr,         2, 0.2f, 0.0f, 0.0f, "Ladder LP 24", 2500.f, 0.30f, 0.002f, 0.25f, 0.40f, 0.20f, 0, 0.0f, 0.0f, 2, 0.40f, 1 },
        { "ARP",  "Trance Arp",         "Super Saw Stack",   "Basic Saw",         nullptr,         5, 0.35f,0.0f, 0.0f, "Low Pass 24",  3500.f, 0.20f, 0.002f, 0.20f, 0.30f, 0.20f, 4, 8.0f, 0.40f, 2, 0.30f, 3 },
        { "ARP",  "Pluck Arp",          "Pulse 12",          "Basic Triangle",    nullptr,         1, 0.1f, 0.0f, 0.0f, "Ladder LP 24", 1800.f, 0.30f, 0.001f, 0.20f, 0.00f, 0.20f, 0, 0.0f, 0.0f, 2, 0.50f, 1 },
        { "ARP",  "Bell Arp",           "FM Bell",           "Glass Harmonic",    nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  8000.f, 0.10f, 0.001f, 0.80f, 0.00f, 0.60f, 0, 0.0f, 0.0f, 7, 0.00f, 1 },
        { "ARP",  "Acid Arp",           "Basic Saw",         nullptr,             nullptr,         1, 0.0f, 0.2f, 0.0f, "Ladder LP 24",  700.f, 0.65f, 0.001f, 0.20f, 0.00f, 0.15f, 0, 0.0f, 0.0f, 4, 0.60f, 5 },
        { "ARP",  "Dream Arp",          "Analog Pad",        "Glass Harmonic",    nullptr,         3, 0.3f, 0.1f, 0.0f, "Low Pass 12",  4000.f, 0.10f, 0.010f, 0.40f, 0.50f, 0.80f, 2, 0.2f, 0.40f, 7, 0.20f, 2 },
        { "ARP",  "Chord Pulse",        "Super Saw Stack",   "Analog Brass",      nullptr,         5, 0.3f, 0.0f, 0.0f, "Low Pass 24",  3000.f, 0.20f, 0.002f, 0.20f, 0.50f, 0.20f, 0, 0.0f, 0.0f, 2, 0.30f, 7 },
        { "ARP",  "FM Arp",             "FM Stack",          "FM EP",             nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 12",  6000.f, 0.10f, 0.001f, 0.30f, 0.10f, 0.30f, 2, 0.5f, 0.30f, 2, 0.20f, 4 },
        { "ARP",  "Retro Arp",          "Pulse 25",          "Vintage PWM",       nullptr,         2, 0.15f,0.1f, 0.0f, "MG Low",       2500.f, 0.30f, 0.001f, 0.20f, 0.30f, 0.15f, 0, 0.0f, 0.0f, 2, 0.40f, 1 },
        { "ARP",  "Bass Arp",           "Basic Saw",         "Basic Square",      nullptr,         1, 0.1f, 0.6f, 0.0f, "Ladder LP 24",  900.f, 0.40f, 0.001f, 0.20f, 0.30f, 0.15f, 0, 0.0f, 0.0f, 0, 0.40f, 1 },
        { "ARP",  "Cosmic Arp",         "Pad Evolve",        "FM Sweep",          nullptr,         2, 0.3f, 0.0f, 0.05f,"Low Pass 12",  5000.f, 0.20f, 0.005f, 0.40f, 0.40f, 0.80f, 5, 0.3f, 0.40f, 7, 0.20f, 3 },
        { "ARP",  "Digital Arp",        "Digital Edge",      "Basic Square",      nullptr,         1, 0.1f, 0.0f, 0.0f, "Low Pass 24",  4000.f, 0.30f, 0.001f, 0.20f, 0.20f, 0.20f, 6, 6.0f, 0.30f, 2, 0.40f, 6 },
        { "ARP",  "Dark Arp",           "Basic Saw",         "Soft Saw",          nullptr,         2, 0.2f, 0.4f, 0.0f, "Low Pass 24",  1200.f, 0.30f, 0.002f, 0.30f, 0.30f, 0.30f, 1, 0.3f, 0.30f, 1, 0.30f, 1 },
    };
    count = (int) (sizeof (rows) / sizeof (rows[0]));
    return rows;
}

inline Patch buildRow (const PresetRow& r)
{
    Patch p;
    const juce::String cat (r.cat);
    const bool bass = cat == "BASS", lead = cat == "LEAD", pad = cat == "PAD", fxCat = cat == "FX";

    p.osc (0, r.wtA, 0.8f, 0, 0.0f, r.uni, r.det, 0.55f, 0.7f);
    if (r.wtB != nullptr) p.osc (1, r.wtB, 0.55f, 0, r.det > 0.05f ? 7.0f : 0.0f, std::max (1, r.uni / 2), r.det, 0.5f, 0.6f);
    if (r.wtC != nullptr) p.osc (2, r.wtC, 0.40f, (pad || lead) ? 12 : (bass ? -12 : 0), -5.0f, std::max (1, r.uni / 2), r.det, 0.5f, 0.5f);
    if (r.sub > 0.0f)   p.sub ("SIN", r.sub, -1);
    if (r.noise > 0.0f) p.noise (fxCat ? "Pink" : "White", r.noise, fxCat ? 3000.0f : 6000.0f, true, false);

    p.filter (0, r.ft, r.cut, r.res, bass ? 0.15f : 0.0f);
    p.routeAll ("Filter 1");
    if (bass && r.sub > 0.0f) p.route ("sub", "Bypass");

    p.env (0, r.a, r.d, r.s, r.r);
    if (r.envAmt > 0.0f)
    {
        p.env (1, 0.002f, std::max (0.15f, r.d), 0.1f, r.r);
        p.mod ("Env 2", "Filter 1 Cutoff", r.envAmt, false);
    }
    p.mod ("Velocity", "Filter 1 Cutoff", 0.12f, false);
    p.mod ("Key", "Filter 1 Cutoff", 0.10f, false);

    switch (r.lfo)
    {
        case 1:  p.lfo (0, "Sine", r.lfoRate, 1.0f);
                 p.mod ("LFO 1", "Filter 1 Cutoff", r.lfoAmt, true); break;
        case 2:  p.lfo (0, "Sine", r.lfoRate, 1.0f);
                 p.mod ("LFO 1", "Osc A Position", r.lfoAmt, true);
                 if (r.wtB != nullptr) p.mod ("LFO 1", "Osc B Position", r.lfoAmt * 0.7f, true); break;
        case 3:  p.lfo (0, "Sine", r.lfoRate, 1.0f);
                 p.mod ("LFO 1", "Osc A Pitch", r.lfoAmt * 0.0175f, true);
                 if (r.wtB != nullptr) p.mod ("LFO 1", "Osc B Pitch", r.lfoAmt * 0.0175f, true); break;
        case 4:  p.lfo (0, "Square", r.lfoRate, 1.0f, true, "1/8", 0.1f);
                 p.mod ("LFO 1", "Filter 1 Cutoff", r.lfoAmt, true); break;
        case 5:  p.lfo (0, "Triangle", r.lfoRate, 1.0f);
                 p.lfo (1, "Sine", r.lfoRate * 0.63f, 1.0f);
                 p.mod ("LFO 1", "Osc A Position", r.lfoAmt, true);
                 p.mod ("LFO 2", "Osc A Pan", 0.5f, true);
                 if (r.wtB != nullptr) p.mod ("LFO 1", "Osc B Position", r.lfoAmt * 0.6f, true); break;
        case 6:  p.lfo (0, "S&H", r.lfoRate, 1.0f, false, "1/4", 0.15f);
                 p.mod ("LFO 1", "Filter 1 Cutoff", r.lfoAmt, true); break;
        case 7:  p.lfo (0, "Triangle", r.lfoRate, 1.0f);
                 p.mod ("LFO 1", "Osc A Pitch", r.lfoAmt, true); break;
        default: break;
    }

    addFxKind (p, r.fxk);
    p.standardChain();

    if (bass)      p.play ("Mono", 25.0f);
    else if (lead) p.play ("Mono", 30.0f);
    else           p.play ("Poly", 0.0f);

    if (r.arp > 0)
    {
        static const char* pats[7] = { "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord" };
        p.arp (true, pats[std::min (6, r.arp - 1)], "1/16", 2, 0.6f, 0.1f);
    }
    return p;
}
}
