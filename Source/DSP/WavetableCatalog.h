#pragma once
// Names and menu groups of the extra "Serum-style" wavetables (appended after the 8 user slots, so old presets keep their indices)
// and of the extra noise types (appended after "Sample").  The names follow the Serum 2 menus; the waves themselves are generated procedurally
// by WavetableExtras.h / NoiseExtras.h and are NOT copies of Serum's data.
#include <cstddef>

namespace nusynth
{
namespace catalog
{
enum WaveGroup { GroupAnalog = 0, GroupDigital, GroupSpectral, GroupVowel, kNumWaveGroups };
inline const char* const waveGroupNames[kNumWaveGroups] = { "ANALOG", "DIGITAL", "SPECTRAL", "VOWEL" };

inline constexpr int kAnalog = 42, kDigital = 56, kSpectral = 35, kVowel = 11, kExtraWaves = kAnalog + kDigital + kSpectral + kVowel;   // 144

inline const char* const extraWaveNames[kExtraWaves] = {
    // ---- Analog (42)
    "4088", "Acid", "Analog_BD_Sin", "Basic Mg", "Basic Mini", "Basic Shapes", "Basic_Cjw", "Basic_McB", "Basic_Mdc", "Basic_Wrd",
    "BS2 - Acid", "BS2 - Filthy", "BS2 - Subby Saw", "BSOD_Square", "DS Saw and Tri", "Jno", "MATRIXY C64", "MB Saw", "MB Triangle (Metal)",
    "Mellow But Instable", "MiniBass", "MsAw", "MsPw", "PWM C64", "PWM DS", "PWM Genji", "PWM Inception", "PWM Juno", "PWM LPF",
    "PWM Maths 2", "PWM Maths", "PWM MedicineHat", "PWM MG", "PWM Mini", "PWM Reso", "PWM Restricted", "PWM Trench", "PWM_Mdc",
    "SawRounded", "SawRoundedToSquare", "SQR_SAW_Wrd", "Sweep20",
    // ---- Digital (56)
    "Anti-Stasis", "bipole_harmonic", "BottleBlow", "CrushWub", "Debussy", "DirtySaw", "Dist 8bit Fwap", "Dist Bass Dropper", "Dist C2", "Dist d00t",
    "Dist Fwapper SQ", "Dist Sub Dskperc", "Dist WaTech", "Dull_toy", "Ethos", "Evol Longreece", "Evol Sweep", "Evolution", "FFT_Add_2nds",
    "FFT_Add_3rds", "FFT_Add_4ths", "FFT_Animal 1", "FFT_SQUEAL", "FMFM", "FM_Freak", "FM_Splat", "Gritty", "GS-xello",
    "Harmonic Morph", "Harmonic Subtle", "HarmonicSeries", "Hoo", "Hypa", "ICanHasKick", "Kream", "Mario", "Misfits", "PerfectSQSteps", "Prime",
    "PWM Electro", "PWM Weird", "Razor", "Reese", "Ring", "Scream", "Shrats", "Sludgecrank", "SSQ", "SubBass_1", "Tigra", "VintageSynth", "Voxu",
    "Waveterminal", "Weow", "Wraith", "Yoy64",
    // ---- Spectral (35)
    "AlienSpectral [SN]", "Bowed Metal [ML]", "Creeper [SN]", "Electric Guitar [SL]", "Gremlin [SN]", "Grimace [SN]", "Light Metal 1 [SL]",
    "Monster 1 [SL]", "Monster 2 [SL]", "Monster 3 [SL]", "Monster 4 [SL]", "Monster 5 [SL]", "Monster 6 [SL]", "Monster 7 [SL]", "Monster 8 [SL]",
    "Monster 9 [SL]", "Oddpass [SN]", "Phase Werb [SL]", "Reesey Mess 1 [SL]", "Reesey Mess 2 [SL]", "Reesey Mess 3 [SL]", "Reesey Mess 4 [SL]",
    "Reesey Mess 5 [SL]", "Scout [SN]", "Solid Phase 1 [SL]", "Solid Phase 2 [SL]", "Squelchy 1 [SL]", "Squelchy 2 [SL]", "Squelchy 3 [SL]",
    "Squelchy FM 1 [SL]", "Squelchy FM 2 [SL]", "Thin Thick [SL]", "Trilobyte 1 [SL]", "Trilobyte 2 [SL]", "Trilobyte 3 [SL]",
    // ---- Vowel (11)
    "Allophones", "DudaChoir", "E_Yeah", "Fred_Likes_Bass", "ML Addifunk", "Native Curse", "OOH_YAH_OO", "Retro-Speaking", "SAM_SERUM", "SAM_Speaks", "WahWah"
};

inline constexpr int waveGroupStart[kNumWaveGroups + 1] = { 0, kAnalog, kAnalog + kDigital, kAnalog + kDigital + kSpectral, kExtraWaves };

inline int waveGroupOf (int extraIdx)
{
    for (int g = 0; g < kNumWaveGroups; ++g) if (extraIdx < waveGroupStart[g + 1]) return g;
    return kNumWaveGroups - 1;
}

// ---- extra noise types (parameter indices 6 .. 6+43-1; 0..4 = White/Pink/Brown/Tape/Circuit, 5 = Sample)
inline constexpr int kNoiseA = 17, kNoiseB = 26, kNoiseOrg = 16, kExtraNoises = kNoiseA + kNoiseB + kNoiseOrg, kFirstExtraNoise = 6;   // A = Analog, B = S2 Noises, Org = Organic (appended last: old presets keep their indices)
inline const char* const extraNoiseNames[kExtraNoises] = {
    "AlphaNz", "ARP circuit", "ARP pink", "ARP white", "BrightWhite", "J 60", "J8", "J106 Chorus", "J106 HP", "J106 HP_Cho", "J106",
    "MicrKrg Noise", "OrganNoise", "SH1 Noise", "SID drone", "SID noise", "SID whine",
    "808 Transient", "Arctic Visit", "Blip", "Cicadas", "Crackle", "Egtr Mute", "Egtr Neck Down", "Egtr Neck Up", "Fizz", "FretNoise A", "FretNoise B",
    "FretNoise C", "HP12 Pink Noise (Mono)", "HP12 Pink Noise (Stereo)", "HP12 White Noise (Stereo)", "Keys and Ball", "Pebbles in Tube", "Radio Glitch",
    "Rain 100 High", "Rising Wind", "Saw", "Screws in Bowl", "Sine Wave C", "Stretched Vinyl", "Vinyl Crackle", "Whiff",
    // ---- Organic (16)
    "AC hum1", "AC hum2", "Air Can 1", "Air Can 2", "Air Can 3", "Air Can 4", "Air Can 5", "CymLoop", "CymMicBleed", "H-Breath", "Paper Bag", "S-Sound",
    "Shush 1", "Shush 2", "Squeaky", "WindChimes"
};
}
}
