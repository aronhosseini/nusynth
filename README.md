# NuSynth  (developed by Aron H)

Wavetable synthesizer VST3 (JUCE 8, C++20). Inspired by Serum and Synplant 2.

## Build locally
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target NuSynth_VST3
```
Output: `build/NuSynth_artefacts/Release/VST3/NuSynth.vst3` (first configure downloads JUCE).

## CI / installers (GitHub Actions)
`.github/workflows/build.yml` builds on every push/PR and produces:
- **Windows:** `NuSynth-<ver>-Windows-Setup.exe` (Inno Setup -> `C:\Program Files\Common Files\VST3`)
- **macOS:** `NuSynth-<ver>-macOS.pkg` (universal arm64 + x86_64 -> `/Library/Audio/Plug-Ins/VST3`)

Create a release:
```bash
git init && git add . && git commit -m "NuSynth foundation"
git branch -M main
git remote add origin https://github.com/<you>/NuSynth.git
git push -u origin main
git tag v0.1.0 && git push origin v0.1.0     # builds both installers and attaches them to a GitHub Release
```

### Optional macOS signing + notarization (recommended for distribution)
Add these repository secrets; without them an ad-hoc-signed, un-notarized pkg is produced.
`APPLE_CERT_APP_P12`, `APPLE_CERT_INST_P12` (base64 .p12 files), `APPLE_CERT_PASSWORD`,
`APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`.

Windows: unsigned installers trigger SmartScreen warnings; add an Authenticode signing step when you have a certificate.

## AI "Type-to-Sound"
`PromptBridge` POSTs `{"prompt": "...", "schema": "nusynth-v1"}` to `http://127.0.0.1:8765/generate`
and expects `{"params": {"<paramID>": value, ...}}` (choice params accept the choice name as a string).
If the server is unreachable, a small offline keyword fallback is used.

See `docs/ARCHITECTURE.md` for the design.


## What the engine does now (rev 4)
- 56 wavetables x 8 morph frames (band-limited mip-maps), unison up to 16 voices, spread/detune, warp (Sync / Wavefold / Bend), phase
- Sub (sine/tri/square), noise (white/pink/brown/tape/circuit) with HP/LP
- 2 filters x 30 types, per-source routing: Bypass / F1 / F2 / Parallel / Series
- 3 LFOs (drawable 16-point shape, tempo sync, smooth), 3 envelopes (draggable points + curves)
- 16-slot mod matrix. Drag an LFO/ENV pill onto any modulatable knob (orange ring = modulation; right-click the knob to edit/remove)
- Poly / Mono / Legato + glide, arpeggiator (7 patterns, octaves, gate, swing, sync), per-key microtuning
- FX rack: Reverb, Delay, Chorus, Flanger, Phaser, Distortion, 3-band EQ, OTT-style compressor, Limiter - re-orderable (UP/DN)


## rev 5 additions
- OSC C (3 wavetable oscillators), pan on every oscillator / sub / noise, noise pitch + fine
- Master volume + master tune (A4 400-480 Hz) on top of MAIN, polyphony limit (1-16)
- Yellow draggable keys: LFO 1-3, ENV 1-3 (in the panel headers), VEL and KEY (top strip)
- FX rack with live graphs (reverb tail, delay taps, LFO waves, phaser notches, distortion curve, interactive EQ, OTT / limiter curves)
- Pages: MAIN, FX, OTHER (mod matrix on top, microtuning below)
- 68 main presets (ARP, BASS, LEAD, PAD, PLUCKED, SYNTH, FX) embedded from `Presets/<ARP|BA|FX|LD|PD|PL|SY>` + INIT + user presets (saved in Documents/Aron H/NuSynth/Presets)
  (user presets are saved as .nsp files in `<user app data>/Aron H/NuSynth/Presets`)
- AI designer (offline): picks a base preset from your words, then edits oscillators, filters, envelopes, LFOs,
  mod matrix, FX chain, arp and play mode


