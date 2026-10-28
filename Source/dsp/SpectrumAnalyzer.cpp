#include "SpectrumAnalyzer.h"

namespace ml
{
static const BandInfo bandTable[SpectrumAnalyzer::numBands] =
{
    { "Sub",       "超低",   "Sub",        20.0f,    60.0f },
    { "Bass",      "低频",   "Bass",       60.0f,   120.0f },
    { "Low Mid",   "低中",   "Low-mid",   120.0f,   250.0f },
    { "Mud",       "浊声",   "Mud",       250.0f,   500.0f },
    { "Mid",       "中频",   "Mid",       500.0f,  1000.0f },
    { "Upper Mid", "中高",   "Upper-mid", 1000.0f, 2000.0f },
    { "Presence",  "明亮",   "Presence",  2000.0f, 5000.0f },
    { "Sibilance", "齿音",   "Sibilance", 5000.0f, 9000.0f },
    { "Air",       "空气",   "Air",       9000.0f, 16000.0f },
    { "Ultra",     "极高频", "Ultra",     16000.0f, 20000.0f }
};

const BandInfo& SpectrumAnalyzer::getBandInfo (int i) noexcept
{
    return bandTable[juce::jlimit (0, numBands - 1, i)];
}

//==============================================================================
void SpectrumAnalyzer::prepare (double newSampleRate, int fftOrder)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    fftSize = 1 << fftOrder;
    numBins = fftSize / 2 + 1;
    binHz = sampleRate / (double) fftSize;

    window.resize ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi
                                                             * (double) i / (double) (fftSize - 1));

    fftData.assign ((size_t) fftSize * 2, 0.0f);
    scratch.resize ((size_t) fftSize);
    longTermPower.assign ((size_t) numBins, 0.0f);
    deviations.assign ((size_t) numBands, 0.0f);

    reset();
}

void SpectrumAnalyzer::reset()
{
    magnitudes.assign ((size_t) numBins, kSilentDb);
    peakHold.assign ((size_t) numBins, kSilentDb);
    std::fill (longTermPower.begin(), longTermPower.end(), 0.0f);
    std::fill (deviations.begin(), deviations.end(), 0.0f);
    averagedFrames = 0;
    framePeakDb = kSilentDb;
}

//==============================================================================
void SpectrumAnalyzer::update (const SpectrumRing& ring)
{
    if (fft == nullptr || scratch.empty())
        return;

    ring.readLatest (scratch.data(), fftSize);

    std::fill (fftData.begin(), fftData.end(), 0.0f);

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = scratch[(size_t) i] * window[(size_t) i];

    fft->performFrequencyOnlyForwardTransform (fftData.data(), true);

    // A full-scale sine peaks at 0 dBFS after this normalisation.
    const float scale = 4.0f / (float) fftSize;

    const float attack  = juce::jlimit (0.05f, 1.0f, 0.85f - 0.55f * smoothing);
    const float release = juce::jlimit (0.02f, 0.8f, 0.32f - 0.25f * smoothing);
    constexpr float peakDecay = 0.15f;

    float peak = kSilentDb;

    for (int i = 0; i < numBins; ++i)
    {
        const float mag = fftData[(size_t) i] * scale;
        const float db = juce::Decibels::gainToDecibels (mag, kSilentDb);

        peak = juce::jmax (peak, db);

        auto& sm = magnitudes[(size_t) i];
        sm += (db - sm) * (db > sm ? attack : release);

        auto& ph = peakHold[(size_t) i];
        ph = juce::jmax (ph - peakDecay, sm);
    }

    framePeakDb = peak;

    if (peak > -70.0f)
    {
        for (int i = 1; i < numBins; ++i)
        {
            const float mag = fftData[(size_t) i] * scale;
            longTermPower[(size_t) i] += mag * mag;
        }

        ++averagedFrames;
        computeBandDeviations();
    }
}

void SpectrumAnalyzer::computeBandDeviations()
{
    if (averagedFrames < 20)
        return;

    float density[numBands] {};
    float sum = 0.0f;
    int valid = 0;

    for (int b = 0; b < numBands; ++b)
    {
        const auto& info = bandTable[b];
        double power = 0.0;
        int bins = 0;

        for (int k = 1; k < numBins; ++k)
        {
            const double f = (double) k * binHz;

            if (f >= (double) info.lo && f < (double) info.hi)
            {
                power += (double) longTermPower[(size_t) k];
                ++bins;
            }
        }

        if (bins == 0)
            continue;

        const double bandDb = 10.0 * std::log10 (power / (double) averagedFrames + 1.0e-14);
        const double octaves = std::log2 ((double) info.hi / (double) info.lo);

        density[b] = (float) (bandDb - 10.0 * std::log10 (octaves));
        sum += density[b];
        ++valid;
    }

    if (valid < 3)
        return;

    const float mean = sum / (float) valid;

    for (int b = 0; b < numBands; ++b)
    {
        if (density[b] == 0.0f)
            continue;

        const float target = density[b] - mean;
        deviations[(size_t) b] += (target - deviations[(size_t) b]) * 0.15f;
    }
}
} // namespace ml
