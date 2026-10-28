#include "AnalysisEngine.h"

namespace ml
{
static constexpr double kPi = juce::MathConstants<double>::pi;
static constexpr float  kClipThreshold = 0.9995f;

//==============================================================================
void Biquad::designLowPass (double sampleRate, double freq, double q) noexcept
{
    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cw = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;

    b0 = ((1.0 - cw) * 0.5) / a0;
    b1 = (1.0 - cw) / a0;
    b2 = b0;
    a1 = (-2.0 * cw) / a0;
    a2 = (1.0 - alpha) / a0;
}

//==============================================================================
void TruePeakDetector::prepare (double sampleRate)
{
    juce::ignoreUnused (sampleRate);

    constexpr int total = phases * tapsPerPhase;
    const double center = (total - 1) * 0.5;

    double h[total] {};
    double sum = 0.0;

    for (int k = 0; k < total; ++k)
    {
        const double t = ((double) k - center) / (double) phases;
        const double sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (kPi * t) / (kPi * t);
        const double x = (double) k / (double) (total - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2.0 * kPi * x)
                                 + 0.14128 * std::cos (4.0 * kPi * x)
                                 - 0.01168 * std::cos (6.0 * kPi * x);
        h[k] = sinc * w;
        sum += h[k];
    }

    const double scale = (double) phases / juce::jmax (1.0e-9, sum);

    for (int p = 0; p < phases; ++p)
        for (int j = 0; j < tapsPerPhase; ++j)
            coeffs[p][j] = (float) (h[p + phases * j] * scale);

    reset();
}

void TruePeakDetector::reset() noexcept
{
    for (auto& s : history)
        s = 0.0f;

    head = 0;
}

//==============================================================================
void AnalysisEngine::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    channels = juce::jlimit (1, 2, numChannels);

    partialLength = juce::jmax (16, (int) std::round (fs * 0.1));

    // --- ITU-R BS.1770-4 K-weighting (libebur128 coefficient derivation) ------
    {
        const double f0 = 1681.974450955533;
        const double gainDb = 3.999843853973347;
        const double q = 0.7071752369554196;

        const double k = std::tan (kPi * f0 / fs);
        const double vh = std::pow (10.0, gainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;

        const double b0 = (vh + vb * k / q + k * k) / a0;
        const double b1 = 2.0 * (k * k - vh) / a0;
        const double b2 = (vh - vb * k / q + k * k) / a0;
        const double a1 = 2.0 * (k * k - 1.0) / a0;
        const double a2 = (1.0 - k / q + k * k) / a0;

        for (auto& f : kShelf)
            f.setCoefficients (b0, b1, b2, a1, a2);
    }

    {
        const double f0 = 38.13547087602444;
        const double q = 0.5003270373238773;
        const double k = std::tan (kPi * f0 / fs);
        const double a0 = 1.0 + k / q + k * k;

        const double a1 = 2.0 * (k * k - 1.0) / a0;
        const double a2 = (1.0 - k / q + k * k) / a0;

        for (auto& f : kHighPass)
            f.setCoefficients (1.0, -2.0, 1.0, a1, a2);
    }

    // --- 120 Hz Linkwitz-Riley low band for the mono-bass check --------------
    for (int ch = 0; ch < 2; ++ch)
    {
        lowPassA[ch].designLowPass (fs, 120.0, 0.7071067811865476);
        lowPassB[ch].designLowPass (fs, 120.0, 0.7071067811865476);
    }

    truePeak[0].prepare (fs);
    truePeak[1].prepare (fs);

    reset();
}

void AnalysisEngine::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        kShelf[ch].reset();
        kHighPass[ch].reset();
        lowPassA[ch].reset();
        lowPassB[ch].reset();
        truePeak[ch].reset();
    }

    partialSamples = 0;
    sumWeightedK = 0.0;
    sumLR = sumL2 = sumR2 = 0.0;
    sumMid = sumSide = 0.0;
    sumLowMid = sumLowSide = 0.0;
    partialMaxTruePeak = 0.0;
    partialMaxSamplePeak = 0.0;
    sumL = sumR = 0.0;

    for (int i = 0; i < 40; ++i)
    {
        dcRingL[i] = 0.0;
        dcRingR[i] = 0.0;
    }

    dcRingPos = 0;
    dcRingCount = 0;
    prevL = prevR = 0.0f;
    clickAverage = 0.0f;
    inClip = false;

    for (auto& e : partialRing)
        e = 0.0;

    ringWrite = 0;
    ringCount = 0;
    partialCounter = 0;

    for (auto& b : blockHist)
        b.store (0);
    for (auto& b : shortHist)
        b.store (0);
    shortHistCount.store (0);

    for (auto& v : shortCurve)
        v = kSilentDb;
    curveWrite.store (0);

    elapsedSamples = 0;
}

