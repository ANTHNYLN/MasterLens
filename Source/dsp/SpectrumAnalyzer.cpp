#include "SpectrumAnalyzer.h"

#include <cstring>

namespace ml
{
namespace
{
    /** log10 for the display path; error < 0.001 dB, much cheaper than log10f. */
    inline float fastLog10 (float x) noexcept
    {
        if (x <= 1.0e-20f)
            return -100.0f;

        juce::uint32 bits;
        std::memcpy (&bits, &x, sizeof (bits));

        const int exponent = (int) ((bits >> 23) & 0xffu) - 127;
        bits = (bits & 0x007fffffu) | 0x3f800000u;

        float mantissa;
        std::memcpy (&mantissa, &bits, sizeof (mantissa));

        const float z = (mantissa - 1.0f) / (mantissa + 1.0f);
        const float z2 = z * z;

        const float logMantissa = 0.8685889638f * z
                                * (1.0f + z2 * (0.3333333433f
                                                + z2 * (0.2f + z2 * 0.1428571492f)));

        return 0.3010299957f * (float) exponent + logMantissa;
    }
} // namespace

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
    displayMagnitudes.assign ((size_t) numBins, kSilentDb);
    displayPeak.assign ((size_t) numBins, kSilentDb);
    instantPower.assign ((size_t) numBins, 0.0f);
    smoothLo.assign ((size_t) numBins, 1);
    smoothHi.assign ((size_t) numBins, 1);
    computeSmoothingWindows();
    powerPrefix.assign ((size_t) numBins + 1, 0.0);
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

    // The FFT writes the full complex output over fftData, so only the real
    // input half has to be prepared. (The previous code also cleared 2 x fftSize
    // floats every frame.)
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
        const float db = mag > 1.0e-5f ? 20.0f * fastLog10 (mag) : kSilentDb;

        instantPower[(size_t) i] = mag * mag;
        peak = juce::jmax (peak, db);

        auto& sm = magnitudes[(size_t) i];
        sm += (db - sm) * (db > sm ? attack : release);

