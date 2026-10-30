#pragma once

#include <vector>

#include "AnalysisEngine.h"

namespace ml
{
/** Objective signal-integrity readings derived from the averaged spectrum plus
    the time-domain counters in StatsSnapshot.

    These are consequence indicators, not a claim of sample-accurate THD: the
    plug-in never knows the source material. Harmonic / intermodulation figures
    are measured as energy above a local spectral baseline, flat-top density is
    a direct waveform measurement, mono fold loss is computed from real mid and
    channel energies, and the masking figure compares low-mid density with the
    1-4 kHz presence band.
*/
struct SignalDefects
{
    bool ready = false;

    // distortion / nonlinearity
    float harmonicExcessDb = 0.0f;   // 2nd-5th harmonic energy above local floor
    float imdSidebandDb = 0.0f;     // sum / difference sidebands around strong tones
    float flatTopPct = 0.0f;        // % of samples sitting on a flat top
    float distortionRisk = 0.0f;    // 0-100

    // aliasing / top-octave integrity
    float hfTiltDb = 0.0f;          // 16-20 kHz vs 10-16 kHz density
    float aliasExcessDb = 0.0f;     // above the expected natural roll-off
    float aliasRisk = 0.0f;         // 0-100

    // sub-bass accumulation
    float subSharePct = 0.0f;       // < 40 Hz share of total energy
    float subTiltDb = 0.0f;         // 20-40 Hz vs 40-80 Hz density
    float subRisk = 0.0f;           // 0-100

    // transient integrity
    float transientScore = 100.0f;  // 0-100
    float crestLossDb = 0.0f;       // how far PLR sits below a 14 dB reference

    // mono fold
    float monoFoldLossDb = 0.0f;
    float monoRetainPct = 100.0f;
    float monoLowRetainPct = 100.0f;
    float monoRisk = 0.0f;

    // mid masking
    float maskingDb = 0.0f;         // low-mid density over 1-4 kHz presence
    float maskingRisk = 0.0f;       // 0-100

    int attributionCount = 0;       // number of layers over their risk threshold

    /** Highest of the six normalised risk scores (0-100). */
    float worstRisk() const noexcept
    {
        return juce::jmax (juce::jmax (distortionRisk, aliasRisk),
                           juce::jmax (juce::jmax (subRisk, maskingRisk),
                                       juce::jmax (monoRisk, 100.0f - transientScore)));
    }
};

/** @param spectrumDb  long-term averaged spectrum, dB per bin (SpectrumAnalyzer)
    @param binHz       bin width of that spectrum
    @param stats       live time-domain counters
*/
SignalDefects analyseSignalDefects (const std::vector<float>& spectrumDb,
                                    double binHz,
                                    const StatsSnapshot& stats);

} // namespace ml
