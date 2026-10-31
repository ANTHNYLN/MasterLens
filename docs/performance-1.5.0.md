# MasterLens 1.5.0 — performance, 60 fps spectrum and feature notes

## Why this release exists

1.4.3 rewrote the spectrum path but ran the spectrum page at ~20 fps. 1.5 keeps that
optimised path and raises the spectrum page to **60 fps** while adding basis/album
work, an audition engine and marker/automation plumbing.

## Measured UI load (single core, Apple Silicon, 48 kHz, 5 s offscreen `bench`)

| Page | 1.4.3 | 1.5.0 | Notes |
|---|---:|---:|---|
| Loudness | ~1.2 % | ~2.7 % | 60 Hz timer, but non-spectrum pages update at 30 Hz |
| Spectrum | ~1.7 % @ 20 fps | ~3.7 % @ 60 fps | 8192 FFT every frame + 60 Hz paint |
| Signal health | ~1.15 % | ~2.2 % | 2 Hz defect recomputation |
| Album (new page) | — | ~1.4 % | 5 Hz table/risk repaint |

The spectrum target is 60 fps, so the 3.7 % figure is the intended trade-off. Other
pages are deliberately throttled (30 Hz state/repaint, 5 Hz advisor/rail, 2 Hz signal
defects, 5 fps band data while off the spectrum page). HOLD stops FFT work.

Reproduce:

```bash
MasterLensUiShot bench page0   # loudness
MasterLensUiShot bench page2   # spectrum — 60 fps
MasterLensUiShot bench page5   # album
MasterLensUiShot bench page6   # signal health
```

## Structural changes

- **60 Hz timer**: `startTimerHz(60)`; spectrum page runs analyzer + repaint every
  frame, every other page skips odd frames to preserve the 1.4.3 low-load behaviour.
- **No album copies on non-album pages**: `UiState` only carries the album snapshot /
  gate / diff when the album page is visible. 60 Hz copies of track strings/vectors
  were the main regression during development.
- **Spectrum basis**: band densities are absolute; deviations are computed only
  against a reference or the album median. Without a basis the deviation decays to
  zero and the UI says “no basis” — there is no pink-noise/absolute target curve.
- **Audition**: 12 s stereo history ring (written in `processBlock`), 5 ms fades,
  HP/LP biquads, RMS loudness match to the full-band window and peak protection.
  Inactive output is bit-transparent (tested). Audition is deliberately not applied
  while the engine analyses; the input remains the measurement source.
- **Album session**: a shared JSON file under
  `~/Library/Application Support/BerryStudio/MasterLens/album.json`, versioned with
  SHA-256 snapshot hashes, previous-track snapshots for diffs and a rules object.
  A `MASTERLENS_ALBUM_FILE` environment override is available for tests.
- **Proof**: JSON + TXT with schema/version/revision, per-track snapshots, median,
  rules, gate, ranked risks, diff and an evidence hash computed **without** the
  generation timestamp, so the same snapshot/rules produce the same evidence hash.

## Honest host limitations

- **DAW markers**: VST3 and AU in JUCE do not expose a portable “write marker to the
  host marker track” API. 1.5 therefore exports/imports a marker **CSV** (timecode,
  seconds, severity, text) for the DAW's marker table. The app does not pretend to
  write every host's proprietary marker object.
- **Automation**: reset on transport start runs on the audio thread and works without
  the editor. Capture / proof / marker commands need the editor open because the
  spectrum and file chooser live on the message thread. Host automation parameters:
  `autoResetOnStart`, `autoScanOnStop`, `autoReportOnStop`, `autoCommand`.

## Verification

- 46 offline DSP assertions: calibration, stereo/mono, true peak, spectrum bands,
  defects, delivery table, audition bypass/active/stop.
- `auval` passes for AU; VST3 vendor is BerryStudio.
- Album proof JSON generated from two captured test tracks; gate/diff/hashes verified.