//==============================================================================
void AnalysisEngine::process (const juce::AudioBuffer<float>& buffer, Stats& stats, int numSamples)
{
    const int nCh = juce::jmin (2, buffer.getNumChannels());

    if (nCh <= 0 || numSamples <= 0)
        return;

    const bool isStereo = nCh > 1;
    stats.stereo = isStereo;

    const float* left = buffer.getReadPointer (0);
    const float* right = isStereo ? buffer.getReadPointer (1) : left;

    for (int n = 0; n < numSamples; ++n)
    {
        const float l = left[n];
        const float r = right[n];

        // --- K-weighting (high shelf -> high pass) ---------------------------
        const float kl = kHighPass[0].process (kShelf[0].process (l));
        const float kr = kHighPass[1].process (kShelf[1].process (r));

        sumWeightedK += isStereo ? (double) kl * kl + (double) kr * kr
                                 : (double) kl * kl;

        // --- true peak, 4x oversampled ---------------------------------------
        float tp = truePeak[0].process (l);
        if (isStereo)
            tp = juce::jmax (tp, truePeak[1].process (r));

        partialMaxTruePeak = juce::jmax (partialMaxTruePeak, (double) tp);

        const float sp = isStereo ? juce::jmax (std::abs (l), std::abs (r))
                                  : std::abs (l);
        partialMaxSamplePeak = juce::jmax (partialMaxSamplePeak, (double) sp);

        // --- correlation / stereo width --------------------------------------
        const double dl = (double) l, dr = (double) r;
        sumLR += dl * dr;
        sumL2 += dl * dl;
        sumR2 += dr * dr;

        const double mid  = 0.5 * (dl + dr);
        const double side = 0.5 * (dl - dr);
        sumMid  += mid * mid;
        sumSide += side * side;

        // --- low band (<= 120 Hz) stereo image -------------------------------
        const float lowL = lowPassB[0].process (lowPassA[0].process (l));
        const float lowR = lowPassB[1].process (lowPassA[1].process (r));
        const double lowMid  = 0.5 * ((double) lowL + (double) lowR);
        const double lowSide = 0.5 * ((double) lowL - (double) lowR);
        sumLowMid  += lowMid * lowMid;
        sumLowSide += lowSide * lowSide;

        // --- DC offset accumulators ------------------------------------------
        sumL += dl;
        sumR += dr;

        // --- sample clipping (one event per contiguous run) ------------------
        const bool clipped = sp >= kClipThreshold;

        if (clipped && ! inClip)
            stats.clipEvents.fetch_add (1);

        inClip = clipped;

        // --- clicks / edit points --------------------------------------------
        const float delta = juce::jmax (std::abs (l - prevL), std::abs (r - prevR));
        clickAverage += 0.0008f * (delta - clickAverage);

        if (delta > 0.55f && delta > 12.0f * clickAverage + 0.02f)
            stats.clickEvents.fetch_add (1);

        prevL = l;
        prevR = r;
    }

    partialSamples += numSamples;
    elapsedSamples += (juce::uint32) numSamples;
    stats.elapsed = (double) elapsedSamples / fs;

    if (partialSamples >= partialLength)
        finishPartialBlock (stats);
}

