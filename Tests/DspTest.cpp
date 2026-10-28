#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>
#include <cmath>

#include "dsp/AnalysisEngine.h"
#include "dsp/SpectrumAnalyzer.h"

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
    // 6) CPU benchmark (stereo, 48 kHz, 512-sample blocks)
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
