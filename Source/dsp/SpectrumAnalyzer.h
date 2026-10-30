#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

#include "AnalysisEngine.h"

namespace ml
{
/** Single-producer / single-consumer ring of mono samples.

    The audio thread only ever writes one sample at a time; the UI thread copies
    the most recent window. A torn read at worst produces a single display glitch,
    which is acceptable for a visualiser and keeps the audio path allocation free.
*/
class SpectrumRing
{
public:
    void prepare (int sizePow2 = 32768)
    {
        data.assign ((size_t) sizePow2, 0.0f);
        mask = sizePow2 - 1;
        index.store (0, std::memory_order_relaxed);
    }

    void reset() noexcept
    {
        for (auto& s : data)
            s = 0.0f;

        index.store (0, std::memory_order_relaxed);
    }

    inline void write (float v) noexcept
    {
        const int i = index.load (std::memory_order_relaxed);
        data[(size_t) i] = v;
        index.store ((i + 1) & mask, std::memory_order_relaxed);
    }

    void readLatest (float* dest, int n) const noexcept
    {
        const int idx = index.load (std::memory_order_relaxed);
        const int size = mask + 1;

        for (int i = 0; i < n; ++i)
            dest[i] = data[(size_t) ((idx - n + i + size) & mask)];
    }

private:
    std::vector<float> data;
    std::atomic<int> index { 0 };
    int mask = 0;
};

//==============================================================================
struct BandInfo
{
    const char* name;
    const char* label;
    const char* labelEn;
    float lo;
    float hi;
};

/** 4096-point FFT based analyser with long-term tonal balance tracking.

    All FFT work happens on the message thread, so the audio callback never pays
    for it.
*/
class SpectrumAnalyzer
{
public:
    static constexpr int numBands = 10;

    void prepare (double sampleRate, int fftOrder = 13);
    void reset();

    /** Runs one analysis frame. Call from a 30 fps timer. */
    void update (const SpectrumRing& ring);

    void setSmoothing (float s) noexcept { smoothing = juce::jlimit (0.0f, 1.0f, s); }

    const float* getMagnitudes() const noexcept { return displayMagnitudes.data(); }
    const float* getPeakHold() const noexcept { return displayPeak.data(); }
    int getNumBins() const noexcept { return numBins; }
    double getBinHz() const noexcept { return binHz; }
    float getFramePeakDb() const noexcept { return framePeakDb; }

    /** Interpolated magnitude in dB at an arbitrary frequency. The spectrum is
        fractional-octave smoothed first (1/3 octave below 250 Hz, 1/6 below
        4 kHz, 1/12 above), so the low end draws as a continuous curve instead
        of FFT-bin stairs. */
    float getMagnitudeAt (double hz) const noexcept;

    /** Same interpolation for the peak-hold trace. */
    float getPeakAt (double hz) const noexcept;

    /** Smoothed octave width used at a given frequency (for documentation/UI). */
    static float smoothingOctavesAt (double hz) noexcept;

    /** Energy share of the four QC bands: 20-250 Hz, 250 Hz-2 kHz, 2-8 kHz, 8-20 kHz (%). */
    const std::array<float, 4>& getBandRatios() const noexcept { return bandRatios; }

    struct Resonance
    {
        float freq = 0.0f;
        float excessDb = 0.0f;
    };

    const std::vector<Resonance>& getResonances() const noexcept { return resonances; }

    /** Long-term averaged spectrum (dB per bin), used for reference comparison. */
    const std::vector<float>& getAverageSpectrumDb() const noexcept { return averageDb; }

    /** Optional reference spectrum (dB per bin, same size as numBins). */
    void setReferenceSpectrum (std::vector<float> refDb);
    void clearReference();
    bool hasReference() const noexcept { return referenceReady; }
    float getReferenceDeviationAt (double hz) const noexcept;
    const std::vector<float>& getReferenceDb() const noexcept { return referenceDb; }

    static const BandInfo& getBandInfo (int i) noexcept;
    const std::vector<float>& getBandDeviations() const noexcept { return deviations; }

private:
    void computeBandDeviations();
    void computeBandRatios();
    void computeResonances();
    void updateAverageSpectrum();
    void buildDisplaySpectrum();
    float interpolateBins (const std::vector<float>& data, double hz) const noexcept;

    std::unique_ptr<juce::dsp::FFT> fft;

    int fftSize = 8192;
    int numBins = 4097;
    double sampleRate = 48000.0;
    double binHz = 5.86;

    std::vector<float> window;
    std::vector<float> fftData;
    std::vector<float> magnitudes;
    std::vector<float> peakHold;
    std::vector<float> displayMagnitudes;
    std::vector<float> displayPeak;
    std::vector<double> powerPrefix;
    std::vector<float> longTermPower;
    std::vector<float> averageDb;
    std::vector<float> scratch;
    std::vector<float> deviations;
    std::vector<float> referenceDb;
    std::array<float, 4> bandRatios { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<Resonance> resonances;
    bool referenceReady = false;

    int averagedFrames = 0;
    int resonanceFrameCounter = 0;
    float framePeakDb = kSilentDb;
    float smoothing = 0.55f;
};
} // namespace ml
