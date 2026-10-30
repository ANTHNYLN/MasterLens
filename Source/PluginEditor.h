#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Text.h"
#include "dsp/ReferenceSpectrum.h"
#include "ui/Modules.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

class MasterLensAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer,
                                      private juce::KeyListener
{
public:
    explicit MasterLensAudioProcessorEditor (MasterLensAudioProcessor&);
    ~MasterLensAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool keyPressed (const juce::KeyPress&) override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged (bool isKeyDown) override;

    /** Used by the offscreen snapshot tool. */
    void setAboutVisible (bool shouldBeVisible) { aboutPanel.setVisibleAndFocus (shouldBeVisible); }
    void showGenreInfoForSnapshot() { showGenreInfo(); }
    void setPresetForSnapshot (int index) { presetBox.setSelectedId (index + 1, juce::dontSendNotification); applyPreset (index); }
    void expandFirstIssueForSnapshot();
    void setPageForSnapshot (int page);
    void setThemeForSnapshot (bool useDark);
    void captureSnapshotForSnapshot (int slot);

private:
    void timerCallback() override;
    void buildControls();
    void updateTexts();
    void updateRail();
    void applyPreset (int index);
    void showGenreInfo();
    void syncPresetToParameters();
    void resetAnalysis();
    void exportReport();
    void importReference();
    void clearReference();
    juce::String buildReport() const;
    juce::String buildReportJson() const;
    juce::String buildReportCsv() const;
    void setParameter (const char* id, float value);
    void setHold (bool shouldHold, bool fromKeyboard);
    void setPage (int index);
    void toggleTheme();
    void applyTheme();
    void attachShortcutListener (juce::Component& c);
    void addMarker (double t, int severity, int category, const juce::String& text);
    void addManualMarker (double time);
    void captureSnapshot (int slot);
    void recallSnapshot (int slot);
    void clearSnapshot (int slot);
    void updateSnapshotChips();

    MasterLensAudioProcessor& processor;
    ml::MasterLensLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 650 };

    juce::ComboBox presetBox;
    juce::TextButton infoButton;
    juce::Label targetCaption, ceilingCaption;
    juce::Slider targetSlider, ceilingSlider;
    juce::TextButton holdButton, clearButton, languageButton, aboutButton, themeButton, exportButton;

    ml::ModuleRail rail;
    ml::DetailArea detail;
    ml::TimelineStrip timeline;
    ml::AboutPanel aboutPanel;
    ml::GenrePanel genrePanel;
    ml::SnapshotChip snapshotA { "A" };
    ml::SnapshotChip snapshotB { "B" };

    ml::SpectrumAnalyzer analyzer;
    ml::Advisor advisor;
    ml::ReferenceSpectrum reference;

    std::vector<float> curve;
    ml::StatsSnapshot stats;
    std::vector<ml::Marker> markers;
    std::vector<ml::TpEvent> tpEvents;
    std::vector<ml::Issue> issueCache;
    std::array<float, 4> bandRatios { 0.0f, 0.0f, 0.0f, 0.0f };

    ml::Snapshot snapshots[2];
    int viewingSnapshot = -1;

    double analyzerSampleRate = 0.0;
    bool frozen = false;
    bool holdFromKeyboard = false;
    bool updatingPreset = false;
    float lraTargetSetting = 8.0f;
    float clipAllowSetting = 0.0f;
    int frameCounter = 0;
    juce::String lastStatsSignature;
    int tick = 0;
    int selectedPage = 0;

    juce::Rectangle<int> headerArea, paramArea, footerArea;

    juce::String footerStatusText;
    juce::Colour footerStatusColour { ml::theme::textFaint };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttachment, ceilingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLensAudioProcessorEditor)
};
