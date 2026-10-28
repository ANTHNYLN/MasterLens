#pragma once

#include <juce_dsp/juce_dsp.h>
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

    void prepare (double sampleRate, int fftOrder = 12);
    void reset();

    /** Runs one analysis frame. Call from a 60 fps timer. */
    void update (const SpectrumRing& ring);

    void setSmoothing (float s) noexcept { smoothing = juce::jlimit (0.0f, 1.0f, s); }

    const float* getMagnitudes() const noexcept { return magnitudes.data(); }
    const float* getPeakHold() const noexcept { return peakHold.data(); }
    int getNumBins() const noexcept { return numBins; }
    double getBinHz() const noexcept { return binHz; }
    float getFramePeakDb() const noexcept { return framePeakDb; }

    static const BandInfo& getBandInfo (int i) noexcept;
    const std::vector<float>& getBandDeviations() const noexcept { return deviations; }

private:
    void computeBandDeviations();

    std::unique_ptr<juce::dsp::FFT> fft;

    int fftSize = 4096;
    int numBins = 2049;
    double sampleRate = 48000.0;
    double binHz = 11.7;

    std::vector<float> window;
    std::vector<float> fftData;
    std::vector<float> magnitudes;
    std::vector<float> peakHold;
    std::vector<float> longTermPower;
    std::vector<float> scratch;
    std::vector<float> deviations;

    int averagedFrames = 0;
    float framePeakDb = kSilentDb;
    float smoothing = 0.55f;
};
} // namespace ml