//==============================================================================
void AnalysisEngine::finishPartialBlock (Stats& stats)
{
    const double inv = 1.0 / (double) juce::jmax (1, partialSamples);
    const double energy = sumWeightedK * inv;

    partialRing[ringWrite] = energy;
    ringWrite = (ringWrite + 1) & 63;

    if (ringCount < 64)
        ++ringCount;

    auto meanOfLast = [this] (int count) -> double
    {
        if (count <= 0)
            return 0.0;

        double sum = 0.0;

        for (int i = 0; i < count; ++i)
            sum += partialRing[(ringWrite - 1 - i + 64) & 63];

        return sum / (double) count;
    };

    const int windowM = juce::jmin (4, ringCount);
    const int windowS = juce::jmin (30, ringCount);

    const float mDb = energyToLufs (meanOfLast (windowM));
    const float sDb = energyToLufs (meanOfLast (windowS));

    stats.momentary = mDb;
    stats.shortTerm = sDb;

    if (mDb > stats.maxMomentary.load())
        stats.maxMomentary = mDb;

    if (windowS >= 30 && sDb > stats.maxShortTerm.load())
        stats.maxShortTerm = sDb;

    if (sDb > -60.0f)
        stats.hasSignal = true;

    {
        const int cw = curveWrite.load (std::memory_order_relaxed);
        shortCurve[cw] = sDb;
        curveWrite.store ((cw + 1) & (curveSize - 1), std::memory_order_relaxed);
    }

    // --- stereo image, correlation, DC ---------------------------------------
    const double corrDenom = std::sqrt (juce::jmax (1.0e-30, sumL2 * sumR2));
    stats.correlation = corrDenom > 1.0e-12
                          ? (float) juce::jlimit (-1.0, 1.0, sumLR / corrDenom)
                          : 0.0f;

    stats.widthPct = (float) juce::jlimit (0.0, 200.0,
                                           100.0 * std::sqrt (sumSide / juce::jmax (1.0e-20, sumMid)));
    stats.lowSidePct = (float) juce::jlimit (0.0, 200.0,
                                             100.0 * std::sqrt (sumLowSide / juce::jmax (1.0e-20, sumLowMid)));
    // true DC = mean of the last 4 s of 100 ms block means (a 2 Hz one-pole
    // leaks far too much bass and produces false positives on low-heavy music)
    {
        const double invSamples = 1.0 / (double) juce::jmax (1, partialSamples);
        dcRingL[dcRingPos] = sumL * invSamples;
        dcRingR[dcRingPos] = sumR * invSamples;
        dcRingPos = (dcRingPos + 1) % 40;

        if (dcRingCount < 40)
            ++dcRingCount;

        double avgL = 0.0, avgR = 0.0;

        for (int i = 0; i < dcRingCount; ++i)
        {
            avgL += dcRingL[i];
            avgR += dcRingR[i];
        }

        avgL /= (double) juce::jmax (1, dcRingCount);
        avgR /= (double) juce::jmax (1, dcRingCount);

        stats.dcOffsetDb = juce::Decibels::gainToDecibels (
            (float) juce::jmax (std::abs (avgL), std::abs (avgR)), -100.0f);
    }

    // --- peaks ---------------------------------------------------------------
    if (partialMaxTruePeak > 0.0)
    {
        const float tpDb = juce::Decibels::gainToDecibels ((float) partialMaxTruePeak, -100.0f);

        if (tpDb > stats.truePeakDb.load())
            stats.truePeakDb = tpDb;

        if (partialMaxTruePeak > 1.0f)
            stats.oversCount.fetch_add (1);
    }

    if (partialMaxSamplePeak > 0.0)
    {
        const float spDb = juce::Decibels::gainToDecibels ((float) partialMaxSamplePeak, -100.0f);

        if (spDb > stats.samplePeakDb.load())
            stats.samplePeakDb = spDb;
    }

    // --- gated histograms ----------------------------------------------------
    auto toBin = [] (float lufs)
    {
        return juce::jlimit (0, histBins - 1, (int) std::round ((lufs - histMinDb) / histStepDb));
    };

    if (windowM >= 4 && mDb > histMinDb)
    {
        blockHist[toBin (mDb)].fetch_add (1);
        stats.integrated = computeIntegrated();
    }

    ++partialCounter;

    if (windowS >= 30 && (partialCounter % 10) == 0 && sDb > histMinDb)
    {
        shortHist[toBin (sDb)].fetch_add (1);
        shortHistCount.fetch_add (1);
        stats.lra = computeLra();
    }

    // --- reset per-partial accumulators --------------------------------------
    partialSamples = 0;
    sumWeightedK = 0.0;
    sumLR = sumL2 = sumR2 = 0.0;
    sumMid = sumSide = 0.0;
    sumLowMid = sumLowSide = 0.0;
    sumL = sumR = 0.0;
    partialMaxTruePeak = 0.0;
    partialMaxSamplePeak = 0.0;
}

