#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "Version.h"

//==============================================================================
juce::String MasterLensAudioProcessorEditor::buildReportCsv() const
{
    juce::String csv;
    const auto stamp = juce::Time::getCurrentTime().toISO8601 (true);

    const auto line = [&csv] (const juce::String& section, const juce::String& item,
                              const juce::String& value)
    {
        csv << section << ',' << item << ',' << value << '\n';
    };

    csv << "section,item,value\n";
    line ("meta", "generatedAt", stamp);
    line ("meta", "version", ml::kVersion);
    line ("meta", "sampleRate", juce::String (processor.getSampleRateValue(), 0));

    line ("loudness", "integratedLufs", ml::num (stats.integrated));
    line ("loudness", "shortTermLufs", ml::num (stats.shortTerm));
    line ("loudness", "momentaryLufs", ml::num (stats.momentary));
    line ("loudness", "lraLu", ml::num (stats.lra));
    line ("loudness", "plrDb", ml::num (stats.getPlr()));
    line ("loudness", "psrDb", ml::num (stats.psr));
    line ("loudness", "psrMinDb", ml::num (stats.psrMin));
    line ("loudness", "targetLufs", ml::num (processor.getTargetLufs()));

    line ("truePeak", "maxDbtp", ml::num (stats.truePeakDb));
    line ("truePeak", "ceilingDbtp", ml::num (processor.getCeilingDb()));
    line ("truePeak", "overWindows", juce::String (stats.oversCount));
    line ("truePeak", "maxAt", ml::fmtTime (stats.truePeakMaxTime));

    line ("stereo", "correlation", ml::sgn (stats.correlation, 2));
    line ("stereo", "widthPct", ml::num (stats.widthPct));
    line ("stereo", "lowSidePct", ml::num (stats.lowSidePct));

    for (int i = 0; i < 4; ++i)
        line ("spectrum", "band" + juce::String (i + 1) + "Pct", ml::num (bandRatios[(size_t) i]));

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (stats, p);
        const auto name = T (p.nameZh, p.nameEn);

        line ("platform", name + " targetLufs", ml::num (p.targetLufs));
        line ("platform", name + " ceilingDbtp", ml::num (p.tpCeiling));
        line ("platform", name + " gainDb", ml::sgn (res.gainDb));
        line ("platform", name + " predictedTp", ml::num (res.predictedTp));
        line ("platform", name + " state",
              res.state == ml::DeliveryState::over ? "over"
            : res.state == ml::DeliveryState::warning ? "warning" : "ok");
    }

    for (const auto& c : ml::buildChecklist (stats, processor.getTargetLufs(),
                                             processor.getCeilingDb(),
                                             analyzer.getBandDeviations()))
        line ("check", c.label, c.value);

    for (const auto& m : markers)
        line ("marker", ml::fmtTime (m.time), m.text);

    return csv;
}

//==============================================================================
void MasterLensAudioProcessorEditor::setPageForSnapshot (int page)
{
    setPage (page);
}

void MasterLensAudioProcessorEditor::expandFirstIssueForSnapshot()
{
    setPage (5);
    detail.expandFirstIssueForSnapshot();
}
