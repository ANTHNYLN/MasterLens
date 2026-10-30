#include "PluginEditor.h"
#include "Delivery.h"
#include "Presets.h"
#include "Version.h"

using ml::T;

//==============================================================================
MasterLensAudioProcessorEditor::MasterLensAudioProcessorEditor (MasterLensAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    tooltipWindow.setLookAndFeel (&lnf);
    setOpaque (true);
    setWantsKeyboardFocus (true);

    const int storedAppearance = (int) processor.apvts.state.getProperty ("appearance", 1);
    ml::theme::apply (storedAppearance != 0);
    lnf.refreshColours();

    addAndMakeVisible (rail);
    addAndMakeVisible (detail);
    addAndMakeVisible (timeline);

    rail.onSelect = [this] (int index) { setPage (index); };
    detail.onImportReference = [this] { importReference(); };
    detail.onClearReference  = [this] { clearReference(); };
    detail.onExportReport    = [this] { exportReport(); };
    detail.onAddMarkerNow    = [this] { addManualMarker (stats.elapsed); };
    detail.onAddMarkerAt     = [this] (double t) { addManualMarker (t); };
    timeline.onDoubleClick   = [this] (double t) { addManualMarker (t); };

    buildControls();

    aboutPanel.onClose = [this] { aboutPanel.setVisibleAndFocus (false); };
    addChildComponent (aboutPanel);
    aboutPanel.setVisible (false);

    lraTargetSetting = (float) processor.apvts.state.getProperty ("lraTarget", 8.0);
    clipAllowSetting = (float) processor.apvts.state.getProperty ("clipAllowance", 0.0);

    genrePanel.onClose = [this] { genrePanel.setVisibleAndFocus (false); };
    genrePanel.onSettingsChanged = [this] (float lraTarget, float clipAllow)
    {
        lraTargetSetting = lraTarget;
        clipAllowSetting = clipAllow;
        processor.apvts.state.setProperty ("lraTarget", lraTarget, nullptr);
        processor.apvts.state.setProperty ("clipAllowance", clipAllow, nullptr);
    };
    genrePanel.onTargetChanged = [this] (float target)
    {
        setParameter ("targetLufs", target);
    };
    addChildComponent (genrePanel);
    genrePanel.setVisible (false);

    const int storedLanguage = (int) processor.apvts.state.getProperty ("language", 0);
    ml::Localisation::set (storedLanguage == 1 ? ml::Language::english : ml::Language::chinese);

    const int storedW = (int) processor.apvts.state.getProperty ("editorWidth", 1180);
    const int storedH = (int) processor.apvts.state.getProperty ("editorHeight", 760);
    setResizable (true, true);
    setResizeLimits (900, 620, 1900, 1400);
    setSize (juce::jlimit (900, 1900, storedW), juce::jlimit (620, 1400, storedH));

    updateTexts();
    syncPresetToParameters();
    updateSnapshotChips();

    analyzerSampleRate = processor.getSampleRateValue();
    analyzer.prepare (analyzerSampleRate, 13);

    const auto saved = processor.apvts.state.getProperty ("referenceFiles").toString();

    if (saved.isNotEmpty())
    {
        juce::StringArray files;
        files.addTokens (saved, "|", {});
        reference.load (files, analyzerSampleRate, 13);
    }

    attachShortcutListener (*this);
    startTimerHz (30);
}

MasterLensAudioProcessorEditor::~MasterLensAudioProcessorEditor()
{
    stopTimer();
    ml::Localisation::clearOnChange();
    setLookAndFeel (nullptr);
}

void MasterLensAudioProcessorEditor::attachShortcutListener (juce::Component& c)
{
    c.addKeyListener (this);

    for (auto* child : c.getChildren())
        if (child != nullptr)
            attachShortcutListener (*child);
}
