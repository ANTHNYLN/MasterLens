#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
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
/** Real-time log-frequency spectrum with peak hold. */
class SpectrumDisplay : public juce::Component
{
public:
    void setAnalyzer (const SpectrumAnalyzer* a) { analyzer = a; }
    void paint (juce::Graphics&) override;

private:
    const SpectrumAnalyzer* analyzer = nullptr;
};

/** Ten band tonal balance deviation bars (pink-noise / octave reference). */
class BandBalanceDisplay : public juce::Component
{
public:
    void setDeviations (std::vector<float> d) { devs = std::move (d); repaint(); }
    void paint (juce::Graphics&) override;

private:
    std::vector<float> devs;
};

/** Short-term loudness history with the target band. */
class HistoryGraph : public juce::Component
{
public:
    void setData (std::vector<float> curve, float targetLufs);
    void paint (juce::Graphics&) override;

private:
    std::vector<float> data;
    float target = -14.0f;
};

/** Scrollable list of diagnosed issues. */
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
        void setItems (std::vector<Issue>);
        int totalHeight() const noexcept;
        void paint (juce::Graphics&) override;

        static constexpr int rowHeight = 60;

    private:
        std::vector<Issue> items;
    };

    Content content;
    juce::Viewport viewport;
    juce::String lastSignature;
};
} // namespace ml
