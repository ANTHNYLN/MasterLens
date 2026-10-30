#include "DefectAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace ml
{
namespace
{
    constexpr float kFloorDb = -99.0f;

    float valueAt (const std::vector<float>& spec, double binHz, double hz)
    {
        if (spec.empty() || binHz <= 0.0)
            return kFloorDb;

        const double bin = hz / binHz;
        const int last = (int) spec.size() - 1;

        if (bin <= 1.0)
            return spec[1];

        if (bin >= (double) (last - 1))
            return spec[(size_t) last];

        const int i = (int) std::floor (bin);
        const float t = (float) (bin - (double) i);

        return spec[(size_t) i] * (1.0f - t) + spec[(size_t) (i + 1)] * t;
    }

    float medianAround (const std::vector<float>& spec, double binHz, double hz,
                        double octaves, double excludeHz = 0.0)
    {
        std::vector<float> values;
        values.reserve (32);

        const int last = (int) spec.size() - 1;
        const double lo = hz * std::pow (2.0, -octaves * 0.5);
        const double hi = hz * std::pow (2.0, octaves * 0.5);

        for (int i = 1; i < last; ++i)
        {
            const double f = (double) i * binHz;

            if (f < lo || f > hi)
                continue;

            if (excludeHz > 0.0 && std::abs (f - hz) <= excludeHz)
                continue;

            values.push_back (spec[(size_t) i]);
        }

        if (values.empty())
            return kFloorDb;

        const size_t mid = values.size() / 2;
        std::nth_element (values.begin(), values.begin() + (long) mid, values.end());
        return values[mid];
    }

    /** Energy density per octave between lo and hi, in dB. */
    float bandDensity (const std::vector<float>& spec, double binHz, double lo, double hi)
    {
        if (spec.empty() || hi <= lo)
            return kFloorDb;

        double power = 0.0;
        int count = 0;

        for (int i = 1; i < (int) spec.size(); ++i)
        {
            const double f = (double) i * binHz;

            if (f >= lo && f < hi)
            {
                power += std::pow (10.0, (double) spec[(size_t) i] * 0.1);
                ++count;
            }
        }

        if (count == 0 || power <= 1.0e-20)
            return kFloorDb;

        const double octaves = std::log2 (hi / lo);
        return (float) (10.0 * std::log10 (power / (double) count))
             - (float) (10.0 * std::log10 (juce::jmax (1.0e-6, octaves)));
    }

    struct Peak
    {
        float db = kFloorDb;
        double hz = 0.0;
    };

    std::vector<Peak> findTonalPeaks (const std::vector<float>& spec, double binHz,
                                      double loHz, double hiHz, int maxCount)
    {
        std::vector<Peak> found;

        if (spec.size() < 32)
            return found;

        const int lo = juce::jmax (2, (int) std::round (loHz / binHz));
        const int hi = juce::jmin ((int) spec.size() - 2, (int) std::round (hiHz / binHz));

        for (int i = lo; i <= hi; ++i)
        {
            const float db = spec[(size_t) i];

            if (db <= -70.0f || db < spec[(size_t) (i - 1)] || db < spec[(size_t) (i + 1)])
                continue;

            const double f = (double) i * binHz;

            if (db - medianAround (spec, binHz, f, 1.0 / 3.0, binHz * 3.0) < 6.0f)
                continue;

            bool tooClose = false;

            for (const auto& p : found)
                if (std::abs (p.hz - f) < p.hz * 0.03)
                    tooClose = true;

            if (! tooClose)
                found.push_back ({ db, f });
        }

        std::sort (found.begin(), found.end(),
                   [] (const Peak& a, const Peak& b) { return a.db > b.db; });

        if ((int) found.size() > maxCount)
            found.resize ((size_t) maxCount);

        return found;
    }
} // namespace

    float harmonicExcess (const std::vector<float>& spec, double binHz,
                          const std::vector<Peak>& peaks)
    {
        float best = 0.0f;

        for (const auto& p : peaks)
        {
            float total = 0.0f;
            int count = 0;

            for (int h = 2; h <= 5; ++h)
            {
                const double f = p.hz * (double) h;

                if (f < 30.0 || f > 18000.0)
                    continue;

                const float value = valueAt (spec, binHz, f);
                const float baseline = medianAround (spec, binHz, f, 1.0 / 3.0, binHz * 2.0);

                if (value <= -90.0f || baseline <= -90.0f)
                    continue;

                const float excess = value - baseline;

                if (excess > 1.5f)
                {
                    total += juce::jlimit (0.0f, 18.0f, excess);
                    ++count;
                }
            }

            if (count > 0)
                best = juce::jmax (best, total / (float) count);
        }

        return juce::jlimit (0.0f, 24.0f, best);
    }

    float imdExcess (const std::vector<float>& spec, double binHz,
                     const std::vector<Peak>& peaks)
    {
        const Peak* bass = nullptr;
        const Peak* mid = nullptr;

        for (const auto& p : peaks)
        {
            if (p.hz >= 40.0 && p.hz <= 250.0 && (bass == nullptr || p.db > bass->db))
                bass = &p;

            if (p.hz >= 400.0 && p.hz <= 4000.0 && (mid == nullptr || p.db > mid->db))
                mid = &p;
        }

        if (bass == nullptr || mid == nullptr || mid->hz < bass->hz * 3.0)
            return 0.0f;

        float worst = 0.0f;

        const double products[3] = { mid->hz - bass->hz, mid->hz + bass->hz,
                                     2.0 * bass->hz - mid->hz };

        for (double f : products)
        {
            if (f < 30.0 || f > 18000.0)
                continue;

            const float value = valueAt (spec, binHz, f);
            const float baseline = medianAround (spec, binHz, f, 1.0 / 6.0, binHz * 2.0);

            if (value > -90.0f && baseline > -90.0f)
                worst = juce::jmax (worst, value - baseline);
        }

        return juce::jlimit (0.0f, 24.0f, worst);
    }

    /** Spectral flatness (0 = tonal, 1 = flat noise) of a band, from power. */
    float bandFlatness (const std::vector<float>& spec, double binHz, double lo, double hi)
    {
        double logSum = 0.0, linSum = 0.0;
        int count = 0;

        for (int i = 1; i < (int) spec.size(); ++i)
        {
            const double f = (double) i * binHz;

            if (f < lo || f >= hi)
                continue;

            const double p = std::pow (10.0, (double) spec[(size_t) i] * 0.1) + 1.0e-20;
            logSum += std::log10 (p);
            linSum += p;
            ++count;
        }

        if (count < 2 || linSum <= 0.0)
            return 1.0f;

        const double geometric = std::pow (10.0, logSum / (double) count);
        const double arithmetic = linSum / (double) count;

        return (float) juce::jlimit (0.0, 1.0, geometric / arithmetic);
    }

//==============================================================================
SignalDefects analyseSignalDefects (const std::vector<float>& spectrumDb,
                                    double binHz,
                                    const StatsSnapshot& stats)
{
    SignalDefects d;
    d.flatTopPct = juce::jlimit (0.0f, 100.0f, stats.flatTopPct);
    d.monoFoldLossDb = juce::jlimit (0.0f, 30.0f, stats.monoFoldLossDb);
    d.monoRetainPct = juce::jlimit (0.0f, 100.0f, stats.monoRetainPct);
    d.monoLowRetainPct = juce::jlimit (0.0f, 100.0f, stats.monoLowRetainPct);
    d.subSharePct = juce::jlimit (0.0f, 100.0f, stats.subSharePct);

    if (! stats.hasSignal || spectrumDb.size() < 32 || binHz <= 0.0)
    {
        d.ready = false;
        return d;
    }

    const auto peaks = findTonalPeaks (spectrumDb, binHz, 50.0, 2000.0, 4);

    d.harmonicExcessDb = harmonicExcess (spectrumDb, binHz, peaks);
    d.imdSidebandDb = imdExcess (spectrumDb, binHz, peaks);

    d.hfTiltDb = bandDensity (spectrumDb, binHz, 16000.0, 20000.0)
               - bandDensity (spectrumDb, binHz, 10000.0, 16000.0);
    d.aliasExcessDb = d.hfTiltDb + 3.0f;   // allow a naturally bright top octave

    const float hfFlatness = bandFlatness (spectrumDb, binHz, 15000.0, 20000.0);

    // Aliasing shows up as a bright *and* flat / noisy top octave. Bright but
    // tonal material (airy mixes, cymbals) must not be flagged as aliasing.
    d.aliasRisk = juce::jlimit (0.0f, 100.0f,
                                (d.hfTiltDb - 2.0f) * 9.0f
                                + hfFlatness * 45.0f - 8.0f);

    d.subTiltDb = bandDensity (spectrumDb, binHz, 20.0, 40.0)
                - bandDensity (spectrumDb, binHz, 40.0, 80.0);
    d.subRisk = juce::jlimit (0.0f, 100.0f,
                              juce::jmax (0.0f, d.subTiltDb - 4.0f) * 6.0f
                              + juce::jmax (0.0f, d.subSharePct - 20.0f) * 2.5f);

    const float plr = stats.getPlr();
    d.crestLossDb = juce::jmax (0.0f, 14.0f - plr);
    d.transientScore = juce::jlimit (0.0f, 100.0f,
                                     100.0f - d.crestLossDb * 4.5f
                                            - juce::jmax (0.0f, 12.0f - stats.psr) * 2.0f
                                            - d.flatTopPct * 250.0f);

    d.monoRisk = juce::jlimit (0.0f, 100.0f, (d.monoFoldLossDb - 1.0f) * 25.0f);

    d.maskingDb = (bandDensity (spectrumDb, binHz, 200.0, 800.0)
                   - bandDensity (spectrumDb, binHz, 1000.0, 4000.0)) - 10.0f;
    d.maskingRisk = juce::jlimit (0.0f, 100.0f, (d.maskingDb - 6.0f) * 10.0f);

    d.distortionRisk = juce::jlimit (0.0f, 100.0f,
                                     d.harmonicExcessDb * 4.5f
                                     + d.imdSidebandDb * 4.0f
                                     + d.flatTopPct * 180.0f
                                     + d.crestLossDb * 2.0f);

    if (d.harmonicExcessDb > 5.0f || d.imdSidebandDb > 5.0f || d.flatTopPct > 0.05f)
        ++d.attributionCount;

    if (d.aliasRisk > 55.0f)      ++d.attributionCount;
    if (d.subRisk > 55.0f)        ++d.attributionCount;
    if (d.transientScore < 45.0f) ++d.attributionCount;
    if (d.monoRisk > 40.0f)       ++d.attributionCount;
    if (d.maskingRisk > 55.0f)    ++d.attributionCount;

    d.ready = true;
    return d;
}
} // namespace ml