        auto& ph = peakHold[(size_t) i];
        ph = juce::jmax (ph - peakDecay, sm);
    }

    framePeakDb = peak;
    buildDisplaySpectrum();

    if (peak > -70.0f)
    {
        for (int i = 0; i < numBins; ++i)
            longTermPower[(size_t) i] += instantPower[(size_t) i];

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

float SpectrumAnalyzer::smoothingOctavesAt (double hz) noexcept
{
    if (hz < 250.0)
        return 1.0f / 3.0f;

    if (hz < 4000.0)
        return 1.0f / 6.0f;

    return 1.0f / 12.0f;
}

void SpectrumAnalyzer::computeSmoothingWindows()
{
    for (int i = 0; i < numBins; ++i)
    {
        if (i == 0)
        {
            smoothLo[0] = smoothHi[0] = 1;
            continue;
        }

        const double f = (double) i * binHz;
        const double halfOct = (double) smoothingOctavesAt (f) * 0.5;
        const double ratio = std::pow (2.0, halfOct);

        int lo = (int) std::floor (f / ratio / binHz);
        int hi = (int) std::ceil (f * ratio / binHz);

        lo = juce::jlimit (1, numBins - 2, lo);
        hi = juce::jlimit (lo + 1, numBins - 1, hi);

        smoothLo[(size_t) i] = lo;
        smoothHi[(size_t) i] = hi;
    }
}

void SpectrumAnalyzer::buildDisplaySpectrum()
{
    if (numBins < 4 || (int) powerPrefix.size() != numBins + 1)
        return;

    // Power-domain fractional-octave averaging. The bin windows are prepared
    // once (computeSmoothingWindows); this path is executed 30 times per second,
    // so it avoids pow() per bin and uses the linear power already produced by
    // the FFT instead of converting dB back to power.
    powerPrefix[0] = 0.0;

    for (int i = 1; i < numBins; ++i)
        powerPrefix[(size_t) i] = powerPrefix[(size_t) (i - 1)] + (double) instantPower[(size_t) i];

    powerPrefix[(size_t) numBins] = powerPrefix[(size_t) (numBins - 1)];

    displayMagnitudes[0] = magnitudes[0];
    displayPeak[0] = peakHold[0];

    for (int i = 1; i < numBins - 1; ++i)
    {
        const int lo = smoothLo[(size_t) i];
        const int hi = smoothHi[(size_t) i];

        const double sum = powerPrefix[(size_t) (hi + 1)] - powerPrefix[(size_t) lo];
        const double mean = sum / (double) (hi - lo + 1);

        displayMagnitudes[(size_t) i] = mean > 1.0e-14
                                            ? 10.0f * fastLog10 ((float) mean)
                                            : kSilentDb;

        // A genuine tonal peak must not be lowered by the band average: restore
        // it when it clearly stands above the smoothed curve. Broadband ripple
        // stays smooth because it does not clear the 3 dB threshold.
        if (magnitudes[(size_t) i] >= magnitudes[(size_t) (i - 1)]
            && magnitudes[(size_t) i] >= magnitudes[(size_t) (i + 1)]
            && magnitudes[(size_t) i] > displayMagnitudes[(size_t) i] + 3.0f)
            displayMagnitudes[(size_t) i] = magnitudes[(size_t) i];

        // Peak hold only needs a local maximum; a full window maximum was one of
        // the most expensive parts of this function and is not visible after the
        // cubic curve interpolation.
        displayPeak[(size_t) i] = juce::jmax (peakHold[(size_t) (i - 1)],
                                               juce::jmax (peakHold[(size_t) i],
                                                           peakHold[(size_t) (i + 1)]));
    }

    // Last bin (Nyquist): no right-hand neighbour exists.
    {
        const int i = numBins - 1;
        const int lo = smoothLo[(size_t) i];
        const int hi = smoothHi[(size_t) i];
        const double sum = powerPrefix[(size_t) (hi + 1)] - powerPrefix[(size_t) lo];
        const double mean = sum / (double) (hi - lo + 1);

        displayMagnitudes[(size_t) i] = mean > 1.0e-14
                                            ? 10.0f * fastLog10 ((float) mean)
                                            : kSilentDb;
        displayPeak[(size_t) i] = juce::jmax (peakHold[(size_t) (i - 1)],
                                               peakHold[(size_t) i]);
    }
}

float SpectrumAnalyzer::interpolateBins (const std::vector<float>& data, double hz) const noexcept
{
    if (data.empty())
        return kSilentDb;

    const double bin = hz / binHz;

    if (bin <= 1.0)
        return data[1];

    if (bin >= (double) (numBins - 2))
        return data[(size_t) (numBins - 1)];

    const int i = (int) std::floor (bin);
    const float t = (float) (bin - (double) i);

    const float p0 = data[(size_t) juce::jmax (0, i - 1)];
    const float p1 = data[(size_t) i];
    const float p2 = data[(size_t) (i + 1)];
    const float p3 = data[(size_t) juce::jmin (numBins - 1, i + 2)];

    // Cubic Hermite between p1 and p2, clamped to avoid overshoot on spikes.
    const float v = 0.5f * ((2.0f * p1)
                             + (-p0 + p2) * t
                             + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t
                             + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t);

    const float lo = juce::jmin (p1, p2) - 1.0f;
    const float hi = juce::jmax (p1, p2) + 1.0f;
    return juce::jlimit (lo, hi, v);
}

float SpectrumAnalyzer::getMagnitudeAt (double hz) const noexcept
{
    return interpolateBins (displayMagnitudes, hz);
}

float SpectrumAnalyzer::getPeakAt (double hz) const noexcept
{
    return interpolateBins (displayPeak, hz);
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

    double power[numBands] {};
    int bins[numBands] {};

    // Single pass over the spectrum; the previous version scanned all bins once
    // per band (10 x 4097 comparisons every update).
    for (int k = 1; k < numBins; ++k)
    {
        const double f = (double) k * binHz;

        for (int b = 0; b < numBands; ++b)
        {
            const auto& info = bandTable[b];

            if (f >= (double) info.lo && f < (double) info.hi)
            {
                power[b] += (double) longTermPower[(size_t) k];
                ++bins[b];
                break;
            }
        }
    }

    float density[numBands] {};
    float sum = 0.0f;
    int valid = 0;

    for (int b = 0; b < numBands; ++b)
    {
        if (bins[b] == 0)
            continue;

        const auto& info = bandTable[b];
        const double bandDb = 10.0 * std::log10 (power[b] / (double) averagedFrames + 1.0e-14);
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
