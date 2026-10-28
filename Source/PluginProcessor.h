#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AnalysisEngine.h"
#include "dsp/SpectrumAnalyzer.h"

class MasterLensAudioProcessor : public juce::AudioProcessor
{
public:
    MasterLensAudioProcessor();
    ~MasterLensAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MasterLens"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "MasterLens"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    ml::Stats stats;
    ml::AnalysisEngine engine;
    ml::SpectrumRing spectrumRing;

    ml::StatsSnapshot getStatsSnapshot() const { return ml::snapshot (stats); }
    void requestReset() { resetRequested.store (true); }
    void copyShortTermCurve (std::vector<float>& dest, int maxPoints) const
    {
        engine.copyRecentShortTerm (dest, maxPoints);
    }

    float getTargetLufs() const;
    float getCeilingDb() const;
    float getSmoothing() const;
    double getSampleRateValue() const noexcept { return currentSampleRate; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<bool> resetRequested { false };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLensAudioProcessor)
};