## rev 6 additions
- Warp modes: Sync, Remap, Bend +/-/+-, PWM, Asym +/-/+-, Quantize, Mirror, Wavefold, FM / AM / RM (source = Osc A/B/C, Sub or Noise)
- Wave picker grouped ANALOG / DIGITAL / VOWEL / USER (9 new vowel tables + 8 user slots: draw on the display, right-click for sample import, or drop an audio file on the OSC panel)
- Noise oscillator "Sample" type (LOAD button or drop a file; Noise Pitch / Fine change the playback speed)
- Distortion: Soft Clip, Hard Clip, Tube, Fold, Bit Crush, Tape, Diode, Overdrive, Fuzz, Sine Shaper, Chebyshev, Downsample
- EQ: low cut + high cut (draggable nodes on the graph)
- Filter key track button, pitch-bend range (0-24 up / down), 8 macros (header), mod wheel key, on-screen keyboard + wheels
- OTHER page: mod matrix (top) | per-key microtuning + voicing (bottom halves)


## rev 7 additions
- Fixed Windows build (JUCE audio_utils / audio_devices includes for MidiKeyboardComponent + MidiMessageCollector)
- 6 LFOs (free-point custom shapes), 4 envelopes, harmonic editor (pencil on every oscillator), on/off for every oscillator
- Undo / redo, output meter, macros on MAIN, octave buttons, Yamaha style wheels, MIDI-lit keyboard, panic button
- Stuck-note fix: note-offs now release every voice that belongs to the key, whatever play mode it started in
- 10 more arp styles, sub saw / pulse and +-4 octaves, noise sample loop / one-shot + waveform view


## rev 8 additions
- Bright pink modulation keys, LFO sync 32 bar ... 1/64, smoother filter (parameters smoothed every 8 samples)
- Noise samples follow pitch bend; pitch-bend range button (2 / 5 / 12 / 24) next to the pitch wheel
- Wavetable editor (pencil on every oscillator) rebuilt like Serum 2: harmonic + phase bars, brushes, frame strip (up to 8 frames), formula, import / export
- Modulation amount: left-click the outer ring of a knob and move around it; right-click > Change source
- AI designer now composes a new patch from semantic traits instead of choosing a preset


## Installing on macOS without Gatekeeper warnings
The installers are not notarized (that needs a paid Apple Developer account), so a double-clicked .pkg can be blocked.
Use one of these instead - files fetched by curl / Homebrew do not get macOS' quarantine flag, and the plugin is ad-hoc signed:

```bash
# 1) one-line installer (user install, no password)
curl -fsSL https://github.com/<you>/NuSynth/releases/latest/download/install.sh | bash
#    all users:  ... | bash -s -- --system        uninstall:  ... | bash -s -- --uninstall

# 2) Homebrew (create a repo called homebrew-nusynth, the workflow fills Casks/nusynth.rb for you; or copy dist/nusynth.rb)
brew tap <you>/nusynth
brew install --cask nusynth
```
If you already downloaded a .pkg / .vst3 by browser and macOS refuses it: `xattr -dr com.apple.quarantine /path/to/file`.
Optional: add the repository secret `HOMEBREW_TAP_TOKEN` so the workflow pushes the cask to your tap automatically.


## rev 9 additions
- Effects can be modulated: drag any pink key onto an FX knob, or use the MATRIX tab (24 slots, voice + effect destinations)
- Reverb types: Hall, Room, Plate, Cathedral, Shimmer, Reverse. Delay types: Stereo, Ping-Pong, Tape Echo, Reverse
- Compressor (OTT) has an output Gain; new Compressor (Analog) effect (threshold, ratio, attack, release, knee, gain)
- 105 new factory presets composed by the sound designer (all synth features, macros, effect modulation)
- Sound designer: drums, 30+ instruments, 20+ genres, dual filters, step-LFO shapes, 300+ words
- Pencil (harmonic editor) now sits above the wave display; pitch-bend range - / + buttons beside the pitch wheel
- Gatekeeper-friendly mac install (see above)


## rev 10 additions
- FX page: macros on top, FX rack (2/3, drag the grip on the left of each effect to re-order), LFOs + envelopes (1/3) to drag onto effect knobs
- Matrix destination picker is grouped (Osc A, Filter 1, FX - Reverb, ...) and includes every effect knob
- Pitch / mod wheels redrawn after the hardware photo (matte wheel in a black slot, centre detent band)
- AI designer: about 1,250 words + 94 compound-phrase recipes ("pad warm night", "bass tech house", "303 acid", "stab chicago", "lead soft glide" ...)


