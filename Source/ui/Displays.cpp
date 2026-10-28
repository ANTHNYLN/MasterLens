#include "Widgets.h"

namespace ml
{
static juce::String formatHz (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

//==============================================================================
void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    const float leftPad = 36.0f, bottomPad = 17.0f, topPad = 12.0f, rightPad = 6.0f;

    auto plot = area.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                    .withTrimmedTop (topPad).withTrimmedBottom (bottomPad);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    constexpr double fMin = 20.0, fMax = 20000.0;
    constexpr float dbMin = -100.0f, dbMax = 6.0f;

    const auto xForFreq = [plot] (double f)
    {
        const double t = std::log (f / fMin) / std::log (fMax / fMin);
        return plot.getX() + (float) juce::jlimit (0.0, 1.0, t) * plot.getWidth();
    };

    const auto yForDb = [plot] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - dbMin) / (dbMax - dbMin));
        return plot.getBottom() - t * plot.getHeight();
    };

    // --- grid ---------------------------------------------------------------
    g.setFont (monoFont (9.5f));

    for (float db = -100.0f; db <= 0.1f; db += 20.0f)
    {
        const float y = yForDb (db);

        g.setColour (db == 0.0f ? theme::border : theme::borderSoft);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (theme::textFaint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (0.0f, y - 6.0f, leftPad - 6.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const double freqTicks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (double f : freqTicks)
    {
        const float x = xForFreq (f);
        g.setColour (theme::borderSoft);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        g.setColour (theme::textFaint);
        const auto label = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0);
        auto labelRect = juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 3.0f, 36.0f, 12.0f);
        labelRect.setX (juce::jlimit (plot.getX(), plot.getRight() - labelRect.getWidth(), labelRect.getX()));
        g.drawText (label, labelRect, juce::Justification::centred);
    }

    const auto hint = uiFont (10.0f);
    g.setFont (hint);
    g.setColour (theme::textFaint);
    g.drawText ("dBFS", juce::Rectangle<float> (0.0f, plot.getY() - 13.0f, leftPad, 12.0f),
                juce::Justification::centredRight);

    if (analyzer == nullptr)
        return;

    const auto& peakInfo = analyzer->getBandDeviations();
    juce::ignoreUnused (peakInfo);

    const int numBins = analyzer->getNumBins();
    const double binHz = analyzer->getBinHz();
    const float* mags = analyzer->getMagnitudes();
    const float* hold = analyzer->getPeakHold();

    if (numBins < 4)
        return;

    const float plotW = plot.getWidth();
    const int steps = juce::jmax (2, (int) plotW);

    std::vector<float> values ((size_t) steps + 1, -100.0f);
    std::vector<float> holds ((size_t) steps + 1, -100.0f);

    for (int px = 0; px <= steps; ++px)
    {
        const double f0 = fMin * std::pow (fMax / fMin, (double) px / (double) steps);
        const double f1 = fMin * std::pow (fMax / fMin, (double) (px + 1) / (double) steps);

        int b0 = (int) std::floor (f0 / binHz);
        int b1 = (int) std::ceil (f1 / binHz);

        b0 = juce::jlimit (1, numBins - 1, b0);
        b1 = juce::jlimit (b0 + 1, numBins, b1);

        float value = -100.0f, held = -100.0f;

        for (int b = b0; b < b1; ++b)
        {
            value = juce::jmax (value, mags[b]);
            held = juce::jmax (held, hold[b]);
        }

        values[(size_t) px] = value;
        holds[(size_t) px] = held;
    }

    // light smoothing keeps the low end readable without hiding narrow peaks
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 1; i < steps; ++i)
            values[(size_t) i] = 0.25f * values[(size_t) (i - 1)]
                               + 0.5f * values[(size_t) i]
                               + 0.25f * values[(size_t) (i + 1)];

    juce::Path curve, holdPath;

    for (int px = 0; px <= steps; ++px)
    {
        const float x = plot.getX() + plot.getWidth() * (float) px / (float) steps;
        const float y = yForDb (values[(size_t) px]);
        const float yh = yForDb (holds[(size_t) px]);

        if (px == 0)
        {
            curve.startNewSubPath (x, y);
            holdPath.startNewSubPath (x, yh);
        }
        else
        {
            curve.lineTo (x, y);
            holdPath.lineTo (x, yh);
        }
    }


    // --- filled spectrum ----------------------------------------------------
    {
        juce::Path filled (curve);
        filled.lineTo (plot.getRight(), plot.getBottom());
        filled.lineTo (plot.getX(), plot.getBottom());
        filled.closeSubPath();

        g.setGradientFill (juce::ColourGradient (theme::accent.withAlpha (0.28f), 0.0f, plot.getY(),
                                                 theme::accent.withAlpha (0.02f), 0.0f, plot.getBottom(),
                                                 false));
        g.fillPath (filled);

        g.setColour (theme::accent);
        g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (theme::violet.withAlpha (0.55f));
        g.strokePath (holdPath, juce::PathStrokeType (1.0f));
    }

    // --- peak readout -------------------------------------------------------
    int peakBin = 1;
    float peakDb = -100.0f;

    for (int i = 1; i < numBins; ++i)
        if (mags[i] > peakDb)
        {
            peakDb = mags[i];
            peakBin = i;
        }

    g.setFont (monoFont (10.0f));
    g.setColour (theme::textDim);
    g.drawText (T ("峰值 ", "Peak ") + fmt1 (peakDb) + " dBFS  @ " + formatHz (peakBin * binHz),
                plot.withHeight (14.0f).translated (0.0f, -12.0f),
                juce::Justification::centredRight);
}

