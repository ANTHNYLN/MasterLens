#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "Presets.h"

//==============================================================================
void MasterLensAudioProcessorEditor::applyPreset (int index)
{
    if (updatingPreset || index < 0 || index >= 7)
        return;

    setParameter ("targetLufs", ml::presets::targets[index]);
    setParameter ("tpCeiling", ml::presets::ceilings[index]);
}

void MasterLensAudioProcessorEditor::syncPresetToParameters()
{
    if (updatingPreset)
        return;

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    int match = 7;

    for (int i = 0; i < 7; ++i)
    {
        if (std::abs (target - ml::presets::targets[i]) < 0.05f
            && std::abs (ceiling - ml::presets::ceilings[i]) < 0.05f)
        {
            match = i;
            break;
        }
    }

    updatingPreset = true;
    presetBox.setSelectedItemIndex (match, juce::dontSendNotification);
    updatingPreset = false;
}

void MasterLensAudioProcessorEditor::setParameter (const char* id, float value)
{
    if (auto* param = processor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

//==============================================================================
void MasterLensAudioProcessorEditor::resetAnalysis()
{
    processor.requestReset();
    analyzer.reset();
    advisor.reset();

    curve.clear();
    stats = ml::StatsSnapshot {};
    markers.clear();
    tpEvents.clear();
    issueCache.clear();

    // Snapshots describe a musical moment; clearing the session clears them too.
    snapshots[0] = ml::Snapshot {};
    snapshots[1] = ml::Snapshot {};
    viewingSnapshot = -1;
    updateSnapshotChips();

    detail.setIssues ({});
    detail.setHistory ({}, processor.getTargetLufs(), 0.0f);
    timeline.setMarkers ({}, 30.0);
    timeline.setPlayhead (0.0);
    updateRail();
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::importReference()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("选择 1–4 条参考轨（WAV / AIFF / FLAC / MP3）",
           "Choose 1-4 reference tracks (WAV / AIFF / FLAC / MP3)"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto results = fc.getResults();

                                  if (results.isEmpty())
                                      return;

                                  juce::StringArray files;

                                  for (int i = 0; i < juce::jmin (4, results.size()); ++i)
                                      files.add (results[i].getFullPathName());

                                  reference.load (files, analyzerSampleRate, 13);
                                  processor.apvts.state.setProperty ("referenceFiles", files.joinIntoString ("|"), nullptr);
                                  analyzer.clearReference();
                              });
}

void MasterLensAudioProcessorEditor::clearReference()
{
    reference.clear();
    analyzer.clearReference();
    processor.apvts.state.setProperty ("referenceFiles", juce::String(), nullptr);
    detail.setReferenceInfo (false, {});
}

//==============================================================================
void MasterLensAudioProcessorEditor::exportReport()
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M");
    const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                 .getChildFile ("MasterLens-Report-" + stamp + ".txt");

    fileChooser = std::make_unique<juce::FileChooser> (T ("导出检查报告（TXT / JSON / CSV）",
                                                          "Export QC report (TXT / JSON / CSV)"),
                                                       defaultFile, "*.txt");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (file == juce::File {})
                                      return;

                                  file.replaceWithText (buildReport());
                                  file.withFileExtension (".json").replaceWithText (buildReportJson());
                                  file.withFileExtension (".csv").replaceWithText (buildReportCsv());
                              });
}
