#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Text.h"
#include "dsp/ReferenceSpectrum.h"
#include "ui/Modules.h"
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

    /** Used by the offscreen snapshot tool. */
    void setAboutVisible (bool shouldBeVisible) { aboutPanel.setVisibleAndFocus (shouldBeVisible); }
    void expandFirstIssueForSnapshot();
    void setPageForSnapshot (int page);

private:
    void timerCallback() override;
    void updateTexts();
    void updateRail();
    void applyPreset (int index);
    void syncPresetToParameters();
    void resetAnalysis();
    void exportReport();
    void importReference();
    void clearReference();
    juce::String buildReport() const;
    juce::String buildReportJson() const;
    void setParameter (const char* id, float value);
    juce::String moduleTitle (int index) const;

    MasterLensAudioProcessor& processor;
    ml::LightLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::Label targetCaption, ceilingCaption;
    juce::Slider targetSlider, ceilingSlider;
    juce::TextButton resetButton, freezeButton, languageButton, aboutButton;

    ml::ModuleRail rail;
    ml::DetailArea detail;
    ml::TimelineStrip timeline;
    ml::AboutPanel aboutPanel;

    ml::SpectrumAnalyzer analyzer;
    ml::Advisor advisor;
    ml::ReferenceSpectrum reference;

    std::vector<float> curve;
    ml::StatsSnapshot stats;
    std::vector<ml::Marker> markers;
    std::vector<ml::TpEvent> tpEvents;
    std::vector<ml::Issue> issueCache;
    std::array<float, 4> bandRatios { 0.0f, 0.0f, 0.0f, 0.0f };

    double analyzerSampleRate = 0.0;
    bool frozen = false;
    bool updatingPreset = false;
    int frameCounter = 0;
    juce::String lastStatsSignature;
    juce::Rectangle<int> pageTitleArea;
    int tick = 0;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment, ceilingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLensAudioProcessorEditor)
};
