#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "Patch.h"

namespace nusynth
{
struct Design { Patch patch; juce::String summary; };

/** AI sound designer (offline, rule + knowledge based).
    It reads the prompt into a vector of sound traits, recognises instruments / genres / drum sounds / textures, and then COMPOSES a new
    patch: wavetables by affinity, warp, 1-2 filters with routing, 4 envelopes, up to 6 LFOs (incl. free-form step shapes), the 24-slot mod
    matrix (voice AND effect destinations), macros, a 10-slot FX chain (reverb / delay types, analog compressor ...), arp and play mode.
    Every generation is different but stays close to the words you typed. */
namespace sd
{
enum Trait { BRIGHT, DARK, WARM, AGGR, WIDE, MOTION, SLOW, FAST, LENGTHY, BRIEF, HIGH, LOW, NOISE, METAL, VOCAL, PLUCK, SUB, SPACE, ECHO,
             GRIT, RHYTHM, LOFI, DIGITAL, ANALOG, GLIDE, MONO, VIB, WOBBLE, SWEEP, RESO, LOUD, DETUNE, PHASER, FLANGER, CHORUS,
             HOLLOW, TAPE, FUZZ, FOLD, TUBE, CLEAN, SHIMMER, PLATE, ROOM, CATHEDRAL, REVERSEFX, PINGPONG, COMP, BREATH, OPEN,
             KICK, SNARE, HAT, CLAP, TOM, CYMBAL,
             R_BASS, R_KEYS, R_PLUCK, R_PAD, R_LEAD, R_FX, R_ARP, R_PERC, NT };

struct Group { const char* words; int t[4]; float v[4]; };

inline const Group* lexicon (int& n)
{
    static const Group g[] = {
        // ---------------------------------------------------------------- roles
        { "bass basses bassline reese bottom subbass sub-bass",             { R_BASS, LOW, SUB, -1 },          { 1.0f, 0.6f, 0.3f, 0 } },
        { "808 boom thump",                                                 { R_BASS, SUB, BRIEF, -1 },        { 0.8f, 0.9f, 0.4f, 0 } },
        { "sub subby",                                                      { SUB, LOW, -1, -1 },              { 0.8f, 0.4f, 0, 0 } },
        { "keys key piano rhodes electric ep clav clavinet wurly wurlitzer harpsichord celesta vibraphone keyboard",
                                                                            { R_KEYS, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "marimba xylophone mallet mallets",                               { R_KEYS, PLUCK, METAL, -1 },      { 0.7f, 0.7f, 0.3f, 0 } },
        { "pluck plucked plucky pizzicato pizz stab stabs staccato kalimba harp koto guitar banjo",
                                                                            { R_PLUCK, PLUCK, BRIEF, -1 },     { 1.0f, 0.9f, 0.5f, 0 } },
        { "pad pads atmosphere atmospheric ambient wash drone strings ensemble soundscape texture cinematic",
                                                                            { R_PAD, SLOW, LENGTHY, SPACE },   { 1.0f, 0.5f, 0.6f, 0.4f } },
        { "lead leads solo melody melodic screamer hoover whistle brass supersaw",
                                                                            { R_LEAD, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "fx sfx riser impact laser zap glitch scifi alien whoosh swoosh siren rumble effect effects transition",
                                                                            { R_FX, MOTION, -1, -1 },          { 1.0f, 0.4f, 0, 0 } },
        { "arp arps arpeggio arpeggiated arpeggiator sequence sequenced sequencer pattern",
                                                                            { R_ARP, RHYTHM, -1, -1 },         { 1.0f, 0.5f, 0, 0 } },
        // ---------------------------------------------------------------- drums / percussion
        { "kick kickdrum bassdrum",                                         { R_PERC, KICK, -1, -1 },          { 1.0f, 1.0f, 0, 0 } },
        { "snare rimshot rim",                                              { R_PERC, SNARE, -1, -1 },         { 1.0f, 1.0f, 0, 0 } },
        { "hihat hat hats hi-hat shaker",                                   { R_PERC, HAT, -1, -1 },           { 1.0f, 1.0f, 0, 0 } },
        { "clap claps handclap",                                            { R_PERC, CLAP, -1, -1 },          { 1.0f, 1.0f, 0, 0 } },
        { "tom toms conga bongo",                                           { R_PERC, TOM, -1, -1 },           { 1.0f, 1.0f, 0, 0 } },
        { "cymbal crash ride gong",                                         { R_PERC, CYMBAL, METAL, -1 },     { 1.0f, 1.0f, 0.5f, 0 } },
        { "perc percussion drum drums hit",                                 { R_PERC, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "open",                                                           { OPEN, -1, -1, -1 },              { 1.0f, 0, 0, 0 } },
        // ---------------------------------------------------------------- tone
        { "bright sharp crisp shiny sparkly sparkling brilliant clear glassy glass crystal icy shimmer piercing",
                                                                            { BRIGHT, -1, -1, -1 },            { 0.8f, 0, 0, 0 } },
        { "airy light thin delicate fragile",                               { BRIGHT, NOISE, HIGH, -1 },       { 0.4f, 0.2f, 0.3f, 0 } },
        { "dark murky muffled dull dim gloomy sinister moody",              { DARK, -1, -1, -1 },              { 0.8f, 0, 0, 0 } },
        { "deep heavy",                                                     { DARK, LOW, -1, -1 },             { 0.5f, 0.5f, 0, 0 } },
        { "warm mellow smooth round gentle velvet cozy soft creamy silky buttery",
                                                                            { WARM, DARK, AGGR, -1 },          { 0.8f, 0.2f, -0.3f, 0 } },
        { "aggressive harsh angry nasty brutal savage violent evil mean ruthless",
                                                                            { AGGR, GRIT, BRIGHT, -1 },        { 0.9f, 0.5f, 0.2f, 0 } },
        { "dirty gritty distorted distortion crunchy crunch overdriven overdrive filthy buzzy",
                                                                            { GRIT, AGGR, -1, -1 },            { 0.9f, 0.4f, 0, 0 } },
        { "fuzz fuzzy",                                                     { FUZZ, GRIT, -1, -1 },            { 1.0f, 0.6f, 0, 0 } },
        { "tube valve tubey",                                               { TUBE, WARM, GRIT, -1 },          { 1.0f, 0.4f, 0.3f, 0 } },
        { "fold folded wavefold folding",                                   { FOLD, AGGR, DIGITAL, -1 },       { 1.0f, 0.4f, 0.4f, 0 } },
        { "clean pure simple plain minimal sine smooth-clean",              { CLEAN, WARM, GRIT, MOTION },     { 1.0f, 0.3f, -0.8f, -0.5f } },
        { "hollow nasal woody boxy",                                        { HOLLOW, -1, -1, -1 },            { 1.0f, 0, 0, 0 } },
        { "resonant squelch squelchy acid screech singing whistling sharp-resonance",
                                                                            { RESO, SWEEP, -1, -1 },           { 0.9f, 0.4f, 0, 0 } },
        { "loud powerful strong slammed compressed punchy impactful glued tight-compression",
                                                                            { LOUD, FAST, COMP, -1 },          { 0.8f, 0.3f, 0.8f, 0 } },
        { "metallic metal bell bells clang clanky steel iron robotic robot mechanical inharmonic gong industrial",
                                                                            { METAL, DIGITAL, -1, -1 },        { 0.9f, 0.3f, 0, 0 } },
        { "vocal voice choir vowel human formant singing sing aah ooh talking choral angelic",
                                                                            { VOCAL, -1, -1, -1 },             { 0.9f, 0, 0, 0 } },
        { "noisy noise breath breathy hiss static wind whisper dust grain grainy",
                                                                            { NOISE, BREATH, -1, -1 },         { 0.8f, 0.3f, 0, 0 } },
        { "vinyl crackle tape cassette dusty worn",                         { TAPE, NOISE, LOFI, -1 },         { 1.0f, 0.4f, 0.5f, 0 } },
        { "lofi vintage retro old 8bit bitcrushed chiptune",                { LOFI, ANALOG, GRIT, -1 },        { 0.9f, 0.3f, 0.3f, 0 } },
        { "digital modern futuristic cyber cyberpunk electronic synthetic hightech",
                                                                            { DIGITAL, ANALOG, -1, -1 },       { 0.9f, -0.5f, 0, 0 } },
        { "analog classic oldschool moog juno prophet oberheim",            { ANALOG, DIGITAL, WARM, -1 },     { 0.9f, -0.5f, 0.3f, 0 } },
        { "high shrill squeaky tiny small piping treble",                   { HIGH, BRIGHT, -1, -1 },          { 0.8f, 0.3f, 0, 0 } },
        { "lowend rumbling rumble growl growly",                            { LOW, SUB, -1, -1 },              { 0.7f, 0.4f, 0, 0 } },
        { "thick fat rich full",                                            { WIDE, DETUNE, WARM, -1 },        { 0.6f, 0.5f, 0.2f, 0 } },
        { "thin weak small-sound",                                          { WIDE, WARM, BRIGHT, -1 },        { -0.4f, -0.4f, 0.2f, 0 } },
        { "scooped hollow-mid",                                             { HOLLOW, WIDE, -1, -1 },          { 0.6f, 0.3f, 0, 0 } },
        { "wonky drunk wobbly detuned-wobble",                              { MOTION, DETUNE, VIB, -1 },       { 0.6f, 0.5f, 0.3f, 0 } },
        { "rubbery rubber boingy",                                          { PLUCK, RESO, WARM, -1 },         { 0.4f, 0.4f, 0.3f, 0 } },
        { "glitchy stuttery broken",                                        { DIGITAL, RHYTHM, LOFI, MOTION },  { 0.6f, 0.6f, 0.4f, 0.5f } },
        // ---------------------------------------------------------------- space / width / effects
        { "wide huge big massive epic stereo spacious broad giant",         { WIDE, DETUNE, LOUD, -1 },        { 0.9f, 0.5f, 0.2f, 0 } },
        { "lush",                                                           { WIDE, CHORUS, WARM, -1 },        { 0.6f, 0.5f, 0.3f, 0 } },
        { "narrow tight focused mono monophonic",                           { WIDE, MONO, -1, -1 },            { -0.7f, 0.6f, 0, 0 } },
        { "detuned unison ensemble",                                        { DETUNE, WIDE, -1, -1 },          { 0.9f, 0.4f, 0, 0 } },
        { "chorus chorused",                                                { CHORUS, WIDE, -1, -1 },          { 1.0f, 0.3f, 0, 0 } },
        { "spacey space hall reverb reverby cavernous ethereal dreamy floating distant heavenly celestial",
                                                                            { SPACE, SLOW, LENGTHY, -1 },      { 0.9f, 0.2f, 0.3f, 0 } },
        { "cathedral church chapel",                                        { CATHEDRAL, SPACE, LENGTHY, -1 }, { 1.0f, 0.7f, 0.4f, 0 } },
        { "shimmer shimmering shimmery octave-reverb",                      { SHIMMER, SPACE, BRIGHT, -1 },    { 1.0f, 0.6f, 0.3f, 0 } },
        { "plate",                                                          { PLATE, SPACE, -1, -1 },          { 1.0f, 0.4f, 0, 0 } },
        { "room studio booth",                                              { ROOM, SPACE, -1, -1 },           { 1.0f, 0.2f, 0, 0 } },
        { "reverse reversed backwards backward",                            { REVERSEFX, SPACE, SWEEP, -1 },   { 1.0f, 0.5f, 0.3f, 0 } },
        { "echo delay echoing repeats",                                     { ECHO, SPACE, -1, -1 },           { 0.9f, 0.2f, 0, 0 } },
        { "pingpong ping-pong bouncing-echo",                               { PINGPONG, ECHO, WIDE, -1 },      { 1.0f, 0.6f, 0.3f, 0 } },
        { "dry close",                                                      { SPACE, ECHO, -1, -1 },           { -0.7f, -0.7f, 0, 0 } },
        { "phaser phasing swirl swirling",                                  { PHASER, MOTION, -1, -1 },        { 1.0f, 0.4f, 0, 0 } },
        { "flanger flanging jet jetplane",                                  { FLANGER, MOTION, -1, -1 },       { 1.0f, 0.4f, 0, 0 } },
        { "compressed squashed pumping-compression analogcomp",             { COMP, LOUD, -1, -1 },            { 1.0f, 0.5f, 0, 0 } },
        // ---------------------------------------------------------------- time / motion
        { "evolving moving morphing animated alive motion shifting changing dynamic living breathing shape shifting",
                                                                            { MOTION, -1, -1, -1 },            { 0.9f, 0, 0, 0 } },
        { "static frozen still",                                            { MOTION, -1, -1, -1 },            { -0.9f, 0, 0, 0 } },
        { "wobble wub womp dubstep",                                        { WOBBLE, RHYTHM, AGGR, -1 },      { 1.0f, 0.4f, 0.3f, 0 } },
        { "pulsing gated chopped stutter pumping rhythmic trancegate sidechain",
                                                                            { RHYTHM, -1, -1, -1 },            { 0.9f, 0, 0, 0 } },
        { "sweep sweeping swept filtersweep rising falling",                { SWEEP, SLOW, -1, -1 },           { 0.9f, 0.4f, 0, 0 } },
        { "vibrato expressive trembling tremolo",                           { VIB, -1, -1, -1 },               { 0.9f, 0, 0, 0 } },
        { "glide portamento slide sliding legato",                          { GLIDE, MONO, -1, -1 },           { 0.9f, 0.4f, 0, 0 } },
        { "slow swell fade gradual creeping soft-attack",                   { SLOW, -1, -1, -1 },              { 0.9f, 0, 0, 0 } },
        { "fast snappy instant percussive",                                 { FAST, BRIEF, -1, -1 },           { 0.9f, 0.3f, 0, 0 } },
        { "long sustained lingering endless infinite ringing drawn tail",   { LENGTHY, -1, -1, -1 },           { 0.9f, 0, 0, 0 } },
        { "short quick brief clipped tight-release",                        { BRIEF, FAST, -1, -1 },           { 0.9f, 0.3f, 0, 0 } },
        // ---------------------------------------------------------------- genres / styles
        { "dubstep brostep riddim",                                         { WOBBLE, AGGR, R_BASS, GRIT },    { 0.8f, 0.6f, 0.6f, 0.5f } },
        { "trance",                                                         { R_LEAD, RHYTHM, WIDE, BRIGHT },  { 0.6f, 0.5f, 0.6f, 0.4f } },
        { "psytrance goa",                                                  { RHYTHM, AGGR, DIGITAL, MOTION }, { 0.7f, 0.4f, 0.4f, 0.4f } },
        { "house deephouse",                                                { R_KEYS, WARM, RHYTHM, -1 },      { 0.5f, 0.3f, 0.3f, 0 } },
        { "techno acid-techno",                                             { METAL, RHYTHM, DARK, RESO },     { 0.4f, 0.6f, 0.3f, 0.4f } },
        { "dnb drumnbass neuro neurofunk jungle",                           { R_BASS, AGGR, MOTION, DIGITAL }, { 0.7f, 0.7f, 0.6f, 0.5f } },
        { "trap hiphop hip-hop rap",                                        { R_BASS, SUB, DARK, LOUD },       { 0.5f, 0.7f, 0.4f, 0.3f } },
        { "synthwave outrun retrowave vaporwave",                           { ANALOG, WIDE, CHORUS, LOFI },    { 0.7f, 0.6f, 0.6f, 0.2f } },
        { "futurebass future-bass kawaii hyperpop",                         { WIDE, BRIGHT, DETUNE, DIGITAL }, { 0.8f, 0.6f, 0.7f, 0.5f } },
        { "chiptune 8bit gameboy nes arcade videogame",                     { LOFI, DIGITAL, BRIGHT, CLEAN },  { 0.9f, 0.5f, 0.3f, 0.3f } },
        { "idm glitchhop",                                                  { DIGITAL, MOTION, RHYTHM, METAL }, { 0.6f, 0.6f, 0.5f, 0.3f } },
        { "hardstyle gabber hardcore",                                      { AGGR, LOUD, GRIT, R_LEAD },      { 0.7f, 0.7f, 0.6f, 0.3f } },
        { "disco funk funky",                                               { ANALOG, PLUCK, BRIGHT, CHORUS }, { 0.5f, 0.3f, 0.3f, 0.3f } },
        { "ambient chill chillout downtempo lounge",                        { SPACE, SLOW, WARM, MOTION },     { 0.6f, 0.4f, 0.4f, 0.3f } },
        { "orchestral epic-cinematic trailer",                              { WIDE, LOUD, SPACE, DARK },       { 0.6f, 0.6f, 0.5f, 0.3f } },
        { "western spaghetti",                                              { ANALOG, VIB, SPACE, LOFI },      { 0.5f, 0.3f, 0.4f, 0.2f } },
        // ---------------------------------------------------------------- moods / images
        { "sad melancholic melancholy lonely cold",                         { DARK, WARM, SLOW, SPACE },       { 0.5f, 0.4f, 0.3f, 0.3f } },
        { "happy cheerful joyful sunny playful bouncy",                     { BRIGHT, FAST, -1, -1 },          { 0.6f, 0.3f, 0, 0 } },
        { "scary creepy horror eerie spooky haunting haunted",              { DARK, METAL, NOISE, SPACE },     { 0.6f, 0.4f, 0.3f, 0.4f } },
        { "underwater ocean waves sea",                                     { DARK, MOTION, SPACE, WARM },     { 0.5f, 0.6f, 0.5f, 0.3f } },
        { "cyberpunk bladerunner",                                          { DIGITAL, AGGR, DARK, WIDE },     { 0.7f, 0.4f, 0.4f, 0.4f } },
        { "80s eighties",                                                   { ANALOG, WIDE, CHORUS, LOFI },    { 0.7f, 0.6f, 0.6f, 0.2f } },
        { "angel angelic heaven",                                           { VOCAL, SPACE, BRIGHT, SHIMMER }, { 0.6f, 0.7f, 0.5f, 0.6f } },
        { "demonic devil hell",                                             { AGGR, DARK, VOCAL, GRIT },       { 0.8f, 0.7f, 0.3f, 0.5f } },
        { "organic natural acoustic",                                       { ANALOG, NOISE, WARM, -1 },       { 0.5f, 0.2f, 0.3f, 0 } },
        { "magical sparkle fairy twinkle",                                  { BRIGHT, METAL, SPACE, HIGH },    { 0.6f, 0.4f, 0.4f, 0.4f } },
        { "space-ship spaceship ufo cosmic galaxy",                         { MOTION, SPACE, DIGITAL, SWEEP }, { 0.6f, 0.7f, 0.4f, 0.4f } },
        { "storm thunder rain",                                             { NOISE, DARK, LOW, SPACE },       { 0.8f, 0.5f, 0.4f, 0.5f } },
        { "water droplet bubble bubbles",                                   { PLUCK, SPACE, RESO, BRIGHT },    { 0.6f, 0.4f, 0.5f, 0.2f } },
        { "wind-chimes chimes",                                             { METAL, BRIGHT, SPACE, LENGTHY }, { 0.7f, 0.6f, 0.4f, 0.4f } },

        // ---------------------------------------------------------------- extended vocabulary: tone synonyms
        { "glittering gleaming radiant luminous dazzling sparkle twinkling sunny golden silvery",
                                                                            { BRIGHT, HIGH, SPACE, -1 },       { 0.7f, 0.3f, 0.2f, 0 } },
        { "sizzling zingy snappy-bright cutting edgy biting searing acidic tangy sharp-edged",
                                                                            { BRIGHT, AGGR, RESO, -1 },        { 0.6f, 0.3f, 0.2f, 0 } },
        { "mellow-dark shadowy smoky foggy hazy misty cloudy veiled underground subterranean",
                                                                            { DARK, WARM, SPACE, -1 },         { 0.6f, 0.3f, 0.3f, 0 } },
        { "midnight dusk twilight nocturnal moonlit nightfall nighttime night evening nightly",
                                                                            { DARK, WARM, SPACE, SLOW },       { 0.6f, 0.4f, 0.4f, 0.3f } },
        { "sunrise dawn morning daybreak sunshine daylight breezy fresh",   { BRIGHT, WARM, NOISE, -1 },   { 0.5f, 0.3f, 0, 0 } },
        { "sunset golden-hour autumn evening-glow nostalgic nostalgia",     { WARM, SLOW, SPACE, ANALOG },     { 0.6f, 0.4f, 0.3f, 0.3f } },
        { "winter snow frost frozen icy ice glacier arctic polar",          { BRIGHT, SPACE, SLOW, METAL },    { 0.6f, 0.5f, 0.3f, 0.2f } },
        { "summer tropical beach island sunny-day",                         { BRIGHT, WARM, FAST, -1 },{ 0.5f, 0.4f, 0, 0 } },
        { "forest jungle woods woodland nature meadow garden birds",        { WARM, NOISE, MOTION, SPACE },    { 0.4f, 0.3f, 0.4f, 0.3f } },
        { "desert dune canyon arid sahara dry-heat",                        { DARK, METAL, VIB, SPACE },       { 0.3f, 0.2f, 0.4f, 0.4f } },
        { "mountain cliff peak highland alpine himalaya",                   { WIDE, SPACE, LOUD, BRIGHT },     { 0.6f, 0.6f, 0.3f, 0.2f } },
        { "river stream creek lake pond waterfall water",                   { MOTION, NOISE, BRIGHT, PLUCK },  { 0.6f, 0.4f, 0.3f, 0.3f } },
        { "rain raindrops drizzle shower rainy monsoon",                    { NOISE, PLUCK, SPACE, MOTION },   { 0.7f, 0.4f, 0.4f, 0.4f } },
        { "sea ocean seaside waves tide shore harbour surf marine",         { DARK, NOISE, MOTION, SPACE },    { 0.4f, 0.5f, 0.7f, 0.4f } },
        { "city street urban traffic downtown metro subway skyline neon",   { DIGITAL, METAL, RHYTHM, DARK },  { 0.5f, 0.3f, 0.3f, 0.3f } },
        { "club party dancefloor festival rave warehouse after-hours afterhours",
                                                                            { RHYTHM, LOUD, WIDE, BRIGHT },    { 0.6f, 0.5f, 0.3f, 0.3f } },
        { "temple monastery ritual meditation zen calm peaceful serene tranquil relaxing relax",
                                                                            { SLOW, SPACE, WARM, CLEAN },      { 0.6f, 0.5f, 0.4f, 0.4f } },
        { "cave tunnel cavern dungeon underground bunker",                  { DARK, SPACE, LOW, METAL },       { 0.6f, 0.7f, 0.3f, 0.2f } },
        { "moon lunar planet stars starry nebula galaxy universe orbit satellite astronaut",
                                                                            { SPACE, MOTION, SHIMMER, SLOW },  { 0.7f, 0.4f, 0.4f, 0.4f } },
        { "fire flame burning inferno lava volcano explosion blast",        { AGGR, NOISE, LOUD, LOW },        { 0.6f, 0.5f, 0.5f, 0.3f } },
        { "wind breeze gust gale air-flow airflow whoosh-wind",             { NOISE, MOTION, BREATH, SLOW },   { 0.7f, 0.4f, 0.4f, 0.3f } },
        { "thunderstorm lightning tempest hurricane",                       { NOISE, DARK, LOUD, AGGR },       { 0.7f, 0.5f, 0.4f, 0.3f } },
        { "bird birds chirp tweet insect crickets frogs",                   { HIGH, PLUCK, NOISE, BRIGHT },    { 0.6f, 0.5f, 0.3f, 0.3f } },
        // ---------------------------------------------------------------- texture / character synonyms
        { "gooey sticky sludgy muddy swampy slimy",                         { DARK, RESO, WARM, MOTION },      { 0.5f, 0.4f, 0.4f, 0.3f } },
        { "crispy crackly snappy-noise popping clicky ticks",               { NOISE, BRIGHT, BRIEF, DIGITAL },  { 0.5f, 0.4f, 0.3f, 0.3f } },
        { "silky satin liquid fluid flowing flowy watery drippy",           { WARM, MOTION, SLOW, CHORUS },    { 0.6f, 0.5f, 0.4f, 0.3f } },
        { "fuzzy-warm blurry blurred smeared washed hazy-pad",              { WARM, SPACE, SLOW, LOFI },       { 0.6f, 0.5f, 0.4f, 0.3f } },
        { "razor razor-sharp serrated jagged spiky needle",                 { BRIGHT, AGGR, HIGH, METAL },     { 0.8f, 0.6f, 0.3f, 0.3f } },
        { "cinematic epic-trailer trailer-hit blockbuster dramatic",        { LOUD, SPACE, DARK, WIDE },       { 0.6f, 0.6f, 0.4f, 0.4f } },
        { "tension suspense suspenseful ominous menacing threatening",      { DARK, METAL, SLOW, NOISE },      { 0.7f, 0.4f, 0.5f, 0.3f } },
        { "dreamlike dreamscape lucid surreal psychedelic trippy",          { MOTION, SPACE, WIDE, PHASER },   { 0.7f, 0.6f, 0.5f, 0.4f } },
        { "innocent childlike cute kawaii tiny-toy toy toylike",            { BRIGHT, HIGH, PLUCK, CLEAN },    { 0.6f, 0.5f, 0.4f, 0.3f } },
        { "heroic triumphant majestic grand noble royal anthem",            { BRIGHT, WIDE, LOUD, SPACE },     { 0.5f, 0.6f, 0.5f, 0.3f } },
        { "romantic tender sensual intimate love lovely soulful",           { WARM, CLEAN, SPACE, SLOW }, { 0.7f, 0.2f, 0.3f, 0.3f } },
        { "funky groovy groove swing swinging shuffle",                     { RHYTHM, PLUCK, BRIGHT, CHORUS }, { 0.5f, 0.4f, 0.3f, 0.2f } },
        { "bouncy bounce springy elastic boing",                            { PLUCK, RESO, FAST, WARM },       { 0.5f, 0.4f, 0.3f, 0.2f } },
        { "chugging driving relentless hypnotic repetitive loopy minimal-loop",
                                                                            { RHYTHM, MOTION, DARK, -1 },      { 0.7f, 0.4f, 0.2f, 0 } },
        { "evil-laugh creepy-voice ghostly spirit spectral phantom wraith", { VOCAL, SPACE, DARK, NOISE },     { 0.5f, 0.6f, 0.5f, 0.3f } },
        // ---------------------------------------------------------------- synth / hardware names
        { "juno jupiter prophet oberheim sequential korg roland moog arp-odyssey minimoog sh101 jx8p cs80 ms20",
                                                                            { ANALOG, WARM, CHORUS, -1 },      { 0.9f, 0.3f, 0.3f, 0 } },
        { "dx7 fm8 yamaha-dx opl2 fm-synth fm-piano",                       { DIGITAL, METAL, BRIGHT, -1 },    { 0.8f, 0.4f, 0.3f, 0 } },
        { "serum massive sylenth vital diva wavetable",                     { DIGITAL, WIDE, BRIGHT, MOTION }, { 0.5f, 0.4f, 0.3f, 0.3f } },
        { "analogue vco vca valve-synth polysynth poly-synth",              { ANALOG, WARM, -1, -1 },          { 0.8f, 0.3f, 0, 0 } },
        { "sampler sample-based romple rompler mellotron tape-keys",        { LOFI, TAPE, WARM, ANALOG },      { 0.4f, 0.6f, 0.4f, 0.3f } },
        { "granular grains cloud clouds particle particles",                { NOISE, MOTION, DIGITAL, SPACE }, { 0.6f, 0.6f, 0.5f, 0.4f } },
        // ---------------------------------------------------------------- production words
        { "sidechain sidechained ducked ducking pumping-bass",              { RHYTHM, LOUD, COMP, -1 },        { 0.8f, 0.4f, 0.5f, 0 } },
        { "saturated saturation warm-saturation analog-drive",              { GRIT, WARM, TUBE, -1 },          { 0.6f, 0.4f, 0.3f, 0 } },
        { "bitcrush bitcrushed bit-crush crusher decimated downsampled",    { LOFI, DIGITAL, GRIT, -1 },       { 0.9f, 0.4f, 0.4f, 0 } },
        { "stereo-wide ultra-wide widened width spread-out",                { WIDE, CHORUS, -1, -1 },          { 0.9f, 0.3f, 0, 0 } },
        { "mono-compatible centred centered",                               { WIDE, MONO, -1, -1 },            { -0.6f, 0.4f, 0, 0 } },
        { "stutter gated-pad choppy chop chopped slicer sliced",            { RHYTHM, FAST, DIGITAL, -1 },     { 0.8f, 0.3f, 0.2f, 0 } },
        { "filtered filtering lowpassed highpassed bandpassed",             { SWEEP, RESO, -1, -1 },           { 0.5f, 0.3f, 0, 0 } },
        { "resampled bounced processed mangled destroyed ruined smashed",   { GRIT, DIGITAL, AGGR, LOFI },     { 0.7f, 0.4f, 0.5f, 0.3f } },
        { "layered stacked doubled octave-layer octaves",                   { WIDE, DETUNE, LOUD, -1 },        { 0.5f, 0.4f, 0.3f, 0 } },

        // ---------------------------------------------------------------- sound-design jargon
        { "pwm pulsewidth pulse-width pulsewave pulse-wave",                { MOTION, ANALOG, HOLLOW, -1 },    { 0.5f, 0.4f, 0.3f, 0 } },
        { "hardsync hard-sync syncsweep sync-sweep sync-lead oscsync",      { AGGR, BRIGHT, DIGITAL, MOTION }, { 0.5f, 0.5f, 0.3f, 0.3f } },
        { "ringmod ring-mod ringmodulator ring-modulated sidebands",        { METAL, DIGITAL, AGGR, -1 },      { 0.8f, 0.4f, 0.2f, 0 } },
        { "additive harmonic harmonics partials overtone overtones inharmonic-partials",
                                                                            { DIGITAL, METAL, BRIGHT, -1 },    { 0.4f, 0.4f, 0.2f, 0 } },
        { "subtractive classic-subtractive vco-synth",                      { ANALOG, WARM, -1, -1 },          { 0.7f, 0.3f, 0, 0 } },
        { "formants formanty vowelly speech speechy talkbox talk-box vocoder vocoded",
                                                                            { VOCAL, DIGITAL, MOTION, -1 },    { 0.8f, 0.3f, 0.3f, 0 } },
        { "comb combfilter comb-filter karplus string-like resonator resonating",
                                                                            { METAL, RESO, PLUCK, -1 },        { 0.6f, 0.4f, 0.3f, 0 } },
        { "blip blips bleep bleeps beep beeps boop boops ping pings",       { PLUCK, BRIEF, HIGH, DIGITAL },   { 0.7f, 0.7f, 0.5f, 0.3f } },
        { "pop pops click clicks tick ticks tap taps knock knocks",         { BRIEF, FAST, NOISE, -1 },        { 0.6f, 0.5f, 0.3f, 0 } },
        { "zap zaps zip zips whip whips swish swoosh-fast",                 { SWEEP, FAST, BRIGHT, NOISE },    { 0.6f, 0.6f, 0.3f, 0.3f } },
        { "drop drops downlifter downlift falling-sweep fall",              { SWEEP, DARK, BRIEF, -1 },        { 0.7f, 0.3f, 0.2f, 0 } },
        { "uplifter uplift build buildup build-up tension-riser crescendo", { SWEEP, SLOW, BRIGHT, NOISE },    { 0.9f, 0.5f, 0.4f, 0.3f } },
        { "subdrop sub-drop boom-drop bassdrop bass-drop",                  { SUB, LOW, BRIEF, LOUD },         { 0.9f, 0.7f, 0.4f, 0.4f } },
        { "swell swells pulse-swell breathing-pad",                         { SLOW, MOTION, LENGTHY, -1 },     { 0.7f, 0.5f, 0.3f, 0 } },
        // ---------------------------------------------------------------- feelings
        { "angry furious rage raging enraged hostile",                      { AGGR, GRIT, LOUD, -1 },          { 0.9f, 0.5f, 0.4f, 0 } },
        { "calm calming soothing gentle-soft lullaby sleepy drowsy dozing", { WARM, SLOW, SPACE, CLEAN },      { 0.7f, 0.6f, 0.4f, 0.3f } },
        { "excited energetic energy hyper upbeat lively vibrant",           { BRIGHT, FAST, RHYTHM, WIDE },    { 0.6f, 0.6f, 0.4f, 0.3f } },
        { "anxious nervous tense uneasy unsettling disturbing creepy-tension",
                                                                            { DARK, METAL, MOTION, NOISE },    { 0.5f, 0.5f, 0.5f, 0.3f } },
        { "mystical mysterious mystery enchanted enchanting arcane occult witchy",
                                                                            { SPACE, DARK, MOTION, VOCAL },    { 0.6f, 0.4f, 0.4f, 0.3f } },
        { "playful goofy silly funny quirky cartoon cartoonish comic",      { BRIGHT, PLUCK, FAST, RESO },     { 0.5f, 0.5f, 0.4f, 0.3f } },
        { "serious solemn grave somber sombre funeral",                     { DARK, SLOW, WARM, LOW },         { 0.6f, 0.5f, 0.4f, 0.3f } },
        { "hopeful uplifting inspiring optimistic bright-future",           { BRIGHT, WIDE, SLOW, SPACE },     { 0.6f, 0.5f, 0.4f, 0.3f } },
        { "nostalgic memories childhood-memory old-school oldschool throwback",
                                                                            { LOFI, ANALOG, WARM, CHORUS },    { 0.5f, 0.4f, 0.4f, 0.3f } },
        { "lonely isolated empty hollow-space void abyss",                  { DARK, SPACE, SLOW, HOLLOW },     { 0.6f, 0.6f, 0.4f, 0.4f } },
        // ---------------------------------------------------------------- genres (more)
        { "pop edm electropop synthpop dance-pop mainstream radio",         { BRIGHT, WIDE, LOUD, DETUNE },    { 0.5f, 0.5f, 0.4f, 0.4f } },
        { "rock metal punk grunge garage-rock guitar-like power-chord",     { AGGR, GRIT, LOUD, FUZZ },        { 0.7f, 0.5f, 0.4f, 0.4f } },
        { "jazz jazzy bossa bebop swing-jazz lounge-jazz smooth-jazz",      { WARM, VIB, SPACE, CLEAN },       { 0.6f, 0.4f, 0.2f, 0.2f } },
        { "classical baroque renaissance medieval chamber orchestra-hit",   { CLEAN, ANALOG, SPACE, -1 },      { 0.6f, 0.5f, 0.3f, 0 } },
        { "folk country bluegrass acoustic-folk campfire western-folk",     { WARM, PLUCK, ANALOG, NOISE },    { 0.5f, 0.5f, 0.4f, 0.1f } },
        { "latin salsa reggaeton cumbia bossa-nova samba tropical-house",   { PLUCK, BRIGHT, RHYTHM, WARM },   { 0.5f, 0.4f, 0.4f, 0.3f } },
        { "tribal ethnic world african asian indian arabic celtic oriental exotic",
                                                                            { PLUCK, VOCAL, METAL, SPACE },    { 0.4f, 0.3f, 0.3f, 0.3f } },
        { "gospel soul rnb r&b neo-soul motown",                            { WARM, CHORUS, VIB, SPACE },      { 0.6f, 0.4f, 0.3f, 0.3f } },
        { "lofihiphop lo-fi-hiphop chillhop bedroom indie dreampop dream-pop shoegaze",
                                                                            { LOFI, WARM, CHORUS, SPACE },     { 0.7f, 0.5f, 0.5f, 0.4f } },
        { "djent metalcore deathcore tech-metal",                           { AGGR, LOW, GRIT, LOUD },         { 0.8f, 0.6f, 0.5f, 0.4f } },
        { "futurefunk future-funk vaporfunk nu-disco nudisco",              { ANALOG, WIDE, CHORUS, BRIGHT },  { 0.6f, 0.6f, 0.6f, 0.3f } },
        { "breakbeat breaks big-beat bigbeat jungle-break amen",            { AGGR, RHYTHM, GRIT, DIGITAL },   { 0.5f, 0.5f, 0.4f, 0.3f } },
        { "garage ukgarage 2step 2-step speed-garage grime drill",          { DARK, RHYTHM, SUB, BRIEF },      { 0.4f, 0.4f, 0.5f, 0.3f } },
        { "electro electrofunk electro-funk miami robot-funk",              { DIGITAL, ANALOG, METAL, FAST },  { 0.5f, 0.3f, 0.3f, 0.3f } },
        { "italo italo-disco hi-nrg hinrg eurodance euro-dance",            { ANALOG, BRIGHT, CHORUS, WIDE },  { 0.6f, 0.5f, 0.4f, 0.4f } },
        { "acidhouse acid-house chicago-house chicagohouse",                { RESO, RHYTHM, ANALOG, SWEEP },   { 0.7f, 0.5f, 0.4f, 0.3f } },
        { "progressive prog-house proghouse melodic-house melodichouse",    { WIDE, BRIGHT, SPACE, MOTION },   { 0.5f, 0.5f, 0.4f, 0.3f } },
        { "melodic-techno melodictechno minimal-techno microhouse minimal", { RHYTHM, DARK, MOTION, METAL },   { 0.5f, 0.5f, 0.4f, 0.3f } },
        { "bigroom big-room mainstage festival-lead",                       { LOUD, WIDE, BRIGHT, DETUNE },    { 0.7f, 0.7f, 0.5f, 0.4f } },
        { "triphop trip-hop downtempo-beat downbeat",                       { DARK, LOFI, SLOW, SPACE },       { 0.5f, 0.5f, 0.4f, 0.3f } },
        { "braindance braindance-idm autechre-like warp-style",             { DIGITAL, METAL, MOTION, GRIT },  { 0.6f, 0.5f, 0.5f, 0.4f } },
        // ---------------------------------------------------------------- instrument words that only set traits
        { "theremin ondes-martenot ondes saw-whistle",                      { VIB, SPACE, CLEAN, GLIDE },      { 0.8f, 0.4f, 0.4f, 0.5f } },
        { "gamelan kora oud erhu bansuri shakuhachi balalaika",             { PLUCK, METAL, SPACE, WARM },     { 0.7f, 0.5f, 0.4f, 0.3f } },
        { "didgeridoo bagpipe bagpipes drone-pipe",                         { LOW, AGGR, VIB, NOISE },         { 0.6f, 0.3f, 0.4f, 0.3f } },
        { "melodica zither dulcimer autoharp toy-piano toypiano",           { PLUCK, BRIGHT, HIGH, ANALOG },   { 0.5f, 0.5f, 0.3f, 0.3f } },
    };
    n = (int) (sizeof (g) / sizeof (g[0]));
    return g;
}

/** Instruments / textures that choose specific wavetables and behaviour. atk / rel / sus: -9 = no opinion. flags: 1 vibrato, 2 breath, 4 formant, 8 mono */
struct Arch { const char* words; const char* a; const char* b; const char* c; int role; float atk, rel, sus; int flags; };
inline const Arch* archetypes (int& n)
{
    static const Arch a[] = {
        { "brass trumpet trombone horn horns tuba",        "Analog Brass",     "Basic Saw",       nullptr,          R_LEAD,  0.03f, 0.25f, 0.8f,  1 },
        { "strings string violin cello viola orchestra",   "Analog Strings",   "Super Saw Stack", "Soft Saw",       R_PAD,   0.4f,  0.8f,  0.9f,  1 },
        { "flute pan-flute panflute recorder ocarina",     "Basic Sine",       "Basic Triangle",  nullptr,          R_LEAD,  0.06f, 0.3f,  0.9f,  3 },
        { "oboe clarinet bassoon reed",                    "Odd Harmonics",    "Pulse 25",        nullptr,          R_LEAD,  0.05f, 0.25f, 0.85f, 1 },
        { "sax saxophone",                                 "Odd Harmonics",    "Vowel O",         "Basic Saw",      R_LEAD,  0.04f, 0.25f, 0.85f, 3 },
        { "organ hammond church-organ pipe-organ",         "Additive Organ",   "Odd Harmonics",   nullptr,          R_KEYS,  0.01f, 0.1f,  1.0f,  1 },
        { "accordion harmonica",                           "Additive Organ",   "Pulse 12",        nullptr,          R_KEYS,  0.04f, 0.15f, 0.9f,  3 },
        { "bell bells glockenspiel chime",                 "FM Bell",          "Digital Bell",    "Glass Harmonic", R_KEYS,  0.001f,1.8f,  0.0f,  0 },
        { "tubular church-bell",                           "FM Metallic",      "FM Bell",         nullptr,          R_KEYS,  0.001f,2.5f,  0.0f,  0 },
        { "piano grand upright",                           "Piano Partials",   "FM EP",           "Basic Triangle", R_KEYS,  0.002f,0.6f,  0.2f,  0 },
        { "rhodes wurly electric-piano",                   "FM EP",            "Basic Sine",      nullptr,          R_KEYS,  0.002f,0.5f,  0.4f,  0 },
        { "guitar acoustic-guitar nylon",                  "FM Pluck",         "Pulse 12",        nullptr,          R_PLUCK, 0.001f,0.4f,  0.0f,  0 },
        { "banjo sitar mandolin lute",                     "FM Pluck",         "Piano Partials",  "Pulse 25",       R_PLUCK, 0.001f,0.35f, 0.0f,  0 },
        { "harp",                                          "FM Pluck",         "Basic Triangle",  nullptr,          R_PLUCK, 0.001f,0.9f,  0.0f,  0 },
        { "kalimba thumb-piano music-box musicbox",        "Glass Harmonic",   "FM Bell",         nullptr,          R_PLUCK, 0.001f,0.7f,  0.0f,  0 },
        { "steeldrum steel-drum handpan",                  "FM Metallic",      "FM Clang",        nullptr,          R_PLUCK, 0.001f,0.6f,  0.0f,  0 },
        { "choir choral aah ooh voice vocal",              "Additive Choir",   "Vowel O",         "Vowel A",        R_PAD,   0.5f,  1.2f,  0.9f,  5 },
        { "whisper-voice formant-voice robot-voice",       "Vowel AEIOU",      "Vowel Female",    nullptr,          R_LEAD,  0.02f, 0.3f,  0.9f,  4 },
        { "supersaw super-saw trance-lead",                "Super Saw Stack",  "Basic Saw",       "Super Saw Stack",R_LEAD,  0.005f,0.25f, 0.85f, 0 },
        { "hoover mentasm",                                "Rasp",             "Super Saw Stack", nullptr,          R_LEAD,  0.01f, 0.3f,  0.85f, 0 },
        { "chiptune square-lead 8bit-lead",                "Pulse 25",         "Basic Square",    nullptr,          R_LEAD,  0.001f,0.1f,  0.9f,  0 },
        { "reese neuro-bass",                              "Basic Saw",        "Basic Saw",       "Digital Edge",   R_BASS,  0.005f,0.3f,  0.85f, 8 },
        { "fm-bass fmbass",                                "FM Bass",          "FM Growl",        nullptr,          R_BASS,  0.002f,0.2f,  0.7f,  8 },
        { "acid-bass tb303 303",                           "Basic Saw",        "Basic Square",    nullptr,          R_BASS,  0.002f,0.15f, 0.1f,  8 },
        { "whale ghost-voice",                             "Spectral Freeze",  "Vowel O",         nullptr,          R_PAD,   1.5f,  3.0f,  1.0f,  4 },
        { "glass-pad glasspad",                            "Glass Harmonic",   "Additive Choir",  "Spectral Pad",   R_PAD,   0.8f,  2.5f,  0.9f,  0 },
        { "analog-pad warm-pad",                           "Analog Pad",       "Analog Strings",  nullptr,          R_PAD,   0.7f,  1.6f,  0.9f,  0 },
        { "laser pewpew zap",                              "FM Sweep",         "Basic Saw",       nullptr,          R_FX,    0.001f,0.3f,  0.0f,  0 },
        { "siren alarm police",                            "Basic Sine",       "Basic Triangle",  nullptr,          R_FX,    0.01f, 0.3f,  1.0f,  0 },
        { "telephone ringtone modem robot-beep r2d2",      "Pulse 25",         "FM Chaos",        nullptr,          R_FX,    0.001f,0.15f, 0.7f,  0 },
        { "wind ocean rain storm",                         "Noise Burst",      "Spectral Freeze", nullptr,          R_FX,    1.5f,  2.5f,  1.0f,  2 },
        { "vinyl static radio",                            "Rasp",             "Noise Burst",     nullptr,          R_FX,    0.01f, 0.4f,  0.8f,  2 },

        { "theremin ondes",                                "Basic Sine",       "Basic Triangle",  nullptr,          R_LEAD,  0.08f, 0.4f,  1.0f,  9 },
        { "melodica harmonium reed-organ",                 "Additive Organ",   "Pulse 12",        nullptr,          R_KEYS,  0.04f, 0.15f, 0.9f,  3 },
        { "gamelan",                                       "FM Metallic",      "FM Bell",         "Glass Harmonic", R_PLUCK, 0.001f,1.0f,  0.0f,  0 },
        { "sitar",                                         "Comb Tear",        "FM Pluck",        nullptr,          R_PLUCK, 0.001f,0.8f,  0.0f,  0 },
        { "shakuhachi bansuri",                            "Basic Sine",       "Odd Harmonics",   nullptr,          R_LEAD,  0.08f, 0.4f,  0.9f,  3 },
        { "didgeridoo",                                    "Vowel Growl",      "Basic Square",    nullptr,          R_BASS,  0.05f, 0.2f,  1.0f,  10 },
        { "bagpipe bagpipes",                              "Odd Harmonics",    "Pulse 12",        "Basic Saw",      R_LEAD,  0.02f, 0.1f,  1.0f,  1 },
        { "toypiano toy-piano music-box-high",             "Glass Harmonic",   "FM Bell",         nullptr,          R_KEYS,  0.001f,0.6f,  0.0f,  0 },
        { "vibes vibraphone",                              "FM Bell",          "Basic Sine",      nullptr,          R_KEYS,  0.002f,1.5f,  0.1f,  1 },
        { "clavinet clav",                                 "Pulse 12",         "Basic Saw",       nullptr,          R_KEYS,  0.001f,0.2f,  0.1f,  0 },
        { "cs80 cs-80 vangelis",                           "Analog Brass",     "Analog Strings",  "Super Saw Stack",R_LEAD,  0.2f,  0.8f,  0.9f,  1 },
        { "jupiter-strings juno-strings",                  "Analog Strings",   "Super Saw Stack", nullptr,          R_PAD,   0.5f,  1.0f,  0.9f,  0 },
        { "mellotron tape-choir",                          "Additive Choir",   "Analog Strings",  nullptr,          R_PAD,   0.15f, 0.8f,  0.9f,  0 },
        { "dubstep-growl growl-bass wub-bass",             "Harsh Fold",       "Rasp",            "Super Saw Stack",R_BASS,  0.003f,0.3f,  0.8f,  8 },
        { "pluck-bass slap-bass slapbass",                 "Pulse 12",         "Basic Saw",       nullptr,          R_BASS,  0.001f,0.25f, 0.1f,  8 },
    };
    n = (int) (sizeof (a) / sizeof (a[0]));
    return a;
}

/** wavetable affinity: bright, soft, aggressive, metallic, vocal, evolving, digital, pure  (0-9 each) */
inline const char* tableTraits (int i)
{
    static const char* t[73] = {
        "62300010","61200021","32100029","10100009","52300021","62400021","83301100","72300110","34000041","44100021","42100031","31100029",
        "75105410","41000131","32005141","53007510","42100131","42100221","91005410","44007621","83502930","44090421","54090521","42000131",
        "86009510","52000421","53200921","83807610","85809310","74005510","63007520","65007620","65906710","62009510","73509710","78909910",
        "86908810","76908720","74908910","86909810","74808700","75907720","54009510","66808900","85809910","85909810",
        "32001220","62101210","52102120","62100320","34006710","43097510","53305610","85809710","43007920","44005520",
        "34090420","34090420","34090420","34090420","34090420","34097620","44090420","54990430","33090520",
        "11000009","11000009","11000009","11000009","11000009","11000009","11000009","11000009" };
    return t[std::min (72, std::max (0, i))];
}
}


/** Compound-phrase recipes ("pad warm night", "bass tech house", "303 acid", "stab chicago" ...). All words of `req` must appear
    (in any order); the recipe with the most matching words wins. Everything it leaves at the default (-1 / nullptr) is still designed
    from the traits, so a recipe is a strong starting point - never a copy of a preset. */
struct Recipe
{
    const char* req = "";
    int role = -1;
    const char* wa = nullptr; const char* wb = nullptr; const char* wc = nullptr;
    const char* warp = nullptr; float warpAmt = 0.4f;
    const char* ftype = nullptr; float cut = -1.0f; float res = -1.0f;
    float atk = -1.0f, dec = -1.0f, sus = -1.0f, rel = -1.0f;
    int uni = -1; float det = -1.0f;
    float sub = -1.0f; const char* noise = nullptr; float noiseLvl = 0.0f;
    int mode = -1; float glide = -1.0f;                        // mode: 0 poly, 1 mono, 2 legato
    const char* rev = nullptr; float revMix = 0.3f; const char* dly = nullptr; float dlyMix = 0.25f;
    float chorus = -1.0f; const char* dist = nullptr; float drive = 12.0f; bool comp = false;
    int lfo = 0;                                               // 1 slow sweep, 2 wt morph, 3 wobble, 4 step gate, 5 vibrato, 6 acid accent, 7 droplets, 8 pump
    bool arp = false;
    int ta = -1; float va = 0.0f; int tb = -1; float vb = 0.0f; int tc = -1; float vc = 0.0f;   // trait nudges
};

inline const Recipe* recipes (int& n)
{
    static const Recipe r[] = {
        // ================================================================ ambient / pads
        { .req = "ambient pad", .role = R_PAD, .wa = "Spectral Pad", .wb = "Pad Evolve", .wc = "Analog Pad", .ftype = "Low Pass 12", .cut = 2800.0f, .res = 0.05f, .atk = 2.2f, .dec = 1.5f, .sus = 0.9f, .rel = 3.5f, .uni = 4, .det = 0.35f, .rev = "Cathedral", .revMix = 0.5f, .chorus = 0.4f, .lfo = 2, .ta = SPACE, .va = 0.7f },
        { .req = "ambient fx rain", .role = R_FX, .wa = "Noise Burst", .wb = "Spectral Freeze", .ftype = "Band Pass 12", .cut = 2600.0f, .res = 0.2f, .atk = 1.5f, .dec = 1.0f, .sus = 1.0f, .rel = 3.0f, .noise = "Pink", .noiseLvl = 0.7f, .rev = "Plate", .revMix = 0.45f, .lfo = 7, .ta = SPACE, .va = 0.5f },
        { .req = "ambient night", .role = R_PAD, .wa = "Analog Pad", .wb = "Glass Harmonic", .ftype = "Low Pass 24", .cut = 1500.0f, .atk = 2.0f, .sus = 0.9f, .rel = 3.0f, .rev = "Hall", .revMix = 0.5f, .dly = "Tape Echo", .dlyMix = 0.2f, .lfo = 1, .ta = DARK, .va = 0.5f },
        { .req = "ambient sea", .role = R_PAD, .wa = "Pad Evolve", .wb = "Basic Sine", .ftype = "Low Pass 24", .cut = 1200.0f, .atk = 3.0f, .sus = 1.0f, .rel = 4.0f, .noise = "Brown", .noiseLvl = 0.3f, .rev = "Hall", .revMix = 0.5f, .chorus = 0.4f, .lfo = 1, .ta = MOTION, .va = 0.6f },
        { .req = "ambient forest", .role = R_PAD, .wa = "Additive Choir", .wb = "Spectral Pad", .ftype = "Low Pass 12", .cut = 2400.0f, .atk = 1.8f, .rel = 3.0f, .noise = "Pink", .noiseLvl = 0.15f, .rev = "Room", .revMix = 0.4f, .lfo = 7, .ta = WARM, .va = 0.5f },
        { .req = "ambient space", .role = R_PAD, .wa = "Spectral Pad", .wb = "Glass Harmonic", .ftype = "Low Pass 12", .cut = 3500.0f, .atk = 2.5f, .rel = 4.0f, .rev = "Shimmer", .revMix = 0.55f, .dly = "Ping-Pong", .dlyMix = 0.3f, .lfo = 2 },
        { .req = "ambient drone", .role = R_PAD, .wa = "Basic Sine", .wb = "Spectral Freeze", .wc = "Analog Pad", .ftype = "Low Pass 24", .cut = 900.0f, .atk = 3.5f, .sus = 1.0f, .rel = 5.0f, .sub = 0.4f, .rev = "Cathedral", .revMix = 0.5f, .lfo = 1 },
        { .req = "ambient wind", .role = R_FX, .wa = "Noise Burst", .ftype = "Band Pass 12", .cut = 1200.0f, .atk = 2.0f, .sus = 1.0f, .rel = 3.5f, .noise = "White", .noiseLvl = 0.6f, .rev = "Hall", .revMix = 0.4f, .lfo = 1 },
        { .req = "ambient snow", .role = R_PAD, .wa = "Glass Harmonic", .wb = "Additive Choir", .ftype = "Low Pass 12", .cut = 6000.0f, .atk = 1.6f, .rel = 3.5f, .rev = "Shimmer", .revMix = 0.5f, .lfo = 2 },
        { .req = "pad saw", .role = R_PAD, .wa = "Basic Saw", .wb = "Super Saw Stack", .ftype = "Ladder LP 24", .cut = 2200.0f, .atk = 0.6f, .sus = 0.9f, .rel = 1.2f, .uni = 6, .det = 0.35f, .rev = "Hall", .revMix = 0.3f, .chorus = 0.5f },
        { .req = "pad soft", .role = R_PAD, .wa = "Soft Saw", .wb = "Analog Pad", .ftype = "MG Low", .cut = 1400.0f, .atk = 1.0f, .sus = 0.9f, .rel = 1.8f, .rev = "Plate", .revMix = 0.3f, .chorus = 0.35f, .ta = WARM, .va = 0.5f },
        { .req = "pad warm", .role = R_PAD, .wa = "Analog Pad", .wb = "Analog Strings", .ftype = "Low Pass 24", .cut = 1700.0f, .atk = 0.8f, .sus = 0.9f, .rel = 1.6f, .rev = "Hall", .revMix = 0.3f, .chorus = 0.5f, .ta = WARM, .va = 0.7f },
        { .req = "pad warm night", .role = R_PAD, .wa = "Analog Pad", .wb = "Analog Strings", .wc = "Soft Saw", .ftype = "Low Pass 24", .cut = 1050.0f, .atk = 1.4f, .sus = 0.9f, .rel = 2.8f, .rev = "Cathedral", .revMix = 0.4f, .dly = "Tape Echo", .dlyMix = 0.15f, .chorus = 0.5f, .lfo = 1, .ta = DARK, .va = 0.5f },
        { .req = "pad soft night", .role = R_PAD, .wa = "Soft Saw", .wb = "Glass Harmonic", .ftype = "MG Low", .cut = 950.0f, .atk = 1.6f, .sus = 0.9f, .rel = 3.0f, .rev = "Plate", .revMix = 0.4f, .chorus = 0.4f, .lfo = 1, .ta = DARK, .va = 0.5f },
        { .req = "pad space", .role = R_PAD, .wa = "Spectral Pad", .wb = "Super Saw Stack", .ftype = "Low Pass 12", .cut = 3200.0f, .atk = 1.2f, .rel = 2.5f, .rev = "Shimmer", .revMix = 0.5f, .dly = "Ping-Pong", .dlyMix = 0.25f, .lfo = 2, .ta = WIDE, .va = 0.6f },
        { .req = "pad dreamy", .role = R_PAD, .wa = "Glass Harmonic", .wb = "Additive Choir", .ftype = "Low Pass 12", .cut = 4200.0f, .atk = 1.5f, .rel = 3.0f, .rev = "Shimmer", .revMix = 0.45f, .chorus = 0.5f, .lfo = 2 },
        { .req = "pad dream", .role = R_PAD, .wa = "Analog Pad", .wb = "Glass Harmonic", .wc = "Additive Choir", .ftype = "Low Pass 12", .cut = 3600.0f, .atk = 1.3f, .rel = 2.8f, .rev = "Cathedral", .revMix = 0.45f, .chorus = 0.5f, .lfo = 2 },
        { .req = "pad trance", .role = R_PAD, .wa = "Super Saw Stack", .wb = "Basic Saw", .ftype = "Ladder LP 12", .cut = 4500.0f, .atk = 0.15f, .rel = 0.8f, .uni = 8, .det = 0.45f, .rev = "Plate", .revMix = 0.3f, .dly = "Ping-Pong", .dlyMix = 0.25f, .lfo = 4 },
        { .req = "pad cinematic", .role = R_PAD, .wa = "Analog Strings", .wb = "Spectral Pad", .wc = "Basic Saw", .ftype = "Low Pass 24", .cut = 1800.0f, .atk = 1.8f, .rel = 3.5f, .rev = "Cathedral", .revMix = 0.5f, .comp = true, .ta = WIDE, .va = 0.7f },
        { .req = "pad choir", .role = R_PAD, .wa = "Additive Choir", .wb = "Vowel O", .wc = "Vowel A", .ftype = "Low Pass 12", .cut = 3800.0f, .atk = 0.9f, .rel = 2.0f, .rev = "Cathedral", .revMix = 0.45f, .lfo = 5 },
        { .req = "pad strings", .role = R_PAD, .wa = "Analog Strings", .wb = "Super Saw Stack", .ftype = "Low Pass 12", .cut = 3400.0f, .atk = 0.5f, .rel = 1.2f, .uni = 6, .rev = "Hall", .revMix = 0.3f, .chorus = 0.5f, .lfo = 5 },
        { .req = "pad glass", .role = R_PAD, .wa = "Glass Harmonic", .wb = "Spectral Pad", .ftype = "Low Pass 12", .cut = 6500.0f, .atk = 1.0f, .rel = 2.5f, .rev = "Shimmer", .revMix = 0.4f, .lfo = 2 },
        { .req = "pad ice", .role = R_PAD, .wa = "Glass Harmonic", .wb = "FM Bell", .ftype = "Low Pass 12", .cut = 8000.0f, .atk = 0.8f, .rel = 2.5f, .rev = "Shimmer", .revMix = 0.5f, .lfo = 2 },
        { .req = "pad vocal", .role = R_PAD, .wa = "Vowel AEIOU", .wb = "Additive Choir", .ftype = "Formant I", .cut = 1400.0f, .res = 0.25f, .atk = 0.8f, .rel = 2.0f, .rev = "Plate", .revMix = 0.4f, .lfo = 2 },
        { .req = "pad drone", .role = R_PAD, .wa = "Basic Sine", .wb = "Analog Pad", .ftype = "Low Pass 24", .cut = 800.0f, .atk = 3.0f, .sus = 1.0f, .rel = 4.0f, .sub = 0.4f, .rev = "Hall", .revMix = 0.4f, .lfo = 1 },
        // ================================================================ leads
        { .req = "lead saw", .role = R_LEAD, .wa = "Basic Saw", .wb = "Super Saw Stack", .ftype = "Ladder LP 12", .cut = 5000.0f, .atk = 0.005f, .sus = 0.85f, .rel = 0.2f, .uni = 3, .det = 0.2f, .mode = 1, .glide = 25.0f, .rev = "Plate", .revMix = 0.2f, .dly = "Stereo", .dlyMix = 0.2f, .lfo = 5 },
        { .req = "lead square", .role = R_LEAD, .wa = "Basic Square", .wb = "Pulse 25", .ftype = "Low Pass 24", .cut = 3500.0f, .atk = 0.005f, .sus = 0.85f, .rel = 0.15f, .uni = 2, .mode = 1, .glide = 20.0f, .dly = "Stereo", .dlyMix = 0.18f, .lfo = 5 },
        { .req = "lead soft", .role = R_LEAD, .wa = "Basic Triangle", .wb = "Soft Saw", .ftype = "MG Low", .cut = 3000.0f, .atk = 0.03f, .sus = 0.9f, .rel = 0.35f, .mode = 1, .glide = 20.0f, .rev = "Plate", .revMix = 0.25f, .lfo = 5, .ta = WARM, .va = 0.5f },
        { .req = "lead soft glide", .role = R_LEAD, .wa = "Basic Triangle", .wb = "Soft Saw", .ftype = "MG Low", .cut = 2800.0f, .atk = 0.04f, .sus = 0.9f, .rel = 0.4f, .mode = 2, .glide = 120.0f, .rev = "Plate", .revMix = 0.3f, .dly = "Tape Echo", .dlyMix = 0.2f, .lfo = 5, .ta = WARM, .va = 0.5f },
        { .req = "lead warm glide", .role = R_LEAD, .wa = "Analog Brass", .wb = "Basic Saw", .ftype = "MG Low", .cut = 2500.0f, .atk = 0.03f, .sus = 0.9f, .rel = 0.35f, .mode = 2, .glide = 110.0f, .rev = "Plate", .revMix = 0.25f, .chorus = 0.3f, .lfo = 5, .ta = WARM, .va = 0.6f },
        { .req = "lead space", .role = R_LEAD, .wa = "Super Saw Stack", .wb = "Glass Harmonic", .ftype = "Ladder LP 12", .cut = 5500.0f, .atk = 0.01f, .rel = 0.6f, .mode = 1, .glide = 40.0f, .rev = "Shimmer", .revMix = 0.4f, .dly = "Ping-Pong", .dlyMix = 0.35f, .lfo = 5, .ta = SPACE, .va = 0.6f },
        { .req = "lead electro", .role = R_LEAD, .wa = "Digital Edge", .wb = "Pulse 12", .warp = "Wavefold", .warpAmt = 0.3f, .ftype = "Diode Ladder", .cut = 4500.0f, .res = 0.35f, .atk = 0.002f, .rel = 0.2f, .mode = 1, .glide = 15.0f, .dly = "Stereo", .dlyMix = 0.2f, .dist = "Overdrive", .drive = 10.0f, .ta = DIGITAL, .va = 0.6f },
        { .req = "lead house", .role = R_LEAD, .wa = "Super Saw Stack", .wb = "Basic Saw", .ftype = "Low Pass 24", .cut = 4000.0f, .atk = 0.005f, .dec = 0.35f, .sus = 0.5f, .rel = 0.25f, .uni = 5, .det = 0.3f, .mode = 0, .rev = "Plate", .revMix = 0.25f, .dly = "Stereo", .dlyMix = 0.25f, .chorus = 0.3f, .comp = true },
        { .req = "lead trance", .role = R_LEAD, .wa = "Super Saw Stack", .wb = "Super Saw Stack", .wc = "Basic Saw", .ftype = "Ladder LP 12", .cut = 6500.0f, .atk = 0.004f, .sus = 0.85f, .rel = 0.3f, .uni = 7, .det = 0.45f, .mode = 0, .rev = "Plate", .revMix = 0.3f, .dly = "Ping-Pong", .dlyMix = 0.3f, .lfo = 4 },
        { .req = "lead delay", .role = R_LEAD, .wa = "Basic Saw", .wb = "Pulse 25", .ftype = "Ladder LP 12", .cut = 4200.0f, .atk = 0.005f, .sus = 0.85f, .rel = 0.2f, .mode = 1, .glide = 25.0f, .dly = "Stereo", .dlyMix = 0.45f, .lfo = 5, .ta = ECHO, .va = 0.8f },
        { .req = "lead dubstep", .role = R_LEAD, .wa = "Harsh Fold", .wb = "Super Saw Stack", .warp = "Wavefold", .warpAmt = 0.5f, .ftype = "Diode Ladder", .cut = 3000.0f, .res = 0.35f, .mode = 1, .dist = "Hard Clip", .drive = 18.0f, .comp = true, .lfo = 3, .ta = AGGR, .va = 0.8f },
        { .req = "lead synthwave", .role = R_LEAD, .wa = "Super Saw Stack", .wb = "Vintage PWM", .ftype = "Ladder LP 24", .cut = 4000.0f, .atk = 0.01f, .rel = 0.4f, .uni = 4, .det = 0.3f, .rev = "Plate", .revMix = 0.3f, .dly = "Tape Echo", .dlyMix = 0.3f, .chorus = 0.5f, .lfo = 5, .ta = ANALOG, .va = 0.6f },
        { .req = "lead pluck", .role = R_LEAD, .wa = "Pulse 12", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 3500.0f, .atk = 0.001f, .dec = 0.4f, .sus = 0.2f, .rel = 0.2f, .mode = 1, .rev = "Plate", .revMix = 0.25f, .dly = "Stereo", .dlyMix = 0.3f },
        { .req = "lead vocal", .role = R_LEAD, .wa = "Vowel Female", .wb = "Additive Choir", .ftype = "Formant II", .cut = 1600.0f, .res = 0.25f, .atk = 0.04f, .sus = 0.9f, .mode = 1, .glide = 50.0f, .rev = "Plate", .revMix = 0.3f, .lfo = 5 },
        // ================================================================ bass
        { .req = "bass techno", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Square", .ftype = "Ladder LP 24", .cut = 700.0f, .res = 0.25f, .atk = 0.002f, .dec = 0.25f, .sus = 0.6f, .rel = 0.12f, .sub = 0.5f, .mode = 1, .glide = 0.0f, .dist = "Tape", .drive = 8.0f, .comp = true, .lfo = 4 },
        { .req = "bass house", .role = R_BASS, .wa = "Basic Saw", .wb = "Pulse 25", .ftype = "Ladder LP 24", .cut = 900.0f, .res = 0.2f, .atk = 0.002f, .dec = 0.3f, .sus = 0.5f, .rel = 0.1f, .sub = 0.4f, .mode = 1, .comp = true, .lfo = 8 },
        { .req = "bass tech house", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Square", .ftype = "Low Pass 24", .cut = 800.0f, .res = 0.22f, .atk = 0.002f, .dec = 0.22f, .sus = 0.55f, .rel = 0.1f, .sub = 0.45f, .mode = 1, .glide = 15.0f, .dist = "Soft Clip", .drive = 8.0f, .comp = true, .lfo = 8 },
        { .req = "bass stab", .role = R_BASS, .wa = "Pulse 25", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 600.0f, .res = 0.3f, .atk = 0.001f, .dec = 0.18f, .sus = 0.0f, .rel = 0.1f, .mode = 1, .comp = true, .ta = PLUCK, .va = 0.6f },
        { .req = "bass saw glide", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 900.0f, .atk = 0.004f, .sus = 0.8f, .rel = 0.15f, .uni = 2, .det = 0.12f, .mode = 2, .glide = 90.0f, .comp = true },
        { .req = "bass organ", .role = R_BASS, .wa = "Additive Organ", .wb = "Basic Sine", .ftype = "Low Pass 12", .cut = 1200.0f, .atk = 0.005f, .sus = 1.0f, .rel = 0.1f, .sub = 0.3f, .mode = 1, .chorus = 0.25f },
        { .req = "bass organ house", .role = R_BASS, .wa = "Additive Organ", .wb = "Odd Harmonics", .ftype = "Low Pass 12", .cut = 1400.0f, .atk = 0.004f, .dec = 0.25f, .sus = 0.5f, .rel = 0.1f, .sub = 0.3f, .mode = 1, .dist = "Tube", .drive = 6.0f, .comp = true, .lfo = 8 },
        { .req = "303", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Square", .ftype = "Ladder LP 24", .cut = 500.0f, .res = 0.7f, .atk = 0.001f, .dec = 0.2f, .sus = 0.15f, .rel = 0.08f, .uni = 1, .sub = 0.0f, .mode = 1, .glide = 60.0f, .dist = "Overdrive", .drive = 10.0f, .lfo = 6, .ta = RESO, .va = 0.9f, .tb = SWEEP, .vb = 0.6f },
        { .req = "acid bass", .role = R_BASS, .wa = "Basic Saw", .ftype = "Ladder LP 24", .cut = 520.0f, .res = 0.72f, .atk = 0.001f, .dec = 0.2f, .sus = 0.12f, .rel = 0.08f, .uni = 1, .mode = 1, .glide = 55.0f, .dist = "Overdrive", .drive = 10.0f, .lfo = 6, .ta = RESO, .va = 0.9f },
        { .req = "303 acid", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Square", .ftype = "Ladder LP 24", .cut = 480.0f, .res = 0.75f, .atk = 0.001f, .dec = 0.18f, .sus = 0.1f, .rel = 0.07f, .uni = 1, .mode = 1, .glide = 60.0f, .dist = "Overdrive", .drive = 12.0f, .lfo = 6, .ta = RESO, .va = 1.0f },
        { .req = "acid 303 distorted", .role = R_BASS, .wa = "Basic Saw", .wb = "Distorted Digital", .ftype = "Ladder LP 24", .cut = 520.0f, .res = 0.78f, .atk = 0.001f, .dec = 0.17f, .sus = 0.1f, .rel = 0.07f, .uni = 1, .mode = 1, .glide = 55.0f, .dist = "Hard Clip", .drive = 24.0f, .comp = true, .lfo = 6, .ta = AGGR, .va = 0.8f },
        { .req = "bass dubstep", .role = R_BASS, .wa = "Super Saw Stack", .wb = "Harsh Fold", .ftype = "Low Pass 24", .cut = 700.0f, .res = 0.4f, .sub = 0.5f, .mode = 1, .dist = "Hard Clip", .drive = 18.0f, .comp = true, .lfo = 3, .ta = AGGR, .va = 0.7f },
        { .req = "bass trap", .role = R_BASS, .wa = "Basic Sine", .wb = "Basic Triangle", .ftype = "Low Pass 12", .cut = 400.0f, .atk = 0.002f, .dec = 1.2f, .sus = 0.5f, .rel = 0.5f, .sub = 0.9f, .mode = 2, .glide = 90.0f, .dist = "Tape", .drive = 6.0f, .comp = true },
        { .req = "bass dnb", .role = R_BASS, .wa = "Basic Saw", .wb = "Digital Edge", .wc = "Basic Saw", .ftype = "Diode Ladder", .cut = 900.0f, .res = 0.35f, .uni = 3, .det = 0.3f, .mode = 1, .dist = "Fold", .drive = 14.0f, .comp = true, .lfo = 3, .ta = AGGR, .va = 0.7f },
        { .req = "bass funk", .role = R_BASS, .wa = "Pulse 12", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 550.0f, .res = 0.4f, .atk = 0.001f, .dec = 0.2f, .sus = 0.2f, .rel = 0.1f, .mode = 1, .ta = PLUCK, .va = 0.5f },
        { .req = "bass reggae", .role = R_BASS, .wa = "Basic Sine", .wb = "Basic Triangle", .ftype = "Low Pass 12", .cut = 500.0f, .atk = 0.003f, .dec = 0.5f, .sus = 0.7f, .rel = 0.3f, .sub = 0.7f, .mode = 1, .dly = "Tape Echo", .dlyMix = 0.2f },
        { .req = "bass sub", .role = R_BASS, .wa = "Basic Sine", .ftype = "Low Pass 12", .cut = 350.0f, .res = 0.05f, .atk = 0.004f, .sus = 1.0f, .rel = 0.2f, .sub = 0.8f, .mode = 1, .comp = true },
        { .req = "bass fm", .role = R_BASS, .wa = "FM Bass", .wb = "FM Growl", .ftype = "Low Pass 24", .cut = 1200.0f, .atk = 0.002f, .sus = 0.7f, .mode = 1, .comp = true, .ta = METAL, .va = 0.4f },
        { .req = "moog bass", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Square", .ftype = "Ladder LP 24", .cut = 700.0f, .res = 0.3f, .atk = 0.003f, .dec = 0.4f, .sus = 0.5f, .rel = 0.15f, .mode = 1, .ta = ANALOG, .va = 0.8f },
        // ================================================================ stabs / keys / plucks
        { .req = "stab house", .role = R_PLUCK, .wa = "Super Saw Stack", .wb = "Analog Brass", .ftype = "Low Pass 24", .cut = 2200.0f, .atk = 0.001f, .dec = 0.3f, .sus = 0.1f, .rel = 0.25f, .uni = 5, .det = 0.3f, .mode = 0, .rev = "Plate", .revMix = 0.25f, .dly = "Stereo", .dlyMix = 0.25f, .chorus = 0.4f, .comp = true },
        { .req = "stab chicago", .role = R_PLUCK, .wa = "Analog Brass", .wb = "Pulse 25", .wc = "Basic Saw", .ftype = "Low Pass 24", .cut = 2600.0f, .atk = 0.001f, .dec = 0.25f, .sus = 0.15f, .rel = 0.2f, .uni = 3, .det = 0.2f, .rev = "Room", .revMix = 0.2f, .dly = "Tape Echo", .dlyMix = 0.2f, .chorus = 0.5f },
        { .req = "stab keys", .role = R_KEYS, .wa = "FM EP", .wb = "Pulse 25", .ftype = "Low Pass 24", .cut = 3500.0f, .atk = 0.001f, .dec = 0.3f, .sus = 0.1f, .rel = 0.2f, .rev = "Plate", .revMix = 0.2f, .chorus = 0.3f },
        { .req = "stab techno", .role = R_PLUCK, .wa = "Basic Saw", .wb = "Comb Tear", .ftype = "Comb +", .cut = 900.0f, .res = 0.5f, .atk = 0.001f, .dec = 0.2f, .sus = 0.0f, .rel = 0.15f, .rev = "Room", .revMix = 0.2f, .dly = "Ping-Pong", .dlyMix = 0.3f, .dist = "Tape", .drive = 8.0f },
        { .req = "keys ep", .role = R_KEYS, .wa = "FM EP", .wb = "Basic Sine", .ftype = "Low Pass 12", .cut = 5000.0f, .res = 0.05f, .atk = 0.002f, .dec = 1.2f, .sus = 0.35f, .rel = 0.5f, .rev = "Plate", .revMix = 0.2f, .chorus = 0.45f },
        { .req = "keys piano", .role = R_KEYS, .wa = "Piano Partials", .wb = "FM EP", .wc = "Basic Triangle", .ftype = "Low Pass 12", .cut = 7000.0f, .atk = 0.002f, .dec = 1.6f, .sus = 0.25f, .rel = 0.6f, .rev = "Room", .revMix = 0.25f },
        { .req = "keys harp", .role = R_KEYS, .wa = "FM Pluck", .wb = "Glass Harmonic", .ftype = "Low Pass 12", .cut = 6000.0f, .atk = 0.001f, .dec = 1.0f, .sus = 0.0f, .rel = 1.2f, .rev = "Hall", .revMix = 0.35f },
        { .req = "harpsichord", .role = R_KEYS, .wa = "Pulse 12", .wb = "Odd Harmonics", .ftype = "Band Pass 12", .cut = 2800.0f, .atk = 0.001f, .dec = 0.6f, .sus = 0.1f, .rel = 0.2f, .rev = "Room", .revMix = 0.15f },
        { .req = "harp", .role = R_PLUCK, .wa = "FM Pluck", .wb = "Basic Triangle", .ftype = "Low Pass 12", .cut = 6500.0f, .atk = 0.001f, .dec = 0.9f, .sus = 0.0f, .rel = 1.0f, .rev = "Hall", .revMix = 0.35f },
        { .req = "keys house", .role = R_KEYS, .wa = "Additive Organ", .wb = "Pulse 25", .ftype = "Low Pass 24", .cut = 3000.0f, .atk = 0.003f, .dec = 0.4f, .sus = 0.4f, .rel = 0.2f, .chorus = 0.4f, .comp = true },
        { .req = "organ house", .role = R_KEYS, .wa = "Additive Organ", .wb = "Odd Harmonics", .ftype = "Low Pass 12", .cut = 4500.0f, .atk = 0.003f, .dec = 0.3f, .sus = 0.7f, .rel = 0.12f, .chorus = 0.4f, .comp = true, .lfo = 5 },
        { .req = "piano house", .role = R_KEYS, .wa = "Piano Partials", .wb = "FM EP", .ftype = "Low Pass 12", .cut = 6000.0f, .atk = 0.002f, .dec = 0.5f, .sus = 0.1f, .rel = 0.3f, .rev = "Plate", .revMix = 0.25f, .comp = true },
        { .req = "piano ballad", .role = R_KEYS, .wa = "Piano Partials", .wb = "Basic Triangle", .ftype = "Low Pass 12", .cut = 4500.0f, .atk = 0.003f, .dec = 2.0f, .sus = 0.3f, .rel = 1.0f, .rev = "Hall", .revMix = 0.4f, .ta = WARM, .va = 0.5f },
        { .req = "strings cinematic", .role = R_PAD, .wa = "Analog Strings", .wb = "Super Saw Stack", .ftype = "Low Pass 12", .cut = 3200.0f, .atk = 0.9f, .rel = 2.5f, .uni = 6, .rev = "Cathedral", .revMix = 0.45f, .comp = true, .lfo = 5 },
        { .req = "brass stab", .role = R_PLUCK, .wa = "Analog Brass", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 2500.0f, .atk = 0.01f, .dec = 0.3f, .sus = 0.2f, .rel = 0.2f, .uni = 3, .rev = "Room", .revMix = 0.2f },
        { .req = "pluck house", .role = R_PLUCK, .wa = "Pulse 25", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 2500.0f, .atk = 0.001f, .dec = 0.25f, .sus = 0.0f, .rel = 0.15f, .rev = "Plate", .revMix = 0.2f, .dly = "Stereo", .dlyMix = 0.3f },
        { .req = "pluck techno", .role = R_PLUCK, .wa = "Basic Saw", .wb = "FM Metallic", .ftype = "Band Pass 12", .cut = 1800.0f, .res = 0.35f, .atk = 0.001f, .dec = 0.18f, .sus = 0.0f, .rel = 0.1f, .rev = "Room", .revMix = 0.2f, .dly = "Ping-Pong", .dlyMix = 0.3f },
        // ================================================================ generic synth phrases / arps / fx
        { .req = "synth space reverb delay", .role = R_LEAD, .wa = "Super Saw Stack", .wb = "Glass Harmonic", .wc = "Analog Pad", .ftype = "Ladder LP 12", .cut = 4500.0f, .atk = 0.02f, .sus = 0.8f, .rel = 1.2f, .mode = 0, .rev = "Shimmer", .revMix = 0.5f, .dly = "Ping-Pong", .dlyMix = 0.4f, .lfo = 2, .ta = SPACE, .va = 0.8f, .tb = ECHO, .vb = 0.8f },
        { .req = "synth flange", .role = R_LEAD, .wa = "Basic Saw", .wb = "Pulse 25", .ftype = "Ladder LP 24", .cut = 3200.0f, .atk = 0.01f, .sus = 0.8f, .rel = 0.4f, .uni = 3, .det = 0.25f, .mode = 0, .ta = FLANGER, .va = 1.2f },
        { .req = "synth bass", .role = R_BASS, .wa = "Basic Saw", .wb = "Basic Square", .ftype = "Ladder LP 24", .cut = 800.0f, .res = 0.25f, .atk = 0.003f, .sus = 0.7f, .mode = 1, .comp = true },
        { .req = "synth lead", .role = R_LEAD, .wa = "Super Saw Stack", .wb = "Basic Saw", .ftype = "Ladder LP 12", .cut = 5000.0f, .atk = 0.005f, .sus = 0.85f, .mode = 1, .glide = 25.0f, .dly = "Stereo", .dlyMix = 0.25f, .lfo = 5 },
        { .req = "synth pad", .role = R_PAD, .wa = "Analog Pad", .wb = "Super Saw Stack", .ftype = "Low Pass 24", .cut = 2400.0f, .atk = 0.8f, .rel = 1.8f, .rev = "Hall", .revMix = 0.3f, .chorus = 0.5f },
        { .req = "synth pluck", .role = R_PLUCK, .wa = "Pulse 12", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 2500.0f, .atk = 0.001f, .dec = 0.3f, .sus = 0.0f, .rel = 0.2f, .dly = "Stereo", .dlyMix = 0.25f },
        { .req = "synth arp", .role = R_ARP, .wa = "Basic Saw", .wb = "Pulse 25", .ftype = "Ladder LP 24", .cut = 2800.0f, .atk = 0.001f, .dec = 0.25f, .sus = 0.3f, .rel = 0.2f, .dly = "Ping-Pong", .dlyMix = 0.3f, .arp = true },
        { .req = "arp trance", .role = R_ARP, .wa = "Super Saw Stack", .wb = "Basic Saw", .ftype = "Low Pass 24", .cut = 3800.0f, .atk = 0.002f, .dec = 0.2f, .sus = 0.3f, .rel = 0.2f, .uni = 5, .det = 0.35f, .rev = "Plate", .revMix = 0.25f, .dly = "Ping-Pong", .dlyMix = 0.3f, .arp = true },
        { .req = "arp house", .role = R_ARP, .wa = "Pulse 25", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 2800.0f, .atk = 0.001f, .dec = 0.22f, .sus = 0.2f, .rel = 0.15f, .dly = "Stereo", .dlyMix = 0.3f, .chorus = 0.3f, .comp = true, .arp = true },
        { .req = "fx riser", .role = R_FX, .wa = "Harmonic Sweep", .wb = "Spectral Shred", .ftype = "High Pass 12", .cut = 2500.0f, .atk = 3.5f, .sus = 1.0f, .rel = 1.5f, .noise = "White", .noiseLvl = 0.5f, .rev = "Cathedral", .revMix = 0.4f, .ta = SWEEP, .va = 1.0f },
        { .req = "fx impact", .role = R_FX, .wa = "Basic Sine", .wb = "Noise Burst", .ftype = "Low Pass 24", .cut = 600.0f, .atk = 0.001f, .dec = 1.5f, .sus = 0.0f, .rel = 1.5f, .sub = 0.9f, .noise = "Brown", .noiseLvl = 0.4f, .rev = "Cathedral", .revMix = 0.5f, .comp = true },
        { .req = "fx sweep", .role = R_FX, .wa = "Harmonic Sweep", .wb = "Basic Saw", .ftype = "Ladder LP 24", .cut = 600.0f, .res = 0.5f, .atk = 2.5f, .sus = 1.0f, .rel = 1.5f, .rev = "Hall", .revMix = 0.4f, .ta = SWEEP, .va = 1.0f },
        { .req = "fx laser", .role = R_FX, .wa = "FM Sweep", .wb = "Basic Saw", .ftype = "Low Pass 12", .cut = 8000.0f, .atk = 0.001f, .dec = 0.4f, .sus = 0.0f, .rel = 0.3f, .rev = "Plate", .revMix = 0.25f, .dly = "Stereo", .dlyMix = 0.3f },
        { .req = "fx noise", .role = R_FX, .wa = "Noise Burst", .ftype = "Band Pass 12", .cut = 3000.0f, .atk = 0.8f, .sus = 1.0f, .rel = 1.5f, .noise = "White", .noiseLvl = 0.7f, .lfo = 1 },
        { .req = "fx wind", .role = R_FX, .wa = "Noise Burst", .ftype = "Band Pass 12", .cut = 1100.0f, .atk = 1.8f, .sus = 1.0f, .rel = 3.0f, .noise = "Pink", .noiseLvl = 0.7f, .rev = "Hall", .revMix = 0.4f, .lfo = 1 },
        { .req = "fx ocean", .role = R_FX, .wa = "Pad Evolve", .wb = "Noise Burst", .ftype = "Low Pass 24", .cut = 1000.0f, .atk = 2.5f, .sus = 1.0f, .rel = 4.0f, .noise = "Brown", .noiseLvl = 0.5f, .rev = "Hall", .revMix = 0.5f, .lfo = 1 },
        { .req = "fx night", .role = R_FX, .wa = "Spectral Pad", .wb = "Spectral Freeze", .ftype = "Low Pass 12", .cut = 1400.0f, .atk = 2.0f, .sus = 1.0f, .rel = 3.5f, .noise = "Pink", .noiseLvl = 0.15f, .rev = "Cathedral", .revMix = 0.5f, .lfo = 7, .ta = DARK, .va = 0.5f },
        { .req = "fx space", .role = R_FX, .wa = "FM Sweep", .wb = "Spectral Pad", .ftype = "Band Pass 12", .cut = 2200.0f, .atk = 1.5f, .sus = 1.0f, .rel = 3.0f, .rev = "Shimmer", .revMix = 0.5f, .dly = "Reverse", .dlyMix = 0.3f, .lfo = 2 },
        { .req = "fx glitch", .role = R_FX, .wa = "Bit Crush", .wb = "Harsh Fold", .ftype = "High Pass 12", .cut = 1500.0f, .atk = 0.001f, .dec = 0.12f, .sus = 0.0f, .rel = 0.1f, .dist = "Downsample", .drive = 12.0f, .lfo = 4 },
    };
    n = (int) (sizeof (r) / sizeof (r[0]));
    return r;
}

inline Design designSound (const juce::String& prompt, juce::Random& rng)
{
    using namespace sd;
    auto rnd = [&rng] (float lo, float hi) { return lo + (hi - lo) * rng.nextFloat(); };
    auto clamp01 = [] (float v) { return std::min (1.0f, std::max (0.0f, v)); };

    // ------------------------------------------------------------ 1) prompt -> trait vector
    std::array<float, NT> t {};
    juce::String clean;
    for (auto c : prompt.toLowerCase()) clean += (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : (c == '\'') ? juce::String() : juce::String (" "));
    juce::StringArray tokens;
    tokens.addTokens (clean, " ", "");
    tokens.removeEmptyStrings();
    // hyphenated words were split by the cleaner; also try them glued (hi-hat -> hihat, lo-fi -> lofi)
    {
        const auto glued = prompt.toLowerCase().removeCharacters ("-'");
        juce::StringArray extra; extra.addTokens (glued, " ,;/", "");
        for (auto& e : extra) if (e.isNotEmpty() && ! tokens.contains (e)) tokens.add (e);
    }

    static const char* strong[] = { "very", "extremely", "super", "ultra", "so", "really", "hugely", "insanely", "massively", "crazy", "totally" };
    static const char* weak[]   = { "slightly", "bit", "little", "subtle", "subtly", "touch", "mildly", "kinda", "somewhat", "hint" };
    static const char* negate[] = { "not", "no", "without", "non", "less", "never", "zero" };
    auto isIn = [] (const juce::String& s, const char* const* list, int n) { for (int i = 0; i < n; ++i) if (s == list[i]) return true; return false; };

    int nGroups = 0, nArch = 0;
    const Group* groups = lexicon (nGroups);
    const Arch* archs = archetypes (nArch);
    const Arch* arch = nullptr;
    float mult = 1.0f;
    int matched = 0;
    juce::StringArray understood;
    for (const auto& tok : tokens)
    {
        if (isIn (tok, strong, 11)) { mult = 1.7f; continue; }
        if (isIn (tok, weak, 10))   { mult = 0.5f; continue; }
        if (isIn (tok, negate, 7))  { mult = -1.0f; continue; }
        if (tok == "more") { mult = 1.3f; continue; }

        auto matches = [&tok] (const juce::String& w)
        { return tok == w || (tok.length() >= 4 && w.startsWith (tok)) || (w.length() >= 4 && tok.startsWith (w)); };

        bool hit = false;
        for (int gi = 0; gi < nGroups; ++gi)
        {
            juce::StringArray words;
            words.addTokens (groups[gi].words, " ", "");
            for (const auto& w : words)
            {
                if (! matches (w)) continue;
                for (int k = 0; k < 4; ++k)
                    if (groups[gi].t[k] >= 0) t[(size_t) groups[gi].t[k]] += groups[gi].v[k] * mult;
                hit = true;
                break;
            }
        }
        for (int ai = 0; ai < nArch; ++ai)
        {
            juce::StringArray words;
            words.addTokens (archs[ai].words, " ", "");
            for (const auto& w : words)
                if (matches (w)) { arch = &archs[ai]; hit = true; break; }
        }
        if (hit) { ++matched; understood.addIfNotAlreadyThere (tok); mult = 1.0f; }
    }

    // compound-phrase recipes ("pad warm night", "bass tech house", "303 acid" ...): the one matching the most words wins
    const Recipe* recipe = nullptr;
    {
        int nRec = 0, best = 0;
        const Recipe* recs = recipes (nRec);
        auto tokMatch = [] (const juce::String& tok, const juce::String& w)
        { return tok == w || (tok.length() >= 4 && w.startsWith (tok)) || (w.length() >= 4 && tok.startsWith (w)); };
        for (int ri = 0; ri < nRec; ++ri)
        {
            juce::StringArray rw;
            rw.addTokens (recs[ri].req, " ", "");
            int hits = 0;
            for (const auto& w : rw)
                for (const auto& tok : tokens) if (tokMatch (tok, w)) { ++hits; break; }
            if (hits == rw.size() && hits > best) { best = hits; recipe = &recs[ri]; }
        }
        if (recipe != nullptr)
        {
            ++matched;
            understood.addIfNotAlreadyThere (juce::String ("[") + recipe->req + "]");
            if (recipe->ta >= 0) t[(size_t) recipe->ta] += recipe->va;
            if (recipe->tb >= 0) t[(size_t) recipe->tb] += recipe->vb;
            if (recipe->tc >= 0) t[(size_t) recipe->tc] += recipe->vc;
        }
    }

    if (matched == 0)
    {   // unknown words still shape the sound: hash them into a repeatable colouring
        const auto h = (juce::uint32) prompt.hashCode();
        t[BRIGHT] += (float) (h % 7) / 10.0f; t[MOTION] += (float) ((h >> 3) % 6) / 10.0f; t[WIDE] += (float) ((h >> 6) % 7) / 10.0f;
        t[R_PAD + (int) ((h >> 9) % 5)] += 1.0f;
    }
    for (auto& v : t) v = std::min (2.0f, v);

    // ------------------------------------------------------------ 2) role
    int role = R_PAD;
    {
        float best = 0.0f;
        for (int r = R_BASS; r <= R_PERC; ++r) if (t[(size_t) r] > best) { best = t[(size_t) r]; role = r; }
        if (arch != nullptr && t[R_PERC] <= 0.0f && best < 1.0f) role = arch->role;
        else if (best <= 0.0f)
        {
            if (t[LOW] > 0.4f || t[SUB] > 0.4f) role = R_BASS;
            else if (t[SPACE] + t[SLOW] > 0.6f) role = R_PAD;
            else if (t[PLUCK] + t[BRIEF] > 0.6f) role = R_PLUCK;
            else if (t[AGGR] + t[BRIGHT] > 0.8f) role = R_LEAD;
            else { static const int pool[5] = { R_PAD, R_LEAD, R_PLUCK, R_KEYS, R_BASS }; role = pool[rng.nextInt (5)]; }
        }
    }
    if (recipe != nullptr && recipe->role >= 0) role = recipe->role;
    const bool isBass = role == R_BASS, isKeys = role == R_KEYS, isPluck = role == R_PLUCK, isPad = role == R_PAD, isLead = role == R_LEAD, isFx = role == R_FX, isArp = role == R_ARP, isPerc = role == R_PERC;
    const bool wantArp = t[R_ARP] > 0.3f || isArp || (recipe != nullptr && recipe->arp);

    const float bright = std::max (-1.0f, std::min (1.0f, t[BRIGHT] - t[DARK] - 0.3f * t[WARM] + (isBass ? -0.4f : (isLead ? 0.3f : (isPluck ? 0.1f : 0.0f)))));
    const float wide = clamp01 (t[WIDE] + (isPad ? 0.45f : 0.0f) + (isLead ? 0.15f : 0.0f) + 0.3f * t[CHORUS] + 0.3f * t[DETUNE]);
    const float motion = clamp01 (t[MOTION] + (isPad || isFx ? 0.35f : 0.0f) + 0.2f * t[PHASER]);
    const float aggr = clamp01 (t[AGGR] + 0.4f * t[GRIT] + 0.4f * t[FUZZ] + 0.3f * t[FOLD]);
    const float space = clamp01 (t[SPACE] + (isPad ? 0.35f : (isFx ? 0.4f : (isPluck || isKeys ? 0.15f : 0.1f))));
    const float echo = clamp01 (t[ECHO] + (isLead || isArp ? 0.25f : (isPluck ? 0.15f : 0.0f)));

    juce::StringArray notes;
    static const char* roleNames[8] = { "bass", "keys", "pluck", "pad", "lead", "fx", "arp", "drum" };
    notes.add (juce::String ("role: ") + roleNames[role - R_BASS]);
    if (understood.size() > 0) notes.add ("heard: " + understood.joinIntoString (" "));

    Patch p;
    const auto names = wavetableNames();
    auto tableIndex = [&names] (const char* n) { for (int i = 0; i < names.size(); ++i) if (names[i] == n) return i; return 0; };
    const bool deep = juce::String (prompt).toLowerCase().contains ("open");

    // ================================================================= DRUMS: noise + pitch envelopes
    if (isPerc)
    {
        const bool kick = t[KICK] > 0.3f, snare = t[SNARE] > 0.3f, hat = t[HAT] > 0.3f, clap = t[CLAP] > 0.3f, tom = t[TOM] > 0.3f, cym = t[CYMBAL] > 0.3f;
        if (kick || (! snare && ! hat && ! clap && ! tom && ! cym))
        {
            p.osc (0, "Basic Sine", 0.95f, -12, 0.0f, 1, 0.1f, 0.3f, 0.7f, 0.0f);
            p.env (0, 0.001f, rnd (0.18f, 0.35f) * (1.0f + t[LENGTHY]), 0.0f, 0.15f);
            p.env (1, 0.001f, rnd (0.04f, 0.1f), 0.0f, 0.05f);
            p.mod ("Env 2", "Osc A Pitch", rnd (0.36f, 0.5f), false);          // fast downward pitch sweep = kick thump
            p.noise ("White", 0.1f + 0.15f * aggr, 6000.0f, true, true);
            p.filter (0, "Low Pass 24", rnd (2200.0f, 5000.0f), 0.1f, 0.2f + 0.3f * aggr);
            p.routeAll ("Filter 1");
            notes.add ("synthesised kick");
        }
        else if (snare)
        {
            p.osc (0, "Basic Triangle", 0.55f, 0, 0.0f, 1, 0.1f, 0.3f, 0.7f, 0.0f);
            p.env (0, 0.001f, rnd (0.12f, 0.22f), 0.0f, 0.12f);
            p.env (1, 0.001f, 0.05f, 0.0f, 0.05f);
            p.mod ("Env 2", "Osc A Pitch", 0.14f, false);
            p.noise ("White", 0.8f, 9000.0f, true, true);
            p.filter (0, "Band Pass 12", rnd (2200.0f, 3600.0f), 0.22f, 0.15f);
            p.routeAll ("Filter 1");
            notes.add ("synthesised snare");
        }
        else if (hat)
        {
            p.oscOff (0);
            p.env (0, 0.001f, deep ? rnd (0.3f, 0.5f) : rnd (0.03f, 0.07f), 0.0f, deep ? 0.3f : 0.04f);
            p.noise ("White", 0.9f, 14000.0f, true, true);
            p.filter (0, "High Pass 12", rnd (6500.0f, 9500.0f), 0.15f, 0.1f);
            p.routeAll ("Filter 1");
            p.osc (1, "FM Metallic", 0.18f, 12, 0.0f, 1, 0.1f, 0.3f, 0.5f, 0.0f);
            notes.add (deep ? "open hi-hat" : "closed hi-hat");
        }
        else if (clap)
        {
            p.oscOff (0);
            p.env (0, 0.001f, rnd (0.12f, 0.2f), 0.0f, 0.1f);
            p.noise ("Pink", 0.95f, 8000.0f, true, true);
            p.filter (0, "Band Pass 12", rnd (1300.0f, 2000.0f), 0.3f, 0.1f);
            p.routeAll ("Filter 1");
            LfoCustom c; c.n = 0;
            const float xs[8] = { 0.0f, 0.05f, 0.06f, 0.12f, 0.13f, 0.2f, 0.21f, 1.0f };
            const float ys[8] = { 1.0f, 1.0f, 0.2f, 0.2f, 1.0f, 1.0f, 0.2f, 0.2f };
            for (int i = 0; i < 8; ++i) { c.x[i] = xs[i]; c.y[i] = ys[i]; c.n = 8; }
            p.lfo (0, "Custom", 40.0f, 1.0f); p.lfoCustom (0, c);
            p.mod ("LFO 1", "Filter 1 Cutoff", 0.25f, true);
            notes.add ("clap with flam LFO");
        }
        else if (tom)
        {
            p.osc (0, "Basic Sine", 0.9f, -5, 0.0f, 1, 0.1f, 0.3f, 0.7f, 0.0f);
            p.env (0, 0.001f, rnd (0.25f, 0.45f), 0.0f, 0.2f);
            p.env (1, 0.001f, 0.12f, 0.0f, 0.1f);
            p.mod ("Env 2", "Osc A Pitch", 0.22f, false);
            p.noise ("White", 0.08f, 5000.0f, true, true);
            p.filter (0, "Low Pass 24", 3000.0f, 0.1f, 0.1f);
            p.routeAll ("Filter 1");
            notes.add ("tom");
        }
        else
        {
            p.osc (0, "Metal Scrape", 0.4f, 12, 0.0f, 1, 0.1f, 0.5f, 0.6f, 0.0f);
            p.osc (1, "FM Metallic", 0.4f, 7, 0.0f, 1, 0.1f, 0.5f, 0.6f, 0.0f);
            p.env (0, 0.001f, rnd (0.8f, 1.6f), 0.0f, rnd (1.0f, 2.0f));
            p.noise ("White", 0.5f, 12000.0f, true, true);
            p.filter (0, "High Pass 12", 5500.0f, 0.2f, 0.1f);
            p.routeAll ("Filter 1");
            notes.add ("cymbal / crash");
        }
        p.mod ("Velocity", "Filter 1 Cutoff", 0.15f, false);
        p.mod ("Macro 1", "Filter 1 Cutoff", 0.5f, false);
        p.mod ("Macro 2", "Osc A Pitch", 0.08f, true);
        p.play ("Poly", 0.0f);
        p.fx (6, 1.0f, { { "Low Cut", kick ? 20.0f : 120.0f } });
        p.fx (9, 1.0f, { { "Threshold", -16.0f }, { "Ratio", 4.0f }, { "Attack", 3.0f }, { "Release", 90.0f }, { "Gain", 2.0f } });
        const bool tail = t[SPACE] > 0.2f || t[PLATE] > 0.2f || t[ROOM] > 0.2f || snare || clap;
        if (tail)
        {
            p.fx (0, 0.12f + 0.3f * space, { { "Size", t[ROOM] > 0.2f ? 0.4f : 0.5f }, { "Damping", 0.5f }, { "High Cut", 11000.0f } });
            p.fxChoice (0, "Type", t[ROOM] > 0.2f ? "Room" : "Plate");
        }
        if (aggr > 0.3f) { p.fx (5, 1.0f, { { "Drive", 10.0f + 15.0f * aggr }, { "Output", -5.0f } }); p.fxChoice (5, "Type", aggr > 0.6f ? "Hard Clip" : "Tape"); }
        p.fx (8, 1.0f, { { "Threshold", -1.0f } });
        p.standardChain();
        Design d; d.patch = std::move (p); d.summary = notes.joinIntoString (" | ");
        return d;
    }

    // ================================================================= tonal sounds
    // ------------------------------------------------------------ 3) oscillators
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
    int wtA = ranked[0].idx;
    int wtB = ranked[1 + rng.nextInt (3)].idx;
    if (wtB == wtA) wtB = ranked[4].idx;
    int wtC = ranked[2 + rng.nextInt (4)].idx;
    if (arch != nullptr)
    {   // instrument knowledge overrides the affinity choice
        wtA = tableIndex (arch->a);
        if (arch->b != nullptr) wtB = tableIndex (arch->b);
        if (arch->c != nullptr) wtC = tableIndex (arch->c);
        notes.add ("instrument model: " + names[wtA]);
    }

    if (recipe != nullptr)
    {
        if (recipe->wa != nullptr) wtA = tableIndex (recipe->wa);
        if (recipe->wb != nullptr) wtB = tableIndex (recipe->wb);
        if (recipe->wc != nullptr) wtC = tableIndex (recipe->wc);
        notes.add ("recipe: " + juce::String (recipe->req));
    }

    const int octShift = (t[HIGH] > 0.5f ? 12 : 0) + (isBass ? -12 : 0) + (t[LOW] > 0.7f && ! isBass ? -12 : 0);
    int uni = std::max (1, std::min (9, 1 + (int) std::lround (wide * 6.0f + t[DETUNE] * 2.0f - (isBass ? 2.0f : 0.0f) - (t[CLEAN] > 0.4f ? 2.0f : 0.0f))));
    float det = 0.12f + 0.45f * wide + rnd (0.0f, 0.12f);
    if (recipe != nullptr && recipe->uni >= 0) uni = recipe->uni;
    if (recipe != nullptr && recipe->det >= 0.0f) det = recipe->det;
    const float spreadAmt = 0.35f + 0.6f * wide;

    p.osc (0, names[wtA].toRawUTF8(), 0.8f, octShift, rnd (-3.0f, 3.0f), uni, det, spreadAmt, rnd (0.25f, 0.9f), 0.0f);
    p.osc (1, names[wtB].toRawUTF8(), isBass ? 0.35f : rnd (0.3f, 0.55f), octShift + (isBass ? 0 : (rng.nextBool() ? 12 : 0)), rnd (-9.0f, 9.0f),
           std::max (1, uni / 2), det, spreadAmt, rnd (0.1f, 0.9f), 0.0f);
    if (wide > 0.55f || isPad || t[METAL] > 0.5f || t[VOCAL] > 0.5f || (arch != nullptr && arch->c != nullptr))
        p.osc (2, names[wtC].toRawUTF8(), rnd (0.2f, 0.4f), octShift + (isPad && rng.nextBool() ? 12 : (isBass ? 0 : -12 * (int) rng.nextInt (2))), rnd (-9.0f, 9.0f), std::max (1, uni / 2), det, spreadAmt, rnd (0.1f, 0.9f), 0.0f);
    notes.add ("waves: " + names[wtA] + " / " + names[wtB]);

    if (recipe != nullptr && recipe->warp != nullptr) { p.warp (0, recipe->warp, recipe->warpAmt); notes.add (juce::String ("warp: ") + recipe->warp); }
    else if (t[METAL] > 0.5f)                  { p.warp (0, rng.nextBool() ? "FM" : "RM", rnd (0.2f, 0.55f)); p.n (ids::osc (0, "warpSrc"), 1.0f); notes.add ("FM warp"); }
    else if (aggr > 0.55f || t[FOLD] > 0.4f)   { const char* m[3] = { "Wavefold", "Asym +/-", "Quantize" }; p.warp (0, m[rng.nextInt (3)], rnd (0.25f, 0.65f) * (0.5f + aggr)); notes.add ("warp drive"); }
    else if (t[LOFI] > 0.5f || t[DIGITAL] > 0.6f) { p.warp (0, "Quantize", rnd (0.3f, 0.7f)); }
    else if (motion > 0.45f)                   { const char* m[4] = { "Bend +/-", "PWM", "Mirror", "Remap" }; p.warp (0, m[rng.nextInt (4)], rnd (0.15f, 0.45f)); }
    else if (wide > 0.7f && rng.nextBool())    { p.warp (1, "AM", rnd (0.1f, 0.35f)); p.n (ids::osc (1, "warpSrc"), 0.0f); }

    const float subLvl = clamp01 (t[SUB] * 0.7f + (isBass ? 0.55f : 0.0f) + (t[LOW] > 0.5f ? 0.2f : 0.0f));
    if (subLvl > 0.05f) { p.sub (isBass && aggr > 0.5f ? "SAW" : "SIN", subLvl, -1, 0.0f); if (isBass) p.route ("sub", "Bypass"); }
    const float noiseAmt = clamp01 (t[NOISE] * 0.45f + (isFx ? 0.25f : 0.0f) + t[TAPE] * 0.2f + (arch != nullptr && (arch->flags & 2) ? 0.12f : 0.0f));
    if (noiseAmt > 0.03f)
    {
        const char* nt = t[TAPE] > 0.4f ? "Analog Tape" : (t[METAL] + t[GRIT] > 1.0f ? "Circuit Noise" : (t[LOW] > 0.5f ? "Brown" : "Pink"));
        p.noise (nt, noiseAmt, t[BRIGHT] > 0.4f ? 9000.0f : 5000.0f, true, t[LOW] < 0.3f, (int) std::lround (rnd (-5.0f, 5.0f)), rnd (-0.4f, 0.4f));
        p.route ("noise", "Filter 1");
        notes.add ("noise");
    }

    if (recipe != nullptr)
    {
        if (recipe->sub >= 0.0f) p.sub ("SIN", recipe->sub, -1, 0.0f);
        if (recipe->noise != nullptr) { p.noise (recipe->noise, recipe->noiseLvl, 8000.0f, true, false, 0, rnd (-0.3f, 0.3f)); p.route ("noise", "Filter 1"); }
    }

    // ------------------------------------------------------------ 4) filters (1 or 2, with routing)
    const float cutoff = 220.0f * std::pow (2.0f, 6.2f * (0.5f + 0.5f * bright)) * rnd (0.9f, 1.12f);
    const char* ftype = "Ladder LP 24";
    const bool formantLike = t[VOCAL] > 0.45f || (arch != nullptr && (arch->flags & 4));
    if (formantLike)                   ftype = rng.nextBool() ? "Formant I" : "Formant II";
    else if (t[METAL] > 0.55f)         ftype = rng.nextBool() ? "Comb +" : "Comb -";
    else if (t[PHASER] > 0.5f)         ftype = "Phaser 8";
    else if (aggr > 0.85f)             ftype = rng.nextBool() ? "Dist Comb" : "Diode Ladder";
    else if (aggr > 0.5f)              ftype = "Diode Ladder";
    else if (t[HOLLOW] > 0.4f)         ftype = "Notch 12";
    else if (t[WARM] > 0.5f || t[DARK] > 0.5f) ftype = rng.nextBool() ? "MG Low" : "Low Pass 24";
    else if (bright > 0.4f)            ftype = rng.nextBool() ? "Low Pass 12" : "SVF Low Pass";
    const float fcut = formantLike ? rnd (700.0f, 2200.0f) : std::min (19000.0f, cutoff);
    const float res = clamp01 (0.08f + 0.5f * t[RESO] + 0.15f * aggr + rnd (0.0f, 0.08f));
    p.filter (0, ftype, fcut, res, aggr * 0.45f);
    p.routeAll ("Filter 1");
    if (isBass) p.route ("sub", "Bypass");
    if (isLead || isPluck || isKeys || wantArp) p.n (ids::filter (0, "keytrack"), 1.0f);
    if (recipe != nullptr && (recipe->ftype != nullptr || recipe->cut > 0.0f || recipe->res >= 0.0f))
    {
        const char* rt = recipe->ftype != nullptr ? recipe->ftype : ftype;
        p.filter (0, rt, recipe->cut > 0.0f ? recipe->cut * rnd (0.94f, 1.06f) : fcut, recipe->res >= 0.0f ? recipe->res : res, aggr * 0.45f);
        if (recipe->ftype != nullptr) ftype = recipe->ftype;
    }
    notes.add (juce::String ("filter: ") + ftype);

    const bool dual = rng.nextFloat() < (0.2f + 0.4f * std::max (aggr, motion)) || t[VOCAL] > 0.5f || t[METAL] > 0.5f;
    if (dual)
    {
        static const char* f2[5] = { "Band Pass 12", "Peak", "High Pass 12", "Notch 12", "Formant II" };
        const char* type2 = f2[rng.nextInt (5)];
        p.filter (1, type2, rnd (500.0f, 6000.0f), clamp01 (res * 0.8f + 0.1f), aggr * 0.2f, rnd (0.6f, 1.0f), true);
        switch (rng.nextInt (4))
        {
            case 0:  p.route ("oscB", "Filter 2"); notes.add ("osc B -> filter 2"); break;
            case 1:  p.route ("noise", "Filter 2"); p.route ("oscC", "Parallel (1+2)"); notes.add ("parallel filters"); break;
            case 2:  p.route ("oscA", "Series (1 > 2)"); notes.add (juce::String ("series ") + ftype + " > " + type2); break;
            default: p.route ("oscC", "Filter 2"); p.route ("oscB", "Parallel (1+2)"); notes.add ("dual filter"); break;
        }
        p.mod ("Env 2", "Filter 2 Cutoff", rnd (0.1f, 0.3f), false);
    }

    // ------------------------------------------------------------ 5) envelopes
    float atkAxis = std::max (-1.0f, std::min (1.0f, t[SLOW] - t[FAST] + (isPad ? 0.45f : (isPluck ? -0.9f : (isBass ? -0.55f : (isKeys ? -0.6f : (isFx ? 0.2f : -0.2f)))))));
    float relAxis = std::max (-1.0f, std::min (1.0f, t[LENGTHY] - t[BRIEF] + (isPad ? 0.55f : (isPluck ? -0.2f : (isBass ? -0.3f : (isKeys ? 0.1f : 0.0f))))));
    float aTime = 0.0015f * std::pow (2000.0f, 0.5f * (atkAxis + 1.0f));
    float rTime = 0.06f * std::pow (80.0f, 0.5f * (relAxis + 1.0f));
    float sustain = isPluck ? rnd (0.0f, 0.12f) : std::max (0.0f, std::min (1.0f, 0.65f + 0.3f * t[LENGTHY] - 0.5f * t[BRIEF] + (isPad ? 0.15f : 0.0f)));
    float decay = isPluck ? rnd (0.15f, 0.45f) * (1.0f + 0.8f * t[LENGTHY] - 0.5f * t[BRIEF]) : rnd (0.35f, 1.1f);
    if (isKeys) { sustain = std::min (sustain, 0.45f); decay = rnd (0.7f, 1.8f); }
    if (isFx && t[SWEEP] > 0.3f) sustain = 1.0f;
    if (arch != nullptr)
    {
        if (arch->atk > -8.0f && t[SLOW] + t[FAST] < 0.3f) aTime = arch->atk;
        if (arch->rel > -8.0f && t[LENGTHY] + t[BRIEF] < 0.3f) rTime = arch->rel;
        if (arch->sus > -8.0f) sustain = arch->sus;
        if (arch->sus <= 0.01f) decay = std::max (decay, arch->rel);
    }
    p.env (0, std::min (aTime, 6.0f), std::max (0.05f, decay), sustain, std::min (rTime, 8.0f));
    if (recipe != nullptr)
    {
        const float ea = recipe->atk >= 0.0f ? recipe->atk : std::min (aTime, 6.0f), ed = recipe->dec >= 0.0f ? recipe->dec : std::max (0.05f, decay);
        const float es = recipe->sus >= 0.0f ? recipe->sus : sustain, er = recipe->rel >= 0.0f ? recipe->rel : std::min (rTime, 8.0f);
        p.env (0, ea, ed, es, er);
        sustain = es;
    }

    const float envAmt = clamp01 (0.12f + 0.45f * t[PLUCK] + 0.4f * t[SWEEP] + 0.25f * aggr + (isPluck || isKeys ? 0.15f : 0.0f));
    p.env (1, t[SWEEP] > 0.3f ? rnd (1.5f, 3.5f) : 0.002f, std::max (0.12f, decay * 0.8f), t[SWEEP] > 0.3f ? 0.6f : 0.12f, std::min (rTime, 3.0f));
    p.mod ("Env 2", "Filter 1 Cutoff", envAmt, false);
    p.mod ("Velocity", "Filter 1 Cutoff", rnd (0.08f, 0.2f), false);
    p.mod ("Velocity", "Osc A Level", rnd (0.05f, 0.15f), false);
    if (motion > 0.4f || isPad)
    {
        p.env (2, rnd (0.8f, 3.0f), rnd (1.0f, 4.0f), rnd (0.2f, 0.6f), 2.0f);
        p.mod ("Env 3", "Osc A Position", rnd (0.25f, 0.55f), false);
    }
    if (aggr > 0.5f) { p.env (3, 0.002f, rnd (0.1f, 0.4f), 0.0f, 0.2f); p.mod ("Env 4", "Filter 1 Drive", rnd (0.2f, 0.45f), false); }

    // ------------------------------------------------------------ 6) LFOs
    const float tmp = 0.0f; (void) tmp;
    if (t[WOBBLE] > 0.3f || t[RHYTHM] > 0.4f)
    {
        const char* div = t[FAST] > 0.4f ? "1/16" : (t[SLOW] > 0.4f ? "1/4" : (rng.nextBool() ? "1/8" : "1/16"));
        if (t[WOBBLE] > 0.3f) p.lfo (0, rng.nextBool() ? "Sine" : "Triangle", 2.0f, 1.0f, true, div, 0.0f);
        else
        {   // free-form step pattern (random gate / level sequence)
            const int steps = rng.nextBool() ? 8 : 16;
            LfoCustom c; c.n = 0;
            for (int i = 0; i < steps; ++i)
            {
                const float y = rng.nextFloat() < 0.3f ? 0.05f : rnd (0.55f, 1.0f);
                c.x[c.n] = std::max (0.0f, (float) i / (float) steps + (i > 0 ? 0.001f : 0.0f)); c.y[c.n++] = y;
                c.x[c.n] = std::min (1.0f, (float) (i + 1) / (float) steps - 0.001f); c.y[c.n++] = y;
            }
            c.x[0] = 0.0f; c.x[c.n - 1] = 1.0f;
            p.lfo (0, "Custom", 2.0f, 1.0f, true, steps == 8 ? "1/2" : "1/1", 0.05f);
            p.lfoCustom (0, c);
        }
        p.mod ("LFO 1", "Filter 1 Cutoff", rnd (0.45f, 0.8f), true);
        if (t[WOBBLE] > 0.5f) p.mod ("LFO 1", "Osc A Warp", rnd (0.15f, 0.4f), true);
        notes.add ("synced LFO " + juce::String (div));
    }
    if (motion > 0.25f)
    {
        p.lfo (1, rng.nextBool() ? "Triangle" : "Sine", rnd (0.07f, 0.4f));
        p.mod ("LFO 2", "Osc A Position", rnd (0.3f, 0.65f) * (0.4f + motion), true);
        p.lfo (2, "Sine", rnd (0.12f, 0.7f));
        p.mod ("LFO 3", "Filter 1 Cutoff", rnd (0.12f, 0.3f), true);
        p.mod ("LFO 3", "Osc B Position", rnd (-0.4f, 0.4f), true);
        if (motion > 0.6f) { p.lfo (3, "S&H", rnd (0.4f, 2.0f), 1.0f, false, "1/4", 0.35f); p.mod ("LFO 4", "Osc C Position", rnd (0.2f, 0.5f), true); }
        notes.add ("motion LFOs");
    }
    if (t[VIB] > 0.3f || isLead || isKeys || (arch != nullptr && (arch->flags & 1)))
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

    if (recipe != nullptr && recipe->lfo != 0)
    {
        switch (recipe->lfo)
        {
            case 1:  p.lfo (0, "Sine", rnd (0.06f, 0.14f)); p.mod ("LFO 1", "Filter 1 Cutoff", rnd (0.25f, 0.45f), true); break;
            case 2:  p.lfo (1, "Triangle", rnd (0.08f, 0.2f)); p.mod ("LFO 2", "Osc A Position", rnd (0.4f, 0.6f), true); p.mod ("LFO 2", "Osc B Position", -rnd (0.25f, 0.45f), true); break;
            case 3:  p.lfo (0, "Sine", 2.0f, 1.0f, true, "1/8"); p.mod ("LFO 1", "Filter 1 Cutoff", rnd (0.5f, 0.75f), true); break;
            case 4:
            {
                LfoCustom c; c.n = 0;
                for (int i = 0; i < 8; ++i)
                {
                    const float y = (i % 2 == 0) ? rnd (0.8f, 1.0f) : rnd (0.1f, 0.5f);
                    c.x[c.n] = std::max (0.0f, (float) i / 8.0f + (i > 0 ? 0.001f : 0.0f)); c.y[c.n++] = y;
                    c.x[c.n] = std::min (1.0f, (float) (i + 1) / 8.0f - 0.001f); c.y[c.n++] = y;
                }
                c.x[0] = 0.0f; c.x[c.n - 1] = 1.0f;
                p.lfo (0, "Custom", 2.0f, 1.0f, true, "1/2", 0.05f); p.lfoCustom (0, c);
                p.mod ("LFO 1", "Filter 1 Cutoff", rnd (0.4f, 0.7f), true);
                break;
            }
            case 5:  p.lfo (4, "Sine", rnd (4.8f, 6.0f)); p.mod ("LFO 5", "Osc A Pitch", rnd (0.0012f, 0.002f), true); p.mod ("LFO 5", "Osc B Pitch", rnd (0.0012f, 0.002f), true); break;
            case 6:  p.env (1, 0.001f, rnd (0.15f, 0.3f), 0.0f, 0.1f); p.mod ("Env 2", "Filter 1 Cutoff", rnd (0.6f, 0.85f), false); p.mod ("Velocity", "Filter 1 Resonance", 0.2f, false); break;
            case 7:  p.lfo (2, "S&H", rnd (0.5f, 2.0f), 1.0f, false, "1/4", 0.3f); p.mod ("LFO 3", "Osc A Pan", 0.7f, true); p.mod ("LFO 3", "Filter 1 Cutoff", 0.35f, true); break;
            default: p.lfo (0, "Saw", 2.0f, 1.0f, true, "1/4"); p.mod ("LFO 1", "Osc A Level", -0.45f, true); p.mod ("LFO 1", "Filter 1 Cutoff", 0.2f, true); break;   // sidechain-style pump
        }
        notes.add ("recipe LFO " + juce::String (recipe->lfo));
    }

    // ------------------------------------------------------------ 7) FX chain (reverb / delay types, analog compressor)
    const bool wantRev = space > 0.2f || isPad || isFx || isPluck || isKeys;
    if (wantRev)
    {
        const char* rt = t[REVERSEFX] > 0.4f ? "Reverse" : (t[SHIMMER] > 0.4f ? "Shimmer" : (t[CATHEDRAL] > 0.4f ? "Cathedral" : (t[PLATE] > 0.4f ? "Plate" : (t[ROOM] > 0.4f ? "Room"
                         : (isLead ? "Plate" : (isPad ? (rng.nextBool() ? "Cathedral" : (rng.nextBool() ? "Shimmer" : "Hall")) : (isPluck ? (rng.nextBool() ? "Plate" : "Hall") : "Hall")))))));
        p.fx (0, clamp01 (0.08f + 0.5f * space), { { "Size", 0.3f + 0.65f * space }, { "Damping", t[WARM] > 0.4f ? 0.65f : 0.4f }, { "Width", 1.0f }, { "Pre-Delay", rnd (5.0f, 40.0f) },
                           { "High Cut", t[WARM] > 0.4f ? 7000.0f : 13000.0f } });
        p.fxChoice (0, "Type", rt);
        notes.add (juce::String ("reverb: ") + rt);
    }
    if (echo > 0.12f)
    {
        const char* dt = t[REVERSEFX] > 0.4f ? "Reverse" : ((t[TAPE] > 0.3f || (t[ANALOG] > 0.4f && echo > 0.3f)) ? "Tape Echo" : ((t[PINGPONG] > 0.3f || wide > 0.5f) ? "Ping-Pong" : "Stereo"));
        p.fx (1, 0.14f + 0.36f * echo, { { "Time L", rng.nextBool() ? 375.0f : 250.0f }, { "Time R", rng.nextBool() ? 500.0f : 375.0f }, { "Feedback", 0.22f + 0.4f * echo },
                                         { "Tone", t[WARM] > 0.4f ? 4500.0f : 9000.0f } });
        p.fxChoice (1, "Type", dt);
        notes.add (juce::String ("delay: ") + dt);
    }
    if (wide > 0.25f || t[CHORUS] > 0.3f || isPad)
        p.fx (2, 0.25f + 0.35f * std::max (wide, t[CHORUS]), { { "Rate", rnd (0.3f, 0.9f) }, { "Depth", rnd (0.35f, 0.65f) }, { "Width", 1.0f } });
    if (t[FLANGER] > 0.3f) p.fx (3, 0.5f, { { "Rate", rnd (0.1f, 0.4f) }, { "Depth", 0.75f }, { "Feedback", 0.6f } });
    if (t[PHASER] > 0.3f)  p.fx (4, 0.5f, { { "Rate", rnd (0.15f, 0.6f) }, { "Depth", 0.8f }, { "Feedback", 0.5f } });
    const bool wantDist = aggr > 0.35f || t[GRIT] > 0.3f || t[LOFI] > 0.4f || t[FUZZ] > 0.3f || t[TUBE] > 0.3f || t[FOLD] > 0.3f;
    if (wantDist)
    {
        const char* dt = t[FUZZ] > 0.3f ? "Fuzz" : (t[FOLD] > 0.3f ? "Fold" : (t[TUBE] > 0.3f ? "Tube" : (t[LOFI] > 0.4f ? (t[TAPE] > 0.3f ? "Tape" : (rng.nextBool() ? "Bit Crush" : "Downsample"))
                         : (aggr > 0.75f ? (rng.nextBool() ? "Hard Clip" : "Diode") : (rng.nextBool() ? "Overdrive" : "Soft Clip")))));
        p.fx (5, 1.0f, { { "Drive", 5.0f + 22.0f * std::max (aggr, t[GRIT]) }, { "Tone", t[WARM] > 0.4f ? 6000.0f : 13000.0f }, { "Bits", rnd (7.0f, 11.0f) }, { "Output", -5.0f } });
        p.fxChoice (5, "Type", dt);
        notes.add (juce::String ("distortion: ") + dt);
    }
    p.fx (6, 1.0f, { { "Low Cut", isBass ? 25.0f : rnd (30.0f, 55.0f) }, { "High Cut", (t[WARM] > 0.45f || t[LOFI] > 0.4f) ? rnd (7500.0f, 11000.0f) : 20000.0f },
                     { "High Gain", bright > 0.5f ? 2.5f : (t[WARM] > 0.4f ? -3.0f : 0.0f) }, { "Low Gain", t[SUB] > 0.5f ? 2.5f : 0.0f } });
    if (t[COMP] > 0.3f || t[LOUD] > 0.4f || isBass)
    {
        p.fx (9, 1.0f, { { "Threshold", rnd (-24.0f, -14.0f) }, { "Ratio", rnd (2.5f, 5.0f) }, { "Attack", rnd (4.0f, 18.0f) }, { "Release", rnd (80.0f, 200.0f) }, { "Knee", 6.0f }, { "Gain", rnd (1.5f, 4.0f) } });
        notes.add ("analog compressor");
    }
    if (t[LOUD] > 0.5f || aggr > 0.6f) p.fx (7, clamp01 (0.2f + 0.35f * std::max (t[LOUD], aggr)), { { "Depth", 0.55f + 0.3f * t[LOUD] }, { "Time", 0.4f }, { "Gain", 1.0f } });
    p.fx (8, 1.0f, { { "Threshold", -1.0f }, { "Release", 60.0f } });
    if (recipe != nullptr)
    {
        if (recipe->rev != nullptr)
        {
            p.fx (0, recipe->revMix, { { "Size", 0.45f + 0.5f * std::min (1.0f, recipe->revMix * 1.3f) }, { "Damping", 0.45f }, { "Width", 1.0f }, { "Pre-Delay", rnd (8.0f, 35.0f) } });
            p.fxChoice (0, "Type", recipe->rev);
        }
        if (recipe->dly != nullptr)
        {
            p.fx (1, recipe->dlyMix, { { "Time L", rng.nextBool() ? 375.0f : 250.0f }, { "Time R", rng.nextBool() ? 500.0f : 375.0f }, { "Feedback", 0.25f + 0.4f * recipe->dlyMix }, { "Tone", 6500.0f } });
            p.fxChoice (1, "Type", recipe->dly);
        }
        if (recipe->chorus >= 0.0f) p.fx (2, recipe->chorus, { { "Rate", rnd (0.3f, 0.8f) }, { "Depth", rnd (0.4f, 0.65f) }, { "Width", 1.0f } });
        if (recipe->dist != nullptr)
        {
            p.fx (5, 1.0f, { { "Drive", recipe->drive }, { "Tone", 11000.0f }, { "Output", -4.0f } });
            p.fxChoice (5, "Type", recipe->dist);
        }
        if (recipe->comp) p.fx (9, 1.0f, { { "Threshold", -20.0f }, { "Ratio", 3.5f }, { "Attack", 8.0f }, { "Release", 120.0f }, { "Knee", 6.0f }, { "Gain", 3.0f } });
    }
    p.standardChain();

    // ------------------------------------------------------------ 8) effect modulation + macros (all 8 macros do something useful)
    auto fxOn = [&p] (int i) { return p.get (ids::fx (i, "on"), 0.0f) > 0.5f; };
    if (motion > 0.3f)
    {
        if (fxOn (2)) p.mod ("LFO 6", "Chorus Depth", rnd (0.15f, 0.3f), true);
        if (fxOn (4)) p.mod ("LFO 2", "Phaser Centre", rnd (0.2f, 0.45f), true);
        if (fxOn (3)) p.mod ("LFO 3", "Flanger Depth", rnd (0.1f, 0.25f), true);
        if (fxOn (0) && space > 0.5f) p.mod ("LFO 3", "Reverb Mix", rnd (0.05f, 0.15f), true);
        if (fxOn (1)) p.mod ("LFO 4", "Delay Feedback", rnd (0.08f, 0.2f), true);
    }
    if (fxOn (5) && aggr > 0.4f) p.mod ("Env 4", "Distortion Drive", rnd (0.1f, 0.3f), false);
    p.mod ("Macro 1", "Filter 1 Cutoff", 0.6f, false);
    p.mod ("Macro 2", "Osc A Position", 0.5f, false);
    p.mod ("Macro 3", "Filter 1 Resonance", 0.4f, false);
    p.mod ("Macro 4", "Osc B Level", 0.5f, false);
    p.mod ("Macro 5", fxOn (0) ? "Reverb Mix" : (fxOn (2) ? "Chorus Mix" : "Delay Mix"), 0.5f, false);
    p.mod ("Macro 6", fxOn (5) ? "Distortion Drive" : "Osc A Warp", 0.5f, false);
    p.mod ("Macro 7", "Osc A Detune", 0.4f, false);
    p.mod ("Macro 8", fxOn (1) ? "Delay Feedback" : "Osc B Position", 0.4f, false);

    // ------------------------------------------------------------ 9) play mode / arp
    if (isBass || isLead || t[MONO] > 0.4f || t[GLIDE] > 0.3f || (arch != nullptr && (arch->flags & 8)))
        p.play (t[GLIDE] > 0.5f && t[MONO] > 0.2f ? "Legato" : "Mono", t[GLIDE] > 0.3f ? rnd (70.0f, 220.0f) : rnd (15.0f, 45.0f));
    else p.play ("Poly", 0.0f);

    if (recipe != nullptr && recipe->mode >= 0)
        p.play (recipe->mode == 0 ? "Poly" : (recipe->mode == 1 ? "Mono" : "Legato"), recipe->glide >= 0.0f ? recipe->glide : 20.0f);

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
