# MasterLens 1.4.3 — performance notes

All numbers are from the same Apple Silicon Mac, Release build, 48 kHz stereo.
The UI benchmark is the offscreen editor (`MasterLensUiShot bench pageN`): 5 seconds
of message-loop + 30 fps editor timer with a pre-analysed signal. It measures process
CPU time (`clock()`) rather than wall time, so it includes drawing and analysis but
not the audio callback.

## UI thread (before → after)

| Page | 1.4.2 | 1.4.3 |
| --- | ---: | ---: |
| Loudness (page 0) | 4.0 – 6.5 % | **1.1 – 1.3 %** |
| Spectrum (page 2) | — (not captured) | **1.6 – 1.7 %** |
| Signal health (page 5) | 4.9 % | **1.1 – 1.2 %** |

The 1.4.2 range reflects run-to-run scheduling noise; the 1.4.3 numbers are stable
to ±0.1 %. Roughly a 4–5× reduction on the loudness page.

## Audio / DSP

The offline DSP suite (60 s of stereo audio) measures ~0.30–0.50 % of one core,
which is within benchmark noise of 1.4.2 (0.45 % nominal). The audio-path changes
in 1.4.3 are structural rather than arithmetic inside the test:

- `SpectrumRing` now writes a whole block with one acquire/release instead of two
  atomics per sample.
- Mono input reuses the left-channel filter results instead of running the same
  biquad chain twice.

## What changed in the UI path

- Precomputed fractional-octave windows (no per-frame `pow`/`floor` over 4097 bins).
- Linear-power prefix from the FFT (no dB→power `pow`), fast log10, 3-bin peak hold.
- No per-frame zeroing of the 2 × 8192 FFT buffer.
- Spectrum analysis is page-aware: 20 fps on the spectrum page, 5 fps elsewhere,
  and fully suspended while Hold is engaged.
- Advisor / rail / issue list at 5 Hz, signal defects at 2 Hz, timeline marker
  copies at 2 Hz, heavy pages repaint at 5 Hz.
- `UiState` is moved instead of copied.

Reproduce:

```bash
cmake --build build-universal --target MasterLensUiShot
./build-universal/MasterLensUiShot_artefacts/Release/MasterLensUiShot.app/Contents/MacOS/MasterLensUiShot bench page0
```