//==============================================================================
void BandBalanceDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    const int n = SpectrumAnalyzer::numBands;

    devs.resize ((size_t) n, 0.0f);

    const int colW = b.getWidth() / n;
    const int labelH = 13;
    const int valueH = 12;

    auto plot = b.withTrimmedTop (valueH).withTrimmedBottom (labelH);
    const float zeroY = (float) plot.getCentreY();
    const float halfH = plot.getHeight() * 0.5f - 2.0f;
    constexpr float maxDev = 12.0f;

    g.setColour (theme::borderSoft);
    g.drawHorizontalLine (juce::roundToInt (zeroY), 0, b.getWidth());

    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<int> (i * colW, 0, colW, b.getHeight());

        const float raw = devs[(size_t) i];
        const float dev = juce::jlimit (-maxDev, maxDev, raw);
        const float mag = std::abs (dev) / maxDev;
        const float h = juce::jmax (1.0f, mag * halfH);

        const auto colour = std::abs (raw) < 3.5f ? theme::accent
                          : (std::abs (raw) < 6.0f ? theme::warn : theme::bad);

        const float barW = juce::jmax (7.0f, colW * 0.34f);
        juce::Rectangle<float> bar (cell.getCentreX() - barW * 0.5f,
                                    dev >= 0.0f ? zeroY - h : zeroY,
                                    barW, h);

        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (bar, 2.0f);

        if (std::abs (raw) >= 1.5f)
        {
            g.setColour (colour);
            g.setFont (monoFont (8.5f));
            g.drawText ((raw >= 0.0f ? "+" : "") + juce::String ((double) raw, 1),
                        juce::Rectangle<int> (cell.getX() - 2, 0, cell.getWidth() + 4, valueH),
                        juce::Justification::centred);
        }

        const auto& bandInfo = SpectrumAnalyzer::getBandInfo (i);
        g.setColour (theme::textDim);
        g.setFont (uiFont (9.5f));
        g.drawText (T (bandInfo.label, bandInfo.labelEn),
                    juce::Rectangle<int> (cell.getX(), b.getBottom() - labelH, cell.getWidth(), labelH),
                    juce::Justification::centred);
    }
}

//==============================================================================
void HistoryGraph::setData (std::vector<float> curve, float targetLufs)
{
    data = std::move (curve);
    target = targetLufs;
    repaint();
}

