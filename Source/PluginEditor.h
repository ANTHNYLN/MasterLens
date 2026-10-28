#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Text.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

class MasterLensAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit MasterLensAudioProcessorEditor (MasterLensAudioProcessor&);
    ~MasterLensAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateTexts();
    void applyPreset (int index);
    void syncPresetToParameters();
    void resetAnalysis();
    void exportReport();
    juce::String buildReport() const;
    void setParameter (const char* id, float value);

    MasterLensAudioProcessor& processor;
    ml::LightLookAndFeel lnf;

    // ---- header -------------------------------------------------------------
    juce::ComboBox presetBox;
    juce::Label targetCaption, ceilingCaption;
    juce::Slider targetSlider, ceilingSlider;
    juce::TextButton resetButton, freezeButton, exportButton, languageButton;

    // ---- cards --------------------------------------------------------------
    ml::Card spectrumCard, issuesCard, loudnessCard, historyCard, metersCard;

    // ---- displays -----------------------------------------------------------
    ml::SpectrumDisplay spectrum;
    ml::BandBalanceDisplay bands;
    ml::LoudnessTiles tiles;
    ml::LevelBars levelBars;
    ml::HistoryGraph historyGraph;
    ml::StereoMeters meters;
    ml::IssueList issueList;

    ml::SpectrumAnalyzer analyzer;
    ml::Advisor advisor;

    std::vector<float> curve;
    ml::StatsSnapshot stats;
    double analyzerSampleRate = 0.0;
    bool frozen = false;
    bool updatingPreset = false;
    int frameCounter = 0;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment, ceilingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLensAudioProcessorEditor)
};
