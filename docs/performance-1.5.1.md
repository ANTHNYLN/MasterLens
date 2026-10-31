# MasterLens 1.5.1 — live solo, compact window, genre target & delivery requirements

## Why this release exists

1.5.0 introduced risk audition but looped the recent 12 s history. That is wrong for
"listen to this layer while the DAW plays": the loop keeps repeating old audio instead
of following the playhead. 1.5.1 adds a real **live** audition path, keeps looping only
for explicit timestamped problem regions, fits the editor on a MacBook Air at default
display scaling, and restores genre loudness references as a separate target module.

## Measured UI load (single core, Apple Silicon, 48 kHz, 5 s offscreen `bench`)

| Page | 1.5.0 | 1.5.1 | Notes |
|---|---:|---:|---|
| Loudness | ~2.7 % | ~1.3 % | 30 Hz state updates on non-spectrum pages |
| Spectrum | ~3.7 % | ~3.0 % | 8192 FFT + 60 Hz paint |
| Signal health | ~2.2 % | ~1.5 % | 2 Hz defect recomputation |
| Genre target / delivery (new) | — | ~1.4 % | 5 Hz table refresh |

Reproduce:

```bash
MasterLensUiShot bench page0   # loudness
MasterLensUiShot bench page2   # spectrum — 60 fps
MasterLensUiShot bench page6   # signal health
MasterLensUiShot bench page9   # genre target / delivery
```

## Live audition

- `AuditionEngine` now has two modes: `live` and `loop`.
- **Live** filters the current `processBlock` input with the selected HP/LP pair,
  follows the host playhead, applies a 5 ms fade-in and a smoothed RMS/peak gain.
  It never reads the history ring. A below -120 dBFS input block clears the filter
  state and output, so a stopped / paused DAW is silent immediately.
- **Loop** is still used for timestamped problem regions (2 s window) and album risk
  rows. It retains the old loudness-matched history slice.
- Inactive output remains bit-transparent; the existing bypass/active/stop tests still
  pass.

## Timestamped problem regions

- The signal-health page tracks six layers: distortion, aliasing, sub build-up,
  transient, mono fold and masking.
- When a layer crosses its threshold (55 risk, 45 for mono), the editor records a
  2 s region ending at the detection time, with 6 s cooldown per layer. Regions are
  capped at 24, written as category-6 timeline markers, and shown as clickable
  timestamp chips under the six cards.
- Clicking a **card** starts live solo; clicking a **timestamp** loops that region.
  The footer states which mode is active; Esc stops either.

## Compact window / DPI

- Base minimum is **880 × 560** (was 900 × 620); default is **1120 × 700**
  (was 1180 × 760).
- First launch chooses the largest scale step whose 880 × 560 layout fits the
  display work area minus a 36 px title-bar allowance.
- A stored 150 % / 200 % scale from an older session is clamped to the largest
  fitting step on the current display, so a small laptop never opens an off-screen
  window.
- 10 rail rows use a compact two-line layout below 34 px row height; signal cards
  switch to a title / value / method layout below 96 px card height.

## Genre target / delivery module

- Page 9 ("曲风目标 / 交付") restores the 36 static genre/platform references from
  `Source/Presets.h`.
- The participation slider maps **0 % → quietest observed release**, **50 % → genre
  reference**, **100 % → loudest observed release**; target LUFS and suggested LRA
  are interpolated from the static `GenreWar.h` table.
- "应用为目标" writes target LUFS / TP ceiling / LRA goal and switches to Manual
  reference. It is never applied implicitly.
- The platform table lists Spotify, Apple Music, YouTube, Tidal, Amazon, SoundCloud,
  podcast and club targets, codec, normalisation behaviour and true-peak ceiling.
- **Apple tail check**: trailing digital silence at -60 dBFS is measured on the audio
  thread. Apple Digital Masters is shown as `TAIL?` until silence is actually
  measured, then `TAIL` if < 1.0 s or pass when >= 1.0 s. This avoids a false failure
  while the song is still playing.

## Honest limits

- **Apple tail measurement needs the host to deliver the tail.** If the DAW stops
  processing the plug-in at the last sample instead of sending the silent tail, the
  plug-in cannot measure it. Play through the tail / use the exported file check.
- **Platform requirements are static delivery references**, not an Apple/Spotify
  certification. Verify the final distributor's current spec; Apple requires a 24-bit
  source and no clipping, which this plug-in cannot inspect from a live bus.
- **DAW marker write-back** remains CSV exchange; VST3/AU has no portable host marker
  API.
- **Genre targets are references**; the platform mode remains the default and no fixed
  genre loudness is silently used as a pass line.

## Verification

- `50` offline DSP assertions pass, including live-audition follow, stopped-DAW silence,
  tail-silence measurement, bypass, sub-share, mono-fold, flat-top and true-peak.
- `auval` passes; VST3 vendor is BerryStudio; version is **1.5.1**.
- Offscreen screenshots checked at 880 × 560 and 1280 × 820 for the signal, target and
  existing pages.