## rev 11 changes
- Mouse wheel no longer turns knobs / sliders / drop-downs (it scrolls the page); FX + matrix lists are cached for smooth scrolling
- Lower CPU: filters only design the coefficients their type needs, knobs poll cached pointers (12 Hz)
- AI designer: LFOs / envelopes are used in a different order each generation, macros get different destinations and names every time
- Macros can be renamed (double-click or right-click the pink key); AI presets name them automatically
- Every preset change (and INIT, and AI generation) resets the per-key micro-tuning
- All 105 factory presets re-written for the newest designer (recipes, macros, effect modulation)


## rev 8 - new look (UI only, no engine / parameter changes)
- Re-skinned to the reference design: dark slate gradient panels with cyan/magenta tinted borders, neon-cyan accents,
  magenta modulation keys and rings, silver knobs with a glowing value arc, teal active buttons, glowing cyan logo,
  gradient GENERATE button, glossy keyboard with magenta wheel slots, cyan -> magenta output meter.
- All styling lives in `Source/UI/Theme.h` (palette + glow helpers) and `Source/UI/NuLookAndFeel.h` (knobs, buttons, combo boxes).


## rev 9 - envelope + LFO fixes
- Envelope: attack at its minimum is now a TRUE instant attack (full level on the first sample, and the modulation matrix sees
  that level immediately, so a filter envelope no longer sweeps open for ~12 ms when attack = 0).
- Envelope curves are now balanced (point-symmetric Bezier, `Source/DSP/EnvCurve.h`): the bend is shared equally by both nodes and the
  drag handle sits exactly in the middle of each curve. The audio engine and the display use the same function, so what you see is what you hear.
  (Existing patches keep their curve values; the shape of a non-zero curve is slightly different from before.)
- LFO mode per LFO: Free (shared phase, keeps running between notes), Retrig (restarts on every note, default = previous behaviour)
  and Envelope (plays the shape once per note, then holds the last value).


## rev 10
- Presets: the old factory presets are gone; the 33 presets from PRESETS.zip are embedded as the main presets (`Presets/*/*.nsp`, added via
  `juce_add_binary_data`). User presets are now stored in `Documents/Aron H/NuSynth/Presets`. Loading any preset first resets every parameter
  to its default, so new parameters never keep the previous sound's value.
- FX: new **Filter** effect (all 30 filter types of the main filter, cutoff / resonance / drive / mix); **Decay** on the reverb;
  **Low Cut / High Cut** on the delay (inside the feedback loop). New modulation destinations are appended at the end of the list,
  so existing modulation routings keep working.
- Noise OSC already has its on/off power button (next to the title), like the other oscillators.
- Wavetable editor: right-click the harmonic bars -> "Clear" keeps only the 1st harmonic.
- Right-click on any knob: Reset to default / Type value... (plus the modulation list when the knob is modulated).
- Header: the NuSynth logo uses the same cyan -> pink gradient as GENERATE; panels, knobs and buttons refined to match the reference image.


## rev 12
- FX: new **Hyper/Dimension** effect (Serum 2 style). HYPER = 2-7 detuned unison voices (Rate, Detune, Unison, Width, Hyper amount) spread over the
  stereo field; DIMENSION = mono-compatible Dimension-D style widener (Size, Dimension amount); plus the usual Mix. Every knob is modulatable.
  It is the 12th rack slot (new parameters only - old presets and modulation routings keep working).
- FX: the **Limiter** has 4 new knobs - Input (push into the limiter), Ceiling (hard output ceiling), Soft Clip (saturation in front of the limiter)
  and Width (mid/side). Defaults reproduce the old sound.
- FX: every effect now uses the same knob size (the Limiter used to get giant knobs because it only had 3).
- FX: **level visualizer** on Compressor (OTT), Limiter and Compressor (Analog): scrolling input-level history, gain reduction hanging from the top
  (pink), OTT upward gain rising from the bottom (green), IN / OUT / GR bars and dB read-outs. The audio thread publishes the MAX since the last
  UI refresh (`FxMeters` in `SharedData.h`), so short peaks are never missed.
- Look: the red/hot-pink of the draggable keys, modulation rings and box lines is now the pink-violet of the reference (`colours::magenta`);
  panels use the reference's glass gradient (light-blue glow top-left -> slate -> violet glow bottom-right, cyan -> pink edge);
  knobs are brushed-metal (cached cap image, `gfx::metalCapImage`) in a dark bezel with a white pointer.