void HistoryGraph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    if (b.getWidth() < 60 || b.getHeight() < 40)
        return;

    auto plot = b.withTrimmedLeft (32).withTrimmedRight (52).withTrimmedTop (8).withTrimmedBottom (14);

    constexpr float windowSeconds = 180.0f;
    const int maxPoints = (int) (windowSeconds * 10.0f);

    const float yMax = 0.0f;
    const float yMin = juce::jmin (-36.0f, target - 14.0f);

    const auto yFor = [plot, yMin, yMax] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - yMin) / (yMax - yMin));
        return plot.getBottom() - t * plot.getHeight();
    };

    g.setFont (monoFont (9.0f));

    for (float db = yMax; db >= yMin - 0.1f; db -= 12.0f)
    {
        const float y = yFor (db);

        g.setColour (theme::borderSoft);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (theme::textFaint);
        g.drawText (juce::String ((int) db),
                    juce::Rectangle<float> (0.0f, y - 5.0f, 27.0f, 11.0f),
                    juce::Justification::centredRight);
    }

    const float bandTop = yFor (target + 1.0f);
    const float bandBottom = yFor (target - 1.0f);
    g.setColour (theme::good.withAlpha (0.12f));
    g.fillRect (juce::Rectangle<float> (plot.getX(), bandTop, plot.getWidth(),
                                        juce::jmax (2.0f, bandBottom - bandTop)));

    const float targetY = yFor (target);
    g.setColour (theme::good.withAlpha (0.55f));

    for (float x = plot.getX(); x < plot.getRight(); x += 8.0f)
        g.fillRect (x, targetY - 0.6f, 4.0f, 1.2f);

    if (data.size() >= 2)
    {
        const int count = juce::jmin ((int) data.size(), maxPoints);
        const int offset = (int) data.size() - count;

        juce::Path path;
        bool started = false;
        bool haveFirst = false;
        float firstX = plot.getX();
        float lastX = plot.getX();

        for (int i = 0; i < count; ++i)
        {
            const float value = data[(size_t) (offset + i)];
            const float x = plot.getX() + plot.getWidth()
                            * (float) (maxPoints - count + i) / (float) maxPoints;

            if (value <= -99.0f)
            {
                started = false;
                continue;
            }

            const float y = yFor (value);

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;

                if (! haveFirst)
                {
                    firstX = x;
                    haveFirst = true;
                }
            }
            else
            {
                path.lineTo (x, y);
            }

            lastX = x;
        }

        if (haveFirst)
        {
            juce::Path filled (path);
            filled.lineTo (lastX, plot.getBottom());
            filled.lineTo (firstX, plot.getBottom());
            filled.closeSubPath();

            g.setGradientFill (juce::ColourGradient (theme::violet.withAlpha (0.22f), 0.0f, plot.getY(),
                                                     theme::violet.withAlpha (0.02f), 0.0f, plot.getBottom(),
                                                     false));
            g.fillPath (filled);

            g.setColour (theme::violet);
            g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }

    g.setFont (uiFont (9.0f));
    g.setColour (theme::textFaint);
    g.drawText ("-3 min", juce::Rectangle<int> (plot.getX(), plot.getBottom() + 1, 40, 12),
                juce::Justification::centredLeft);
    g.drawText ("-2 min", juce::Rectangle<int> (plot.getCentreX() - 20, plot.getBottom() + 1, 40, 12),
                juce::Justification::centred);
    g.drawText ("-1 min", juce::Rectangle<int> (plot.getRight() - 82, plot.getBottom() + 1, 40, 12),
                juce::Justification::centred);
    g.drawText (T ("现在", "now"), juce::Rectangle<int> (plot.getRight() - 40, plot.getBottom() + 1, 40, 12),
                juce::Justification::centredRight);

    if (! data.empty())
    {
        const float latest = data.back();
        const auto colour = latest <= -99.0f ? theme::textFaint
                          : (std::abs (latest - target) <= 2.0f ? theme::good : theme::warn);

        g.setColour (colour);
        g.setFont (monoFont (13.0f, true));
        g.drawText (latest <= -99.0f ? "--" : fmt1 (latest),
                    juce::Rectangle<int> (plot.getRight() + 6, plot.getCentreY() - 9, 44, 17),
                    juce::Justification::centredRight);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (9.5f));
        g.drawText ("LUFS", juce::Rectangle<int> (plot.getRight() + 6, plot.getCentreY() + 8, 44, 12),
                    juce::Justification::centredRight);
    }
}

