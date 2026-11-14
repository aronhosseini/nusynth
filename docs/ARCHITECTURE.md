# NuSynth architecture

## 1. Folder structure
```
Source/
  PluginProcessor.*            AudioProcessor, APVTS owner, MIDI -> synth
  PluginEditor.*               TabbedComponent (MAIN / MATRIX / FX / ARP / MICROTUNE)
  Parameters/                  ParameterIDs.h, ParameterLayout.*   (single source of truth)
  DSP/
    MicroTuning.h              12 cents -> 128-note frequency table
    SynthVoice.*               voice (bootstrap now; full engine next)
    (next) WavetableBank, Oscillator, Unison, Warp, SubOsc, NoiseOsc,
           FilterBank (30 types), Router, Envelope, LFO, ModMatrix, Arpeggiator, Portamento
    (next) FX/ Reverb, Delay, Chorus, Flanger, Phaser, Distortion, MultiEQ, OTT, Limiter, FxChain
  UI/                          (next) OscPanel, FilterPanel, ModMatrixTable, KeyboardTuner, EnvelopeView, LfoEditor...
  AI/                          PromptBridge (thread + HTTP + JSON -> APVTS)
```

## 2. Audio loop / voice strategy
- `processBlock`: `ScopedNoDenormals` -> `tuning.update()` (12 atomics) -> arp (MIDI in/out) ->
  `synth.renderNextBlock` -> FX chain -> master gain. No allocation, no locks, no I/O.
- Voice (per note): Osc A + Osc B (wavetable, unison up to 16, warp) + Sub + Noise are summed per **route**
  into three buses: F1-only, F2-only, bypass. Parallel = copy to F1 and F2 buses; Series = F1 output feeds F2.
  Filters -> amp (Env 1) -> voice output.
- Modulation: per-block control-rate pass (LFOs, Envs 2-3, velocity, key) resolves the mod matrix into
  an offset array; audio-rate only for pitch/cutoff where required. Parameters use `SmoothedValue`.
- Mono/Legato/Glide: a voice allocator picks voice 0 in mono modes; portamento is an exponential/curved
  ramp on the *note frequency* (after microtuning lookup), so glides land on the tuned pitch.
- Performance: process unison/filters in SIMD (`juce::dsp::SIMDRegister` / xsimd) four voices at a time;
  wavetables are band-limited mip-mapped, built on a background thread and swapped via an atomic pointer.

## 3. APVTS + tabbed editor
- All params are declared once in `ParameterLayout.cpp`; IDs live in `ParameterIDs.h`.
- The audio thread reads raw atomics (`getRawParameterValue`) - lock-free. UI uses `SliderParameterAttachment`,
  `ComboBoxParameterAttachment`, `ButtonParameterAttachment`.
- Editor: `TabbedComponent` with 5 pages; MAIN hosts Oscillators / Noise / Sub / Filter / Mod mini-matrix /
  LFO / ENV / Play modes / Per-key tuning / Type-to-Sound bar, like the reference image.
- Non-parameter state (custom LFO shapes, wavetable choice names, FX order) lives in `apvts.state` child trees.

## 4. Microtuning
`freq(note) = 440 * 2^((note-69)/12) * 2^(cents[note % 12] / 1200)`; one slider per pitch class applies to
MIDI notes 0-127 (global octave propagation).

## 5. AI prompt flow
UI (message thread) -> `submit()` -> background thread (HTTP/ONNX or fallback) -> JSON ->
`callAsync` back to message thread -> `param->setValueNotifyingHost()` (atomic; audio thread never blocks).
