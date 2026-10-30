#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>
#include <cmath>

#include "dsp/AnalysisEngine.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/DefectAnalyzer.h"
#include "Delivery.h"

static int failures = 0;

static void check (const char* name, double actual, double expected, double tolerance)
{
    const bool ok = std::abs (actual - expected) <= tolerance;
    std::printf ("%-28s actual %8.2f  expected %8.2f  tol %.2f  %s\n",
                 name, actual, expected, tolerance, ok ? "OK" : "FAIL");

    if (! ok)
        ++failures;
}

int main()
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 256;

    std::printf ("MasterLens DSP self-test @ %.0f Hz\n\n", sr);

    // ---------------------------------------------------------------------
    // 1) 997 Hz sine, -20 dBFS peak (stereo) -> ITU-R BS.1770 expects -23.0 LUFS
    // ---------------------------------------------------------------------
    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        const double freq = 997.0;
        double phase = 0.0;
        const int totalBlocks = (int) (sr * 20.0 / blockSize);

        for (int b = 0; b < totalBlocks; ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = (float) (0.1 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, v);

                phase += 2.0 * juce::MathConstants<double>::pi * freq / sr;

                if (phase > 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
            }

            engine.process (buffer, stats, blockSize);
        }

        const auto s = ml::snapshot (stats);

        // BS.1770 sums channel energies: a dual-mono stereo sine reads 3 LU
        // higher than the same sine on a single channel.
        check ("integrated LUFS (stereo)", s.integrated, -20.0, 0.3);
        check ("true peak dBTP", s.truePeakDb, -20.0, 0.5);
        check ("sample peak dBFS", s.samplePeakDb, -20.0, 0.05);
        check ("correlation", s.correlation, 1.0, 0.01);
        check ("width %", s.widthPct, 0.0, 0.5);
        check ("LRA (steady tone)", s.lra, 0.0, 0.3);
        check ("PLR (dual mono sine)", s.getPlr(), 0.0, 0.3);
        check ("clip events", (double) s.clipEvents, 0.0, 0.0);
    }

    // ---------------------------------------------------------------------
    // 1b) Mono 997 Hz sine at -20 dBFS -> exactly -23.0 LUFS (R128 reference)
    // ---------------------------------------------------------------------
    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 1);

        juce::AudioBuffer<float> buffer (1, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 20.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                buffer.setSample (0, n, (float) (0.1 * std::sin (phase)));
                phase += 2.0 * juce::MathConstants<double>::pi * 997.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        const auto s = ml::snapshot (stats);

        check ("integrated LUFS (mono)", s.integrated, -23.0, 0.3);
    }

    // ---------------------------------------------------------------------
    // 2) Silence gating: 20 s of silence must not corrupt an earlier reading
    // ---------------------------------------------------------------------
    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        const int toneBlocks = (int) (sr * 10.0 / blockSize);
        double phase = 0.0;

        for (int b = 0; b < toneBlocks; ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = (float) (0.1 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, v);
                phase += 2.0 * juce::MathConstants<double>::pi * 997.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        buffer.clear();

        for (int b = 0; b < (int) (sr * 20.0 / blockSize); ++b)
            engine.process (buffer, stats, blockSize);

        const auto s = ml::snapshot (stats);

        check ("integrated after silence", s.integrated, -20.0, 0.3);
        check ("short term after silence", s.shortTerm, -100.0, 1.0);
    }

    // ---------------------------------------------------------------------
    // 3) Phase inverted stereo -> correlation close to -1
    // ---------------------------------------------------------------------
    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 5.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = (float) (0.1 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, -v);
                phase += 2.0 * juce::MathConstants<double>::pi * 997.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        const auto s = ml::snapshot (stats);

        check ("correlation (inverted)", s.correlation, -1.0, 0.05);
        check ("width % (inverted)", s.widthPct, 200.0, 0.5);
    }

    // ---------------------------------------------------------------------
    // 4) Spectrum analyser calibration: full scale 1 kHz sine -> ~0 dB at 1 kHz
    // ---------------------------------------------------------------------
    {
        ml::SpectrumRing ring;
        ring.prepare (16384);

        ml::SpectrumAnalyzer analyzer;
        analyzer.prepare (sr, 12);

        double phase = 0.0;

        for (int n = 0; n < 16384; ++n)
        {
            const float v = (float) std::sin (phase);
            ring.write (v);
            phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / sr;
        }

        for (int i = 0; i < 30; ++i)
            analyzer.update (ring);

        int peakBin = 1;
        float peakDb = -200.0f;
        const float* mags = analyzer.getMagnitudes();

        for (int i = 1; i < analyzer.getNumBins(); ++i)
            if (mags[i] > peakDb)
            {
                peakDb = mags[i];
                peakBin = i;
            }

        check ("spectrum peak dB", peakDb, 0.0, 1.5);
        check ("spectrum peak Hz", peakBin * analyzer.getBinHz(), 1000.0, 15.0);
    }

    // ---------------------------------------------------------------------
    // 5) DC offset: a real offset must be detected, a plain bass note must not
    // ---------------------------------------------------------------------
    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 6.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = 0.01f + (float) (0.1 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, v);
                phase += 2.0 * juce::MathConstants<double>::pi * 997.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        check ("DC offset (0.01 DC)", ml::snapshot (stats).dcOffsetDb, -40.0, 2.0);
    }

    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 8.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = (float) (0.3 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, v);
                phase += 2.0 * juce::MathConstants<double>::pi * 55.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        check ("DC offset (55 Hz bass)", ml::snapshot (stats).dcOffsetDb, -100.0, 30.0);
    }

    // ---------------------------------------------------------------------
    // 6) Delivery prediction: deterministic gain + predicted true peak
    // ---------------------------------------------------------------------
    {
        ml::StatsSnapshot s;
        s.integrated = -16.0f;
        s.truePeakDb = -3.0f;
        s.hasSignal = true;

        const auto spotify = ml::evaluate (s, ml::platform (0));   // -14 LUFS / -1 dBTP
        check ("delivery gain (Spotify)", spotify.gainDb, 2.0, 0.001);
        check ("delivery predicted TP", spotify.predictedTp, -1.0, 0.001);
        check ("delivery state warning", (double) spotify.state,
               (double) ml::DeliveryState::warning, 0.0);

        s.truePeakDb = -1.0f;
        const auto over = ml::evaluate (s, ml::platform (0));
        check ("delivery raw TP (over)", over.predictedTpRaw, 1.0, 0.001);
        check ("delivery predicted TP (limiter)", over.predictedTp, -1.0, 0.001);
        check ("delivery service limiting", over.limitedDb, 2.0, 0.001);
        check ("delivery state over", (double) over.state, (double) ml::DeliveryState::over, 0.0);

        s.integrated = -100.0f;
        s.truePeakDb = -100.0f;
        const auto silent = ml::evaluate (s, ml::platform (0));
        check ("delivery silent gain", silent.gainDb, 0.0, 0.0);
        check ("delivery silent state", (double) silent.state, (double) ml::DeliveryState::ok, 0.0);

        check ("platform table size", (double) ml::numPlatforms, 8.0, 0.0);
    }

    // ---------------------------------------------------------------------
    // 7) Signal integrity: mono fold loss, sub share, flat-top density
    // ---------------------------------------------------------------------
    {
        // anti-phase channels cancel completely when folded to mono
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 6.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = (float) (0.25 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, -v);
                phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        const auto s = ml::snapshot (stats);
        check ("mono fold loss (anti-phase)", s.monoFoldLossDb, 30.0, 3.0);
        check ("mono retained % (anti-phase)", s.monoRetainPct, 0.0, 3.0);
        check ("correlation (anti-phase)", s.correlation, -1.0, 0.01);
    }

    {
        // 30 Hz bass should count as sub energy; a 2 kHz tone should not
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 6.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = (float) (0.25 * std::sin (phase));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, v);
                phase += 2.0 * juce::MathConstants<double>::pi * 30.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        const auto s = ml::snapshot (stats);
        check ("sub share 30 Hz > 40%", s.subSharePct > 40.0f ? 1.0 : 0.0, 1.0, 0.0);
        check ("mono retained % (dual mono)", s.monoRetainPct, 100.0, 2.0);
        check ("mono fold loss (dual mono)", s.monoFoldLossDb, 0.0, 0.2);
    }

    {
        // hard-clipped sine leaves a measurable flat top
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, blockSize);
        double phase = 0.0;

        for (int b = 0; b < (int) (sr * 5.0 / blockSize); ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const float v = juce::jlimit (-1.0f, 1.0f, (float) (1.7 * std::sin (phase)));
                buffer.setSample (0, n, v);
                buffer.setSample (1, n, v);
                phase += 2.0 * juce::MathConstants<double>::pi * 100.0 / sr;
            }

            engine.process (buffer, stats, blockSize);
        }

        const auto s = ml::snapshot (stats);
        check ("flat-top density > 0.2%", s.flatTopPct > 0.2f ? 1.0 : 0.0, 1.0, 0.0);
        check ("flat-top density < 60%", s.flatTopPct < 60.0f ? 1.0 : 0.0, 1.0, 0.0);
    }

    // ---------------------------------------------------------------------
    // 8) Defect analyser on a synthetic averaged spectrum
    // ---------------------------------------------------------------------
    {
        constexpr double binHz = 48000.0 / 8192.0;
        std::vector<float> spec (4097, -60.0f);

        // fundamental 100 Hz with a strong 2nd-5th harmonic series
        const auto addTone = [&] (double hz, float db)
        {
            const int b = (int) std::round (hz / binHz);
            spec[(size_t) b] = db;
            spec[(size_t) (b - 1)] = db - 3.0f;
            spec[(size_t) (b + 1)] = db - 3.0f;
        };

        addTone (100.0, -20.0f);
        addTone (200.0, -40.0f);
        addTone (300.0, -40.0f);
        addTone (400.0, -40.0f);
        addTone (500.0, -40.0f);

        ml::StatsSnapshot st;
        st.hasSignal = true;
        st.integrated = -14.0f;
        st.truePeakDb = -1.0f;
        st.psr = 12.0f;
        st.subSharePct = 10.0f;
        st.monoRetainPct = 100.0f;
        st.flatTopPct = 0.0f;

        const auto d = ml::analyseSignalDefects (spec, binHz, st);
        check ("defects ready", d.ready ? 1.0 : 0.0, 1.0, 0.0);
        check ("harmonic excess > 8 dB", d.harmonicExcessDb > 8.0f ? 1.0 : 0.0, 1.0, 0.0);
        check ("distortion risk > 30", d.distortionRisk > 30.0f ? 1.0 : 0.0, 1.0, 0.0);

        // lift the top octave to simulate aliasing
        for (int i = 1; i < 4096; ++i)
        {
            const double f = (double) i * binHz;

            if (f >= 16000.0)
                spec[(size_t) i] = -40.0f;
        }

        const auto alias = ml::analyseSignalDefects (spec, binHz, st);
        check ("alias risk > 50", alias.aliasRisk > 50.0f ? 1.0 : 0.0, 1.0, 0.0);
        check ("smoothing 1/3 oct at 100 Hz",
               (double) ml::SpectrumAnalyzer::smoothingOctavesAt (100.0), 1.0 / 3.0, 1.0e-6);
        check ("smoothing 1/6 oct at 1 kHz",
               (double) ml::SpectrumAnalyzer::smoothingOctavesAt (1000.0), 1.0 / 6.0, 1.0e-6);
        check ("smoothing 1/12 oct at 10 kHz",
               (double) ml::SpectrumAnalyzer::smoothingOctavesAt (10000.0), 1.0 / 12.0, 1.0e-6);
    }

    // ---------------------------------------------------------------------
    // 9) CPU benchmark (stereo, 48 kHz, 512-sample blocks)
    // ---------------------------------------------------------------------
    {
        ml::Stats stats;
        ml::AnalysisEngine engine;
        engine.prepare (sr, 2);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::Random rng (7);

        for (int n = 0; n < 512; ++n)
        {
            buffer.setSample (0, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.2f);
            buffer.setSample (1, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.2f);
        }

        const int blocks = (int) (sr * 60.0 / 512.0);
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < blocks; ++b)
            engine.process (buffer, stats, 512);

        const double wall = juce::Time::highResolutionTicksToSeconds (
            juce::Time::getHighResolutionTicks() - start);

        std::printf ("\nCPU: %.3f s wall for 60 s of stereo audio (%.2f %% of one core)\n",
                     wall, 100.0 * wall / 60.0);

        if (wall > 6.0)
        {
            std::printf ("performance budget exceeded\n");
            ++failures;
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
