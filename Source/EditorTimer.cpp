#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "ui/StatusText.h"

using ml::statusLabel;
using ml::statusColour;

//==============================================================================
void MasterLensAudioProcessorEditor::setPage (int index)
{
    selectedPage = juce::jlimit (0, ml::numPages - 1, index);
    rail.setSelected (selectedPage);
    detail.setPage ((ml::PageId) selectedPage);
    repaint();
}

void MasterLensAudioProcessorEditor::addMarker (double t, int severity, int category,
                                                const juce::String& text)
{
    if (t <= 0.05)
        return;

    for (const auto& m : markers)
        if (m.category == category && std::abs (m.time - t) < 0.45)
            return;

    markers.push_back ({ t, severity, category, text });

    if (markers.size() > 64)
        markers.erase (markers.begin());
}

void MasterLensAudioProcessorEditor::addManualMarker (double time)
{
    const auto t = juce::jlimit (0.0, juce::jmax (0.5, (double) stats.elapsed), time);
    addMarker (t, 0, 4, T ("手动标记 ", "Manual marker ") + ml::fmtTime (t));
    detail.invalidateDisplays();
    updateRail();
}

void MasterLensAudioProcessorEditor::timerCallback()
{
    ++frameCounter;

    if (holdFromKeyboard && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        holdFromKeyboard = false;
        setHold (false, false);
    }

    const double sr = processor.getSampleRateValue();

    if (std::abs (sr - analyzerSampleRate) > 0.5)
    {
        analyzerSampleRate = sr;
        analyzer.prepare (sr, 13);
    }

    analyzer.setSmoothing (processor.getSmoothing());

    // Page-aware analysis rate: the spectrum page gets 20 fps, other pages only
    // need the band data for the rail/advisor (5 fps). Hold stops FFT work too.
    const bool spectrumVisible = selectedPage == (int) ml::PageId::spectrum;
    const bool runAnalyzer = spectrumVisible ? (frameCounter % 3) != 0
                                             : (frameCounter % 6) == 0;

    if (! frozen && runAnalyzer)
        analyzer.update (processor.spectrumRing);

    if (reference.isReady() && ! analyzer.hasReference())
        analyzer.setReferenceSpectrum (reference.getSpectrumDb());

    if (frozen)
    {
        timeline.setPlayhead (stats.elapsed);
        return;
    }

    stats = processor.getStatsSnapshot();

    if (viewingSnapshot >= 0 && snapshots[viewingSnapshot].valid)
        stats = snapshots[viewingSnapshot].stats;

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    {
        const auto& ring = processor.engine.getTpEvents();
        const int n = ring.size();

        tpEvents.clear();
        tpEvents.reserve ((size_t) n);

        for (int i = 0; i < n; ++i)
            tpEvents.push_back (ring.get (i));
    }

    bandRatios = analyzer.getBandRatios();

    const auto& devs = analyzer.getBandDeviations();
    const auto& resos = analyzer.getResonances();

    if ((frameCounter % 15) == 0)
        signalDefects = ml::analyseSignalDefects (analyzer.getAverageSpectrumDb(),
                                                  analyzer.getBinHz(), stats);

    for (const auto& e : tpEvents)
        addMarker (e.time, 2, 0, T ("真峰值 +", "TP +") + ml::fmt1 (e.overshootDb) + " dBTP");

    if (stats.psrMin > -99.0f && stats.psrMinTime > 0.0)
        addMarker (stats.psrMinTime, 1, 1, T ("PSR 最低 ", "PSR minimum ") + ml::fmt1 (stats.psrMin) + " dB");

    if (stats.lastSpikeTime > 0.0)
        addMarker (stats.lastSpikeTime, 1, 1, T ("响度瞬时尖峰", "Loudness spike"));

    if (stats.correlation < 0.0f && stats.elapsed > 1.0)
        addMarker (std::floor (stats.elapsed), 2, 2, T ("相关性为负", "Negative correlation"));

    if (stats.elapsed > 3.0)
        for (const auto& r : resos)
            addMarker (std::floor (stats.elapsed / 5.0) * 5.0, 1, 3,
                       T ("谐振 ", "Resonance ") + juce::String (juce::roundToInt (r.freq))
                           + " Hz +" + ml::fmt1 (r.excessDb));

    ml::UiState us;
    us.stats = stats;
    us.bandDevs = devs;
    us.ratios = bandRatios;
    us.resonances = resos;
    us.tpEvents = tpEvents;
    us.markers = markers;
    us.defects = signalDefects;
    us.hasReferenceStats = reference.hasStats();

    if (us.hasReferenceStats)
        us.referenceStats = reference.getStats();

    us.target = target;
    us.ceiling = ceiling;
    us.hasReference = analyzer.hasReference();
    us.referenceStatus = reference.getStatus();
    us.analyzer = &analyzer;
    us.snapshotSlot = viewingSnapshot;
    us.snapshotValid[0] = snapshots[0].valid;
    us.snapshotValid[1] = snapshots[1].valid;

    if (snapshots[0].valid && snapshots[1].valid)
    {
        us.snapshotDelta[0] = snapshots[1].stats.integrated - snapshots[0].stats.integrated;
        us.snapshotDelta[1] = snapshots[1].stats.truePeakDb - snapshots[0].stats.truePeakDb;
    }

    detail.setState (std::move (us));

    if ((frameCounter % 6) == 0)
        detail.setReferenceInfo (analyzer.hasReference(), reference.getStatus());

    if ((frameCounter % 15) == 0)
        timeline.setMarkers (markers, juce::jmax (30.0, stats.elapsed));

    timeline.setPlayhead (stats.elapsed);

    if ((frameCounter % 2) == 0)
    {
        if (viewingSnapshot < 0)
        {
            processor.copyShortTermCurve (curve, 1800);
            detail.setHistory (curve, target, stats.lra);
        }
    }

    // The advisor, issue list and rail only need a few updates per second; the
    // meters and spectrum keep their full frame rate.
    if ((frameCounter % 6) == 0)
    {
        advisor.update (stats, devs, target, ceiling, signalDefects);
        issueCache = advisor.getIssues();

        detail.setIssues (issueCache);
        updateRail();
    }
}
