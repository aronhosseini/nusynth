# Round 24

- Presets are stored in category folders again, exactly like your Presets zip: `Presets/ARP`, `BA`, `FX`, `LD`, `PD`, `PL`, `SY` (68 presets, `Presets/Main` is gone).
  CMake embeds `Presets/*/*.nsp` (GLOB_RECURSE).
- Preset menu folders: ARP = ARP, BA = BASS, FX = FX, LD = LEAD, PD = PAD, PL = PLUCKED (was "PLUCK"), SY = SYNTH. The prefix check is now case-insensitive
  and also accepts the long words, so no preset can fall into a "MAIN" folder because of its name.

# Round 23

- Noise osc: 16 new ORGANIC noises (AC hum1/2, Air Can 1-5, CymLoop, CymMicBleed, H-Breath, Paper Bag, S-Sound, Shush 1/2, Squeaky, WindChimes),
  synthesised in NoiseExtras.h (not samples). Appended at the end of the noise list, so old presets keep their noise type.
- Noise menu order: Analog (the old White / Pink / Brown / Analog Tape / Circuit Noise merged in, then the analog set), Organic, S2 Noises, Sample.
- Osc A / B / C wave menus: the "Serum 2 style" sections are gone; each category (Analog, Digital, Spectral, Vowel) is one merged alphabetical list.

# Round 22

- Main presets: all old presets removed and replaced by the 68 presets from Presets.zip (ARP 10, BA 10, FX 11, LD 10, PD 10, PL 9, SY 8).
  The category sub-folders were flattened into `Presets/Main` (spaces -> underscores, same naming as before) because the build embeds `Presets/Main/*.nsp`.

# Round 21

- LFO start delay (rare): the LFO smoother used to start at 0 and climb to the LFO value, which sounded like a long attack when Smooth > 0
  (voice LFOs and effect LFOs). It now starts AT the first LFO value on every note.
- ARP + LFO: with the arpeggiator on, every arp step is a new MIDI note, so RETRIG / ENVELOPE LFOs restarted on each step and a slow LFO never
  got going while keys were held (it only "worked" after release). While the arp is on these LFOs now use one shared phase that restarts only
  when a NEW key (group) is pressed. FREE-mode LFOs are unchanged. (Engine::setArp, Arpeggiator::consumeRestart)
