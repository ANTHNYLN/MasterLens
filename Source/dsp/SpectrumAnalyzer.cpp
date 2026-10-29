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
    averageDb.assign ((size_t) numBins, kSilentDb);
    referenceDb.assign ((size_t) numBins, 0.0f);
    deviations.assign ((size_t) numBands, 0.0f);
    resonances.clear();
    referenceReady = false;

    reset();
}

void SpectrumAnalyzer::reset()
{
    magnitudes.assign ((size_t) numBins, kSilentDb);
    peakHold.assign ((size_t) numBins, kSilentDb);
    std::fill (longTermPower.begin(), longTermPower.end(), 0.0f);
    std::fill (averageDb.begin(), averageDb.end(), kSilentDb);
    std::fill (deviations.begin(), deviations.end(), 0.0f);
    resonances.clear();
    bandRatios = { 0.0f, 0.0f, 0.0f, 0.0f };
    averagedFrames = 0;
    resonanceFrameCounter = 0;
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

        if ((++resonanceFrameCounter % 15) == 0)
        {
            updateAverageSpectrum();
            computeBandDeviations();
            computeBandRatios();
            computeResonances();
        }
    }
}

void SpectrumAnalyzer::updateAverageSpectrum()
{
    if (averagedFrames < 20)
        return;

    const float inv = 1.0f / (float) averagedFrames;

    for (int i = 0; i < numBins; ++i)
        averageDb[(size_t) i] = juce::Decibels::gainToDecibels (longTermPower[(size_t) i] * inv, kSilentDb);
}

float SpectrumAnalyzer::getMagnitudeAt (double hz) const noexcept
{
    if (magnitudes.empty())
        return kSilentDb;

    const double bin = hz / binHz;

    if (bin <= 1.0)
        return magnitudes[1];

    if (bin >= (double) (numBins - 2))
        return magnitudes[(size_t) (numBins - 1)];

    // Linear interpolation between the two neighbouring bins removes the
    // staircase look of a log-frequency plot with few low-frequency bins.
    const int i0 = (int) std::floor (bin);
    const float t = (float) (bin - (double) i0);

    return magnitudes[(size_t) i0] * (1.0f - t) + magnitudes[(size_t) (i0 + 1)] * t;
}

float SpectrumAnalyzer::getReferenceDeviationAt (double hz) const noexcept
{
    if (! referenceReady)
        return 0.0f;

    const int bin = juce::jlimit (1, numBins - 1, (int) std::round (hz / binHz));
    const float ref = referenceDb[(size_t) bin];

    if (ref <= -99.0f)
        return 0.0f;

    return averageDb[(size_t) bin] - ref;
}

void SpectrumAnalyzer::setReferenceSpectrum (std::vector<float> refDb)
{
    if ((int) refDb.size() != numBins)
        return;

    referenceDb = std::move (refDb);
    referenceReady = true;
}

void SpectrumAnalyzer::clearReference()
{
    referenceReady = false;
    std::fill (referenceDb.begin(), referenceDb.end(), 0.0f);
}

void SpectrumAnalyzer::computeBandRatios()
{
    if (averagedFrames < 20)
        return;

    const double edges[5] { 20.0, 250.0, 2000.0, 8000.0, 20000.0 };
    double power[4] {};
    double total = 0.0;

    for (int i = 1; i < numBins; ++i)
    {
        const double f = (double) i * binHz;

        if (f < edges[0] || f > edges[4])
            continue;

        const double p = (double) longTermPower[(size_t) i];

        for (int b = 0; b < 4; ++b)
        {
            if (f >= edges[b] && f < edges[b + 1])
            {
                power[b] += p;
                total += p;
                break;
            }
        }
    }

    if (total <= 0.0)
        return;

    for (int b = 0; b < 4; ++b)
        bandRatios[(size_t) b] = (float) (100.0 * power[b] / total);
}

void SpectrumAnalyzer::computeResonances()
{
    resonances.clear();

    if (averagedFrames < 40)
        return;

    struct Peak { float db; int bin; };
    std::vector<Peak> found;

    const int lowBin = juce::jmax (2, (int) std::round (60.0 / binHz));
    const int highBin = juce::jmin (numBins - 2, (int) std::round (16000.0 / binHz));

    for (int i = lowBin; i < highBin; ++i)
    {
        const float db = averageDb[(size_t) i];

        if (db <= -70.0f)
            continue;

        // local median over a +/- 1/6 octave window (excluding the centre bin)
        const int span = juce::jmax (2, (int) std::round (i * 0.12));   // ~1/6 octave
        float sum = 0.0f;
        int count = 0;

        for (int j = juce::jmax (1, i - span); j <= juce::jmin (numBins - 2, i + span); ++j)
        {
            if (j == i)
                continue;

            sum += averageDb[(size_t) j];
            ++count;
        }

        if (count < 4)
            continue;

        const float neighbour = sum / (float) count;

        if (db - neighbour < 6.0f)
            continue;

        const bool isLocalMax = db >= averageDb[(size_t) (i - 1)] && db >= averageDb[(size_t) (i + 1)];

        if (! isLocalMax)
            continue;

        found.push_back ({ db - neighbour, i });
    }

    std::sort (found.begin(), found.end(), [] (const Peak& a, const Peak& b) { return a.db > b.db; });

    for (size_t i = 0; i < found.size() && i < 6; ++i)
        resonances.push_back ({ (float) (found[i].bin * binHz), found[i].db });
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
