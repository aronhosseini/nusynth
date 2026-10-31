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
- 100 factory presets (BASS, KEYS, PLUCK, PAD, LEAD, FX, ARP) + INIT + user presets
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
