#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <set>
#include <vector>

#include "../Text.h"
#include "../dsp/AnalysisEngine.h"
#include "../dsp/SpectrumAnalyzer.h"
#include "Advisor.h"
#include "Localisation.h"
#include "Theme.h"

namespace ml
{
/** White rounded card with a title row. */
class Card : public juce::Component
{
public:
    Card (juce::String titleText = {}, juce::String subtitleText = {});

    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setTitle (juce::String s) { title = std::move (s); repaint(); }
    void setSubtitle (juce::String s) { subtitle = std::move (s); repaint(); }

    juce::Rectangle<int> contentArea() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (juce::Rectangle<int>)> onLayout;

private:
    juce::String title, subtitle;
    juce::Colour accent { theme::accent };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Card)
};

//==============================================================================
/** 2x2 tiles: integrated loudness, true peak, LRA, PLR. */
class LoudnessTiles : public juce::Component
{
public:
    void setValues (const StatsSnapshot& s, float targetLufs, float ceilingDb);
    void paint (juce::Graphics&) override;

private:
    void paintTile (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                    const juce::String& value, const juce::String& unit,
                    const juce::String& hint, juce::Colour valueColour);

    StatsSnapshot stats;
    float target = -14.0f;
    float ceiling = -1.0f;
};

/** Momentary / short-term horizontal bars. */
class LevelBars : public juce::Component
{
public:
    void setValues (const StatsSnapshot& s, float targetLufs);
    void paint (juce::Graphics&) override;

private:
    void paintBar (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                   float valueDb, juce::Colour colour);

    StatsSnapshot stats;
    float target = -14.0f;
};

/** Correlation, stereo width, low-band mono-ness and artefact counters. */
class StereoMeters : public juce::Component
{
public:
    void setValues (const StatsSnapshot& s);
    void paint (juce::Graphics&) override;

private:
    void paintMeter (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                     const juce::String& readout, float normalised, juce::Colour fill,
                     float marker = -1.0f);

    StatsSnapshot stats;
};

//==============================================================================
/** Real-time log-frequency spectrum with peak hold.

    The grid, axis labels and any static text are cached in an off-screen image
    and only rebuilt on resize; each frame only the curve is drawn.
*/
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();

    void setAnalyzer (const SpectrumAnalyzer* a) { analyzer = a; }
    void paint (juce::Graphics&) override;
    void resized() override { backgroundDirty = true; }
    void refreshLanguage() { backgroundDirty = true; repaint(); }

private:
    void ensureBackground();
    void drawGrid (juce::Graphics&);

    const SpectrumAnalyzer* analyzer = nullptr;
    juce::Image background;
    bool backgroundDirty = true;
};

/** Ten band tonal balance deviation bars (pink-noise / octave reference). */
class BandBalanceDisplay : public juce::Component
{
public:
    BandBalanceDisplay();

    void setDeviations (std::vector<float> d) { devs = std::move (d); repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override { backgroundDirty = true; }
    void refreshLanguage() { backgroundDirty = true; repaint(); }

private:
    void ensureBackground();

    std::vector<float> devs;
    juce::Image background;
    bool backgroundDirty = true;
};

/** Short-term loudness history with the target band. */
class HistoryGraph : public juce::Component
{
public:
    HistoryGraph();

    void setData (std::vector<float> curve, float targetLufs);
    void paint (juce::Graphics&) override;
    void resized() override { backgroundDirty = true; }
    void refreshLanguage() { backgroundDirty = true; repaint(); }

private:
    void ensureBackground();

    std::vector<float> data;
    float target = -14.0f;
    juce::Image background;
    bool backgroundDirty = true;
};

//==============================================================================
/** Scrollable list of diagnosed issues with collapsible details. */
class IssueList : public juce::Component
{
public:
    IssueList();

    void setIssues (std::vector<Issue> items);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content : public juce::Component
    {
    public:
        Content() { setOpaque (true); }

        void setItems (std::vector<Issue>, const std::set<juce::String>* expanded);
        int totalHeight() const noexcept;
        int rowHeightAt (int index) const noexcept;
        int rowTop (int index) const noexcept;
        int indexAtY (int y) const noexcept;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (const juce::String&)> onRowClicked;

        static constexpr int collapsedHeight = 46;
        static constexpr int detailExtra = 58;

    private:
        std::vector<Issue> items;
        std::vector<int> rowHeights;
        const std::set<juce::String>* expandedIds = nullptr;
    };

    Content content;
    juce::Viewport viewport;
    std::set<juce::String> expanded;
    std::vector<Issue> viewIssuesCache;
    juce::String lastSignature;

public:
    /** Snapshot helper: opens the first row so the expanded layout can be verified. */
    void expandFirstForSnapshot();
};

//==============================================================================
/** Modal-ish "About" overlay: product, developer, organisation, standards. */
class AboutPanel : public juce::Component
{
public:
    AboutPanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setVisibleAndFocus (bool shouldBeVisible);

    std::function<void()> onClose;

private:
    juce::TextButton closeButton;
};
} // namespace ml
