#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>

#include "../dsp/AnalysisEngine.h"
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
    int category = 0;      // 0 TP, 1 loudness, 2 phase, 3 spectrum, 4 other
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
    float target = -14.0f;
    float ceiling = -1.0f;
    bool hasReference = false;
    juce::String referenceStatus;
    const SpectrumAnalyzer* analyzer = nullptr;
};

enum class PageId { loudness = 0, truePeak, spectrum, stereo, delivery, diagnostics, timeline };
constexpr int numPages = 7;

//==============================================================================
/** Left hand module list: status lamp + key reading per module. */
class ModuleRail : public juce::Component
{
public:
    ModuleRail();

    void setModule (int index, juce::String title, juce::String value, juce::Colour colour);
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }

    std::function<void (int)> onSelect;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Item
    {
        juce::String title, value;
        juce::Colour colour { theme::textFaint };
    };

    std::array<Item, numPages> items;
    int selected = 0;
    int rowHeight() const { return juce::jmax (46, getHeight() / numPages); }
};

//==============================================================================
/** Shared horizontal marker strip. */
class TimelineStrip : public juce::Component
{
public:
    TimelineStrip() { setInterceptsMouseClicks (false, false); }

    void setMarkers (const std::vector<Marker>& m, double totalSeconds);
    void paint (juce::Graphics&) override;

private:
    std::vector<Marker> markers;
    double total = 60.0;
};

//==============================================================================
/** The large primary area: renders the selected module, reusing the existing
    widgets (tiles, level bars, history, spectrum, band balance, meters, issues).
*/
class DetailArea : public juce::Component
{
public:
    DetailArea();

    void setPage (PageId p);
    PageId getPage() const noexcept { return page; }

    void setState (const UiState& s);
    void setIssues (std::vector<Issue> issues);
    void setHistory (const std::vector<float>& curve, float targetLufs);
    void expandFirstIssueForSnapshot();
    void setReferenceInfo (bool hasRef, const juce::String& status);
    void refreshTexts();

    std::function<void()> onImportReference;
    std::function<void()> onClearReference;
    std::function<void()> onExportReport;

    juce::TextButton& getImportButton() noexcept { return refImport; }
    juce::TextButton& getClearButton() noexcept { return refClear; }
    juce::TextButton& getExportButton() noexcept { return reportButton; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void layoutForPage();
    void paintLoudness (juce::Graphics&);
    void paintTruePeak (juce::Graphics&);
    void paintSpectrumExtras (juce::Graphics&);
    void paintStereoExtras (juce::Graphics&);
    void paintDelivery (juce::Graphics&);
    void paintTimeline (juce::Graphics&);
    void paintReferenceBar (juce::Graphics&, juce::Rectangle<int>);

    PageId page = PageId::loudness;
    UiState state;
    juce::String refStatus;
    bool refReady = false;
    int lastTimelineClick = -1;
    int tick = 0;

    LoudnessTiles tiles;
    LevelBars levelBars;
    HistoryGraph history;
    SpectrumDisplay spectrum;
    BandBalanceDisplay bands;
    StereoMeters meters;
    IssueList issues;

    juce::TextButton refImport, refClear, reportButton;
};
} // namespace ml