//==============================================================================
IssueList::IssueList()
{
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    viewport.setViewedComponent (&content, false);
    viewport.setOpaque (false);
    addAndMakeVisible (viewport);
}

void IssueList::setIssues (std::vector<Issue> items)
{
    juce::String signature;

    for (const auto& i : items)
        signature << i.id << '|' << i.title << '|' << i.detail << '|'
                  << (int) i.severity << '|' << (i.timed ? i.detectedAt : 0.0) << ';';

    if (signature == lastSignature)
        return;

    lastSignature = signature;

    const int scroll = viewport.getViewPositionY();

    content.setItems (std::move (items));

    content.setSize (juce::jmax (120, getWidth() - 10),
                     juce::jmax (content.totalHeight(), viewport.getHeight()));

    viewport.setViewPosition (0, juce::jmin (scroll, juce::jmax (0, content.getHeight() - viewport.getHeight())));
}

void IssueList::resized()
{
    viewport.setBounds (getLocalBounds());

    content.setSize (juce::jmax (120, viewport.getMaximumVisibleWidth()),
                     juce::jmax (content.totalHeight(), viewport.getHeight()));
}

void IssueList::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

//==============================================================================
void IssueList::Content::setItems (std::vector<Issue> newItems)
{
    items = std::move (newItems);
    setSize (getWidth(), totalHeight());
    repaint();
}

int IssueList::Content::totalHeight() const noexcept
{
    return (int) items.size() * rowHeight;
}

void IssueList::Content::paint (juce::Graphics& g)
{
    if (items.empty())
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (12.0f));
        g.drawText (T ("等待音频输入 — 将插件插在总线 / 母带上开始分析",
                       "Waiting for audio - insert the plug-in on a bus or the master"),
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const auto row = juce::Rectangle<int> (0, (int) i * rowHeight, getWidth(), rowHeight);

        if (i > 0)
        {
            g.setColour (theme::borderSoft);
            g.drawHorizontalLine (row.getY(), 6, getWidth() - 6);
        }

        const auto colour = severityColour (item.severity);

        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (8.0f, (float) row.getY() + 13.0f, 7.0f, 7.0f));

        g.setColour (theme::text);
        g.setFont (uiFont (12.5f, true));
        g.drawText (item.title, 24, row.getY() + 7, getWidth() - 24 - 92, 17,
                    juce::Justification::centredLeft);

        const juce::String tag = item.severity == Severity::critical ? T ("需要处理", "Action")
                               : item.severity == Severity::warn ? T ("建议检查", "Review")
                               : item.severity == Severity::info ? T ("可优化", "Improve")
                                                                  : T ("良好", "Good");
        const auto chip = juce::Rectangle<int> (getWidth() - 78, row.getY() + 8, 64, 17);

        g.setColour (colour.withAlpha (0.12f));
        g.fillRoundedRectangle (chip.toFloat(), 8.0f);
        g.setColour (colour);
        g.setFont (uiFont (9.5f));
        g.drawText (tag, chip, juce::Justification::centred);

        if (item.timed)
        {
            g.setColour (theme::textFaint);
            g.setFont (monoFont (9.5f));
            g.drawText (fmtTime (item.detectedAt) + T (" 检出", " detected"),
                        juce::Rectangle<int> (getWidth() - 164, row.getY() + 27, 150, 12),
                        juce::Justification::centredRight);
        }

        g.setColour (theme::textDim);
        g.setFont (uiFont (10.8f));
        g.drawFittedText (item.detail, 24, row.getY() + 26,
                          getWidth() - 24 - 18 - (item.timed ? 150 : 0), 30,
                          juce::Justification::topLeft, 2);
    }
}
} // namespace ml



