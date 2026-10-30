#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>

#include "../dsp/AnalysisEngine.h"
#include "../dsp/DefectAnalyzer.h"
#include "../dsp/SpectrumAnalyzer.h"
#include "Advisor.h"
#include "Theme.h"
#include "Widgets.h"

namespace ml
{
struct Marker
{
    double time = 0.0;
    int severity = 0;      // 0 info, 1 warn, 2 over
    int category = 0;      // 0 TP, 1 loudness, 2 phase, 3 spectrum, 4 manual
    juce::String text;
};

struct UiState
{
    StatsSnapshot stats;
    std::vector<float> bandDevs;
    std::array<float, 4> ratios { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<SpectrumAnalyzer::Resonance> resonances;
    std::vector<TpEvent> tpEvents;
    std::vector<Marker> markers;
    SignalDefects defects;
    StatsSnapshot referenceStats;
    bool hasReferenceStats = false;
    float target = -14.0f;
    float ceiling = -1.0f;
    bool hasReference = false;
    juce::String referenceStatus;
    const SpectrumAnalyzer* analyzer = nullptr;

    int snapshotSlot = -1;          // -1 = live, 0 = A, 1 = B
    float snapshotDelta[2] { 0.0f, 0.0f };
    bool snapshotValid[2] { false, false };
};

enum class PageId { loudness = 0, truePeak, spectrum, stereo, delivery, signal, diagnostics, timeline };
constexpr int numPages = 8;

//==============================================================================
/** Left hand module list: verdict header + one 40 px row per module. */
class ModuleRail : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    ModuleRail();

    void setModule (int index, juce::String title, juce::String value,
                    juce::String status, juce::Colour colour);
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }
    void setVerdict (juce::String headline, juce::String subline, juce::Colour colour);

    std::function<void (int)> onSelect;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    struct Item
    {
        juce::String title, value, status;
        juce::Colour colour { theme::textFaint };
    };

    juce::Rectangle<int> rowBounds (int index) const;

    std::array<Item, numPages> items;
    juce::String verdictHeadline, verdictSubline;
    juce::Colour verdictColour { theme::textDim };
    int selected = 0;
    int hoverRow = -1;
};

//==============================================================================
/** Song-length marker strip with a live playhead; double click adds a marker. */
class TimelineStrip : public juce::Component
{
public:
    TimelineStrip();

    void setMarkers (const std::vector<Marker>& m, double totalSeconds);
    void setPlayhead (double seconds);

    std::function<void (double)> onDoubleClick;

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> trackArea() const;
    double timeAtX (int x) const;

    std::vector<Marker> markers;
    double total = 60.0;
    double playhead = 0.0;
};

//==============================================================================
/** Primary content area: one page per module, plus the shared empty state. */
class DetailArea : public juce::Component
{
public:
    DetailArea();

    void setPage (PageId p);
    PageId getPage() const noexcept { return page; }

    void setState (const UiState& s);
    void setIssues (std::vector<Issue> issues);
    void setHistory (const std::vector<float>& curve, float targetLufs, float lra);
    void expandFirstIssueForSnapshot();
    void setReferenceInfo (bool hasRef, const juce::String& status);
    void refreshTexts();
    void invalidateDisplays();

    std::function<void()> onImportReference;
    std::function<void()> onClearReference;
    std::function<void()> onExportReport;
    std::function<void()> onAddMarkerNow;
    std::function<void (double)> onAddMarkerAt;

    juce::TextButton& getImportButton() noexcept { return refImport; }
    juce::TextButton& getClearButton() noexcept { return refClear; }
    juce::TextButton& getExportButton() noexcept { return reportButton; }
    juce::TextButton& getMarkButton() noexcept { return markButton; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void layoutForPage();
    void hideAllChildren();
    juce::Rectangle<int> contentArea() const;
    void paintPageHeader (juce::Graphics&, const juce::String& meta);

    void paintLoudness (juce::Graphics&);
    void paintTruePeak (juce::Graphics&);
    void paintSpectrumExtras (juce::Graphics&);
    void paintStereoExtras (juce::Graphics&);
    void paintDelivery (juce::Graphics&);
    void paintSignal (juce::Graphics&);
    void paintDiagnostics (juce::Graphics&);
    void paintTimeline (juce::Graphics&);
    void paintReferenceBar (juce::Graphics&, juce::Rectangle<int>);

    PageId page = PageId::loudness;
    UiState state;
    juce::String refStatus;
    bool refReady = false;
    bool emptyMode = true;
    int tick = 0;

    MetricGrid tiles;
    LevelMeters levelBars;
    HistoryGraph history;
    SpectrumDisplay spectrum;
    BandBalanceDisplay bands;
    StereoMeters meters;
    IssueList issues;

    juce::TextButton refImport, refClear, reportButton, markButton;
};
} // namespace ml