//==============================================================================
float AnalysisEngine::computeIntegrated() const
{
    double totalEnergy = 0.0, totalCount = 0.0;

    for (int i = 0; i < histBins; ++i)
    {
        const auto count = blockHist[i].load();

        if (count == 0)
            continue;

        totalEnergy += (double) count * lufsToEnergy (histMinDb + (double) i * histStepDb);
        totalCount += (double) count;
    }

    if (totalCount < 1.0)
        return kSilentDb;

    // relative gate: -10 LU below the absolute-gated mean
    const double relThreshold = (totalEnergy / totalCount) * 0.1;

    double gatedEnergy = 0.0, gatedCount = 0.0;

    for (int i = 0; i < histBins; ++i)
    {
        const auto count = blockHist[i].load();

        if (count == 0)
            continue;

        const double e = lufsToEnergy (histMinDb + (double) i * histStepDb);

        if (e >= relThreshold)
        {
            gatedEnergy += (double) count * e;
            gatedCount += (double) count;
        }
    }

    if (gatedCount < 1.0)
        return kSilentDb;

    return energyToLufs (gatedEnergy / gatedCount);
}

float AnalysisEngine::computeLra() const
{
    if (shortHistCount.load() < 2)
        return 0.0f;

    double totalEnergy = 0.0, totalCount = 0.0;

    for (int i = 0; i < histBins; ++i)
    {
        const auto count = shortHist[i].load();

        if (count == 0)
            continue;

        totalEnergy += (double) count * lufsToEnergy (histMinDb + (double) i * histStepDb);
        totalCount += (double) count;
    }

    if (totalCount < 2.0)
        return 0.0f;

    // relative gate: -20 LU below the mean short-term loudness
    const double relThreshold = (totalEnergy / totalCount) * 0.01;

    double gatedCount = 0.0;

    for (int i = 0; i < histBins; ++i)
    {
        const auto count = shortHist[i].load();

        if (count == 0)
            continue;

        if (lufsToEnergy (histMinDb + (double) i * histStepDb) >= relThreshold)
            gatedCount += (double) count;
    }

    if (gatedCount < 2.0)
        return 0.0f;

    const double lowTarget = gatedCount * 0.10;
    const double highTarget = gatedCount * 0.95;

    double accum = 0.0;
    float lowL = kSilentDb, highL = kSilentDb;
    bool haveLow = false, haveHigh = false;

    for (int i = 0; i < histBins; ++i)
    {
        const auto count = shortHist[i].load();

        if (count == 0)
            continue;

        if (lufsToEnergy (histMinDb + (double) i * histStepDb) < relThreshold)
            continue;

        accum += (double) count;
        const float l = histMinDb + (float) i * histStepDb;

        if (! haveLow && accum >= lowTarget)
        {
            lowL = l;
            haveLow = true;
        }

        if (accum >= highTarget)
        {
            highL = l;
            haveHigh = true;
            break;
        }
    }

    if (! haveLow || ! haveHigh)
        return 0.0f;

    return juce::jmax (0.0f, highL - lowL);
}

//==============================================================================
void AnalysisEngine::copyRecentShortTerm (std::vector<float>& dest, int maxPoints) const
{
    dest.clear();

    const int total = juce::jlimit (0, curveSize, maxPoints);
    const int w = curveWrite.load (std::memory_order_relaxed);

    dest.reserve ((size_t) total);

    for (int i = total - 1; i >= 0; --i)
        dest.push_back (shortCurve[(w - 1 - i + curveSize) & (curveSize - 1)]);
}

} // namespace ml



