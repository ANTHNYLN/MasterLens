#include "ReferenceSpectrum.h"

namespace ml
{
ReferenceSpectrum::ReferenceSpectrum()
    : juce::Thread ("MasterLens reference loader")
{
}

ReferenceSpectrum::~ReferenceSpectrum()
{
    stopThread (4000);
}

void ReferenceSpectrum::load (const juce::StringArray& files, double sampleRate, int fftOrder)
{
    stopThread (4000);

    pendingFiles = files;
    pendingSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    pendingOrder = fftOrder;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = files.isEmpty() ? juce::String ("empty") : juce::String ("loading");
    }

    ready.store (false);
    busy.store (true);

    if (files.isEmpty())
    {
        busy.store (false);
        return;
    }

    startThread();
}

void ReferenceSpectrum::clear()
{
    stopThread (4000);

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        spectrumDb.clear();
        refStats = StatsSnapshot {};
        status = "empty";
    }

    pendingFiles.clear();
    ready.store (false);
    statsReady.store (false);
    busy.store (false);
}

juce::String ReferenceSpectrum::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

juce::StringArray ReferenceSpectrum::getFiles() const
{
    return pendingFiles;
}

std::vector<float> ReferenceSpectrum::getSpectrumDb() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return spectrumDb;
}

StatsSnapshot ReferenceSpectrum::getStats() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return refStats;
}

void ReferenceSpectrum::run()
{
    const int fftSize = 1 << pendingOrder;
    const int numBins = fftSize / 2 + 1;
    const int hop = fftSize / 2;

    std::vector<double> power ((size_t) numBins, 0.0);
    int totalBlocks = 0;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    juce::dsp::FFT fft (pendingOrder);
    std::vector<float> window ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi
                                                             * (double) i / (double) (fftSize - 1));

    std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
    std::vector<float> block ((size_t) fftSize, 0.0f);

    AnalysisEngine refEngine;
    Stats refEngineStats;
    resetStats (refEngineStats);
    refEngine.prepare (pendingSampleRate, 2);

    int blockIndex = 0;

    for (const auto& path : pendingFiles)
    {
        if (threadShouldExit())
            return;

        juce::File file (path);

        if (! file.existsAsFile())
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
            continue;

        const int channels = juce::jmax (1, (int) reader->numChannels);
        const juce::int64 length = reader->lengthInSamples;

        juce::AudioBuffer<float> readBuffer (channels, fftSize);

        for (juce::int64 start = 0; start + fftSize <= length; start += hop)
        {
            if (threadShouldExit())
                return;

            readBuffer.clear();
            reader->read (&readBuffer, 0, fftSize, start, true, true);

            std::fill (block.begin(), block.end(), 0.0f);

            for (int ch = 0; ch < channels; ++ch)
                for (int n = 0; n < fftSize; ++n)
                    block[(size_t) n] += readBuffer.getSample (ch, n);

            const float norm = 1.0f / (float) channels;

            std::fill (fftData.begin(), fftData.end(), 0.0f);

            for (int n = 0; n < fftSize; ++n)
                fftData[(size_t) n] = block[(size_t) n] * norm * window[(size_t) n];

            fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

            for (int b = 1; b < numBins; ++b)
            {
                const double m = (double) fftData[(size_t) b];
                power[(size_t) b] += m * m;
            }

            // Feed every other hop to the loudness engine: hops are half the
            // window, so this covers the file exactly once without overlap.
            // One engine for the whole set keeps integrated loudness and LRA
            // cumulative across the reference tracks.
            if ((blockIndex++ % 2) == 0)
                refEngine.process (readBuffer, refEngineStats, fftSize);

            ++totalBlocks;
        }
    }

    if (totalBlocks < 4)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        status = "failed";
        busy.store (false);
        return;
    }

    std::vector<float> result ((size_t) numBins, kSilentDb);
    const double inv = 1.0 / (double) totalBlocks;

    for (int b = 1; b < numBins; ++b)
    {
        const double mean = power[(size_t) b] * inv;
        result[(size_t) b] = mean > 1.0e-20 ? (float) (10.0 * std::log10 (mean)) : kSilentDb;
    }

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        spectrumDb = std::move (result);
        refStats = ml::snapshot (refEngineStats);
        status = "ready";
        statsReady.store (refStats.hasSignal);
    }

    ready.store (true);
    busy.store (false);
}
} // namespace ml