- Micro tune: double-click any key to TYPE its value in cents (-100..+100; "12", "+12.5", "-30 ct" all work). Right-click a key: Type value / Reset to 0.
- Online AI generator: the prompt now makes 3-5 effects mandatory (with the effect numbers listed) and the reply is checked: an effect with a mix /
  parameter but no "on" is switched on, and if fewer than 3 effects are on the built-in designer adds fitting ones (the model's own effects are kept).

# Round 20

- Starting LFOs: every LFO now starts with the shape from your screenshot (Custom: 3 points, bottom-left -> top-centre -> bottom-right, straight lines).
  Used when the plugin is created, when a preset / AI sound has no stored shape, and by LFO reset.
- Main presets: added the new presets from Presets.zip to Presets/Main (BA Anyma STAB, BA Melodic Techno, LD Guitar Flanger KORON, PD Sea Wave Dream,
  PD Simple SAW, PL JUNO, PL Nylon Soft, PL Soft Trance, SY RAIN DROP, SY Rhodes) and updated LD NEY ANBOUN. 43 main presets now.
- AI sound designer: genre sounds keep their genre waveforms / warp / filter (only gentle movement is added); the random synthesis-method layer
  runs for non-genre prompts. All earlier knowledge (words, recipes, tone words, methods, anti-repeat) is still active.
- AI sound designer, new accuracy rules: clean focused bass (no wide reverb / chorus, narrow spread, centred sub); plucks / stabs with instant attack and
  ~zero sustain and a fast filter envelope; pads with slow attack, long release and gentle evolving filter movement; instrument families
  (strings, brass, winds, mallets, plucked strings, drums / percussion) get the right onset and decay.

# Round 19

- Knobs: right-click on the knob CIRCLE opens the menu (Reset to default, Type value, then modulation entries). Right-click anywhere else
  (label, empty cell, blank FX space) is "blank": in the FX rack it opens the "+ FX" menu.
- AI sound designer knows 40 electronic genres x 4 roles (Bass / Pad / Pluck / Synth) from the genre guide: House, Melodic House, Deep House,
  Tech House, Progressive House, Acid House, French House, Tropical House, UK Garage, Techno, Melodic / Detroit / Minimal / Industrial Techno,
  Hardstyle, Hardcore / Gabber, Trance, Uplifting Trance, Psytrance, DnB, Liquid DnB, Jungle, Dubstep, Breakbeat, UK Bass, Ambient, Downtempo,
  Chillwave, Trip Hop, Lo-Fi Beats, Synthwave, Darksynth, Electro, Electro House, IDM, Glitch, Dub, Footwork, Jersey Club, Hyperpop.
  Type e.g. "deep house bass", "psytrance pad", "synthwave lead", "dnb pluck", "liquid dnb pad"; a genre alone gives its synth sound.
  Tone words (bright, soft, warm, dark, harsh, reverb) still steer the result, and the synthesis-method layer still varies every generation.

# Round 18 - FL Studio hang when deleting the plugin

- Static knob-cap images (juce::Image in a function-local static) are now cleared when the editor closes; images that outlive JUCE's shutdown can hang the host on DLL unload.
- The modulation-pointer cache (keyed by the processor address) could hand out dangling pointers when FL re-created the plugin at the same address; it now validates and rebuilds itself.
- The AI thread is no longer started with every plugin instance (only on the first GENERATE); the localhost network bridge is off by default
  (set NUSYNTH_AI_ENDPOINT to use one), connection timeout shortened, and shutdown only joins the thread when it exists.
- Editor destructor tears down pages / children in a fixed order before the look-and-feel is released.

# Round 17

- Installers: last page now says "Follow Aron H on Instagram" with the link https://www.instagram.com/aronh.official/
  (Windows: ticked "Follow Aron H on Instagram" box that opens the page; macOS pkg: conclusion page; install.sh + Homebrew cask print it).
- Knobs (FX rack and everywhere): right-click anywhere on a knob cell, including its label, opens the menu with Reset to default / Type value...
  Right-click on empty FX space (not on a knob) still opens the "+ FX" menu.

# Round 16

- FX rack: right-click on ANY empty area (rack background, effect boxes, graphs, panel margins) opens the "+ FX" menu.
- AI sound designer: each generation now also picks a synthesis method (subtractive, FM, phase distortion, ring/AM, hard sync, PWM,
  wavefold, wavetable morph, additive, supersaw, comb/Karplus string, granular, formant) - waveforms, warp, filter and modulation all change.
  Wavetable choice has a much wider random spread, and instrument / recipe waves are varied too. The last 3 generations are avoided.

# Round 15

- FX page: the macro strip is now the same full box as on the MAIN page (full sentence, macros inside the same box).
- FX rack: right-click on empty space opens the "+ FX" menu.
- Reverb visualiser: removed the grey grid lines and early-reflection spikes; only the cyan decay curve remains.
- AI sound designer: every GENERATE starts fresh (new random seed, voices silenced, previous structure is avoided).
- AI sound designer: new recipes for Bass Analog Soft/Saw, Key Piano/Rhodes/Organ, Pad Space/Granular/Bright,
  Plucked Retro/80s, Synth Retro/80s (+ Pad/Key Retro/80s). Bright/soft/warm/dark/harsh/reverb words now steer recipes.
  New genre words (darkwave, industrial, phonk, afrobeat, new age ...).

# Changes in this revision

**Not compiled yet** – this revision was written without a JUCE toolchain at hand. Build it once (`cmake -B build && cmake --build build`) and send me the first compiler messages if there are any.

## Oscillators
* **Warp menu** (one button per oscillator) grouped like Serum 2: Off · Sync · Alt Warp ▸ · Filter ▸ · Distortion ▸ · FM ▸ · PD ▸ · AM ▸ · RM ▸
  * Alt Warp: Bend +/−/±, PWM, Asym +/−/±, Flip, Mirror, Remap 1-4, Quantize, Odd/Even
  * Filter: LPF, HPF (harmonic filters)
  * Distortion: Tube, Soft Clip, Hard Clip, Diode 1/2, Linear Fold, Sine Fold, Zero-Square, Asym, Rectify, Sine Shaper, Stomp Box, Tape Sat., Soft Sat.
  * FM / PD / AM / RM: sub-menu with the source – Osc A, B, C, Sub, Noise, **Filter 1, Filter 2** (new). PD (phase distortion) is new.
  * The first 16 warp modes keep their old indices, so old presets sound the same.
* New knobs per oscillator: **Coarse** (±48 st, continuous) and **Blend** (unison: 0 = centre voice only, 1 = all voices). Both are modulation destinations.
* **Drag & drop wavetable import** on every oscillator and on the harmonic editor: a menu offers Dynamic pitch (zero snap / follow), Frequency estimation, Pitch average and FFT 256/512/1024/2048 (`UserContent::importWave`).

## Noise oscillator
* The on/off button was hidden behind the LOOP button – LOAD / LOOP moved next to the type selector.
* **Pitch now works on every noise type** (the generator is re-clocked like a sample played at another speed; raising the pitch also tilts the spectrum so white noise gets brighter).

## Filter
* Two more ring-modulator filter types: *Ring Mod Square*, *Ring Mod Dual* (appended, old presets unchanged).

## FX rack (Serum 2 style)
* **+ FX** menu adds any effect, up to 8 copies of each type (`kFxCopies` in `ParameterIDs.h`) – several reverbs, several distortions … each with its own tail / state and its own modulation destinations ("Reverb #2 Size" …).
* Grip = re-order, × = remove (the slot is reset).
* **Compressor** = Analog + OTT in one effect, with a SINGLE | OTT toggle. Old presets are converted on load (`Presets/Migrate.h`).

## Presets
* Folders inside `Documents/Aron H/NuSynth/Presets` show up as nested sub-menus under USER (also empty ones); "New folder…" and "Show user presets folder" in the USER menu; the SAVE dialog has a folder chooser.
* Saving a preset also writes the user wavetables used by the oscillators to `NuSynth/User Wavetable` and the noise sample to `NuSynth/User Noise`.
