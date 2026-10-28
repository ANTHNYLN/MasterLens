#include "Widgets.h"
#include "../Version.h"

namespace ml
{
static juce::String formatHz (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

//==============================================================================
SpectrumDisplay::SpectrumDisplay()
{
    setOpaque (true);
}

void SpectrumDisplay::drawGrid (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    const float leftPad = 38.0f, bottomPad = 19.0f, topPad = 14.0f, rightPad = 6.0f;
    auto plot = area.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                    .withTrimmedTop (topPad).withTrimmedBottom (bottomPad);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    const auto yForDb = [plot] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 106.0f);
        return plot.getBottom() - t * plot.getHeight();
    };

    g.setFont (monoFont (9.5f));

    for (float db = -100.0f; db <= 0.1f; db += 20.0f)
    {
        const float y = yForDb (db);

        g.setColour (db == 0.0f ? theme::border : theme::borderSoft);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (theme::textFaint);
        g.drawText (juce::String ((int) db),
                    juce::Rectangle<float> (0.0f, y - 6.0f, leftPad - 6.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const double freqTicks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (double f : freqTicks)
    {
        const float x = plot.getX() + plot.getWidth()
                        * (float) (std::log (f / 20.0) / std::log (20000.0 / 20.0));

        g.setColour (theme::borderSoft);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        g.setColour (theme::textFaint);
        const auto label = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0);
        auto labelRect = juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 4.0f, 36.0f, 12.0f);
        labelRect.setX (juce::jlimit (plot.getX(), plot.getRight() - labelRect.getWidth(), labelRect.getX()));
        g.drawText (label, labelRect, juce::Justification::centred);
    }

    g.setFont (uiFont (10.0f));
    g.setColour (theme::textFaint);
    g.drawText ("dBFS", juce::Rectangle<float> (0.0f, plot.getY() - 13.0f, leftPad, 12.0f),
                juce::Justification::centredRight);
}

void SpectrumDisplay::ensureBackground()
{
    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    const float scale = juce::jmax (1.0f, getDesktopScaleFactor());

    background = juce::Image (juce::Image::ARGB,
                              juce::roundToInt ((float) w * scale),
                              juce::roundToInt ((float) h * scale),
                              true);

    juce::Graphics bg (background);
    bg.addTransform (juce::AffineTransform::scale (scale));
    bg.fillAll (theme::card);
    drawGrid (bg);

    backgroundDirty = false;
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    if (analyzer == nullptr)
        return;

    auto area = getLocalBounds().toFloat();

    const float leftPad = 38.0f, bottomPad = 19.0f, topPad = 14.0f, rightPad = 6.0f;
    auto plot = area.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                    .withTrimmedTop (topPad).withTrimmedBottom (bottomPad);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    const auto xFor = [plot] (double t)
    {
        return plot.getX() + (float) juce::jlimit (0.0, 1.0, t) * plot.getWidth();
    };

    const auto yForDb = [plot] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 106.0f);
        return plot.getBottom() - t * plot.getHeight();
    };

    const int numBins = analyzer->getNumBins();
    const double binHz = analyzer->getBinHz();
    const float* mags = analyzer->getMagnitudes();
    const float* hold = analyzer->getPeakHold();

    if (numBins < 4)
        return;

    const int steps = juce::jlimit (120, 640, getWidth());

    std::vector<float> values ((size_t) steps + 1, -100.0f);
    std::vector<float> holds ((size_t) steps + 1, -100.0f);

    const double logRange = std::log (20000.0 / 20.0);

    for (int px = 0; px <= steps; ++px)
    {
        const double f0 = 20.0 * std::exp (logRange * (double) px / (double) steps);
        const double f1 = 20.0 * std::exp (logRange * (double) (px + 1) / (double) steps);

        int b0 = juce::jlimit (1, numBins - 1, (int) std::floor (f0 / binHz));
        int b1 = juce::jlimit (b0 + 1, numBins, (int) std::ceil (f1 / binHz));

        float value = -100.0f, held = -100.0f;

        for (int b = b0; b < b1; ++b)
        {
            value = juce::jmax (value, mags[b]);
            held = juce::jmax (held, hold[b]);
        }

        values[(size_t) px] = value;
        holds[(size_t) px] = held;
    }

    for (int i = 1; i < steps; ++i)
        values[(size_t) i] = 0.25f * values[(size_t) (i - 1)]
                           + 0.5f * values[(size_t) i]
                           + 0.25f * values[(size_t) (i + 1)];

    juce::Path curve, holdPath;

    for (int px = 0; px <= steps; ++px)
    {
        const float x = xFor ((double) px / (double) steps);
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

    juce::Path filled (curve);
    filled.lineTo (plot.getRight(), plot.getBottom());
    filled.lineTo (plot.getX(), plot.getBottom());
    filled.closeSubPath();

    g.setColour (theme::accent.withAlpha (0.12f));
    g.fillPath (filled);

    g.setColour (theme::accent);
    g.strokePath (curve, juce::PathStrokeType (1.6f));

    g.setColour (theme::violet.withAlpha (0.5f));
    g.strokePath (holdPath, juce::PathStrokeType (1.0f));

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
                plot.withHeight (14.0f).translated (0.0f, -13.0f),
                juce::Justification::centredRight);
}

//==============================================================================
BandBalanceDisplay::BandBalanceDisplay()
{
    setOpaque (true);
}

void BandBalanceDisplay::ensureBackground()
{
    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    const float scale = juce::jmax (1.0f, getDesktopScaleFactor());

    background = juce::Image (juce::Image::ARGB,
                              juce::roundToInt ((float) w * scale),
                              juce::roundToInt ((float) h * scale),
                              true);

    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (theme::card);

    const int n = SpectrumAnalyzer::numBands;
    const int colW = w / n;
    const int labelH = 15;

    auto plot = getLocalBounds().withTrimmedTop (13).withTrimmedBottom (labelH);
    const float zeroY = (float) plot.getCentreY();

    g.setColour (theme::borderSoft);
    g.drawHorizontalLine (juce::roundToInt (zeroY), 0, w);

    for (int i = 0; i < n; ++i)
    {
        const auto& info = SpectrumAnalyzer::getBandInfo (i);

        g.setColour (theme::textDim);
        g.setFont (uiFont (9.5f));
        g.drawText (T (info.label, info.labelEn),
                    juce::Rectangle<int> (i * colW, h - labelH, colW, labelH),
                    juce::Justification::centred);
    }

    backgroundDirty = false;
}

void BandBalanceDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    const int n = SpectrumAnalyzer::numBands;
    devs.resize ((size_t) n, 0.0f);

    const int colW = getWidth() / n;
    const int labelH = 15;
    const int valueH = 13;

    auto plot = getLocalBounds().withTrimmedTop (valueH).withTrimmedBottom (labelH);
    const float zeroY = (float) plot.getCentreY();
    const float halfH = plot.getHeight() * 0.5f - 2.0f;
    constexpr float maxDev = 12.0f;

    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<int> (i * colW, 0, colW, getHeight());

        const float raw = devs[(size_t) i];
        const float dev = juce::jlimit (-maxDev, maxDev, raw);
        const float mag = std::abs (dev) / maxDev;
        const float h = juce::jmax (1.0f, mag * halfH);

        const auto colour = std::abs (raw) < 3.5f ? theme::accent
                          : (std::abs (raw) < 6.0f ? theme::warn : theme::bad);

        const float barW = juce::jmax (8.0f, colW * 0.34f);
        juce::Rectangle<float> bar (cell.getCentreX() - barW * 0.5f,
                                    dev >= 0.0f ? zeroY - h : zeroY,
                                    barW, h);

        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (bar, 2.0f);

        if (std::abs (raw) >= 1.5f)
        {
            g.setColour (colour);
            g.setFont (monoFont (9.0f));
            g.drawText ((raw >= 0.0f ? "+" : "") + juce::String ((double) raw, 1),
                        juce::Rectangle<int> (cell.getX() - 2, 0, cell.getWidth() + 4, valueH),
                        juce::Justification::centred);
        }
    }
}

//==============================================================================
HistoryGraph::HistoryGraph()
{
    setOpaque (true);
}

void HistoryGraph::ensureBackground()
{
    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    const float scale = juce::jmax (1.0f, getDesktopScaleFactor());

    background = juce::Image (juce::Image::ARGB,
                              juce::roundToInt ((float) w * scale),
                              juce::roundToInt ((float) h * scale),
                              true);

    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (theme::card);

    auto b = getLocalBounds();
    auto plot = b.withTrimmedLeft (34).withTrimmedRight (56).withTrimmedTop (8).withTrimmedBottom (16);

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
                    juce::Rectangle<float> (0.0f, y - 5.0f, 29.0f, 11.0f),
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

    g.setFont (uiFont (9.0f));
    g.setColour (theme::textFaint);
    g.drawText ("-3 min", juce::Rectangle<int> (plot.getX(), plot.getBottom() + 2, 44, 12),
                juce::Justification::centredLeft);
    g.drawText ("-2 min", juce::Rectangle<int> (plot.getCentreX() - 22, plot.getBottom() + 2, 44, 12),
                juce::Justification::centred);
    g.drawText ("-1 min", juce::Rectangle<int> (plot.getRight() - 88, plot.getBottom() + 2, 44, 12),
                juce::Justification::centred);
    g.drawText (T ("现在", "now"), juce::Rectangle<int> (plot.getRight() - 44, plot.getBottom() + 2, 44, 12),
                juce::Justification::centredRight);

    backgroundDirty = false;
}

void HistoryGraph::setData (std::vector<float> curve, float targetLufs)
{
    data = std::move (curve);

    if (std::abs (targetLufs - target) > 0.01f)
    {
        target = targetLufs;
        backgroundDirty = true;
    }

    repaint();
}

void HistoryGraph::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    auto plot = getLocalBounds().withTrimmedLeft (34).withTrimmedRight (56)
                              .withTrimmedTop (8).withTrimmedBottom (16);

    if (plot.getWidth() < 20 || plot.getHeight() < 20)
        return;

    const float yMax = 0.0f;
    const float yMin = juce::jmin (-36.0f, target - 14.0f);

    const auto yFor = [plot, yMin, yMax] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - yMin) / (yMax - yMin));
        return plot.getBottom() - t * plot.getHeight();
    };

    constexpr int maxPoints = 1800;

    if (data.size() >= 2)
    {
        const int count = juce::jmin ((int) data.size(), maxPoints);
        const int offset = (int) data.size() - count;

        juce::Path path;
        bool started = false, haveFirst = false;
        float firstX = (float) plot.getX(), lastX = (float) plot.getX();

        for (int i = 0; i < count; ++i)
        {
            const float value = data[(size_t) (offset + i)];

            if (value <= -99.0f)
            {
                started = false;
                continue;
            }

            const float x = (float) plot.getX() + (float) plot.getWidth()
                            * (float) (maxPoints - count + i) / (float) maxPoints;
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
            filled.lineTo (lastX, (float) plot.getBottom());
            filled.lineTo (firstX, (float) plot.getBottom());
            filled.closeSubPath();

            g.setColour (theme::violet.withAlpha (0.10f));
            g.fillPath (filled);

            g.setColour (theme::violet);
            g.strokePath (path, juce::PathStrokeType (1.6f));
        }
    }

    if (! data.empty())
    {
        const float latest = data.back();
        const auto colour = latest <= -99.0f ? theme::textFaint
                          : (std::abs (latest - target) <= 2.0f ? theme::good : theme::warn);

        g.setColour (colour);
        g.setFont (monoFont (14.0f, true));
        g.drawText (latest <= -99.0f ? "--" : fmt1 (latest),
                    juce::Rectangle<int> (plot.getRight() + 6, plot.getCentreY() - 10, 46, 18),
                    juce::Justification::centredRight);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (9.5f));
        g.drawText ("LUFS", juce::Rectangle<int> (plot.getRight() + 6, plot.getCentreY() + 9, 46, 12),
                    juce::Justification::centredRight);
    }
}

//==============================================================================
IssueList::IssueList()
{
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    viewport.setViewedComponent (&content, false);
    viewport.setOpaque (false);
    addAndMakeVisible (viewport);

    content.onRowClicked = [this] (const juce::String& id)
    {
        if (expanded.count (id) > 0)
            expanded.erase (id);
        else
            expanded.insert (id);

        content.setItems (viewIssuesCache, &expanded);
        content.setSize (juce::jmax (140, getWidth() - 12), content.totalHeight());
        content.repaint();
    };
}

void IssueList::setIssues (std::vector<Issue> items)
{
    juce::String signature;

    for (const auto& i : items)
    {
        // Collapsed rows never show the detail text, so numeric churn inside it
        // must not force a repaint of the whole list.
        const bool open = expanded.count (i.id) > 0;

        signature << i.id << '|' << i.title << '|' << (int) i.severity << '|'
                  << (i.timed ? i.detectedAt : 0.0) << '|'
                  << (open ? i.detail : juce::String()) << ';';
    }

    if (signature == lastSignature)
        return;

    lastSignature = signature;
    viewIssuesCache = std::move (items);

    const int scroll = viewport.getViewPositionY();

    content.setItems (viewIssuesCache, &expanded);
    content.setSize (juce::jmax (140, getWidth() - 12),
                     juce::jmax (content.totalHeight(), viewport.getHeight()));

    viewport.setViewPosition (0, juce::jmin (scroll, juce::jmax (0, content.getHeight() - viewport.getHeight())));
}

void IssueList::resized()
{
    viewport.setBounds (getLocalBounds());

    content.setSize (juce::jmax (140, viewport.getMaximumVisibleWidth()),
                     juce::jmax (content.totalHeight(), viewport.getHeight()));
}

void IssueList::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void IssueList::expandFirstForSnapshot()
{
    if (viewIssuesCache.empty())
        return;

    expanded.insert (viewIssuesCache.front().id);
    content.setItems (viewIssuesCache, &expanded);
    content.setSize (juce::jmax (140, getWidth() - 12), content.totalHeight());
    content.repaint();
}

//==============================================================================
void IssueList::Content::setItems (std::vector<Issue> newItems, const std::set<juce::String>* expanded)
{
    items = std::move (newItems);
    expandedIds = expanded;

    rowHeights.assign (items.size(), collapsedHeight);

    const float available = (float) juce::jmax (200, getWidth() - 92);
    const auto detailFont = uiFont (11.8f);

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (expandedIds == nullptr || expandedIds->count (items[i].id) == 0)
            continue;

        const float textWidth = juce::GlyphArrangement::getStringWidth (detailFont, items[i].detail);
        const int lines = juce::jlimit (1, 4, (int) std::ceil (textWidth / available));
        rowHeights[i] = collapsedHeight + 6 + lines * 18;
    }

    setSize (getWidth(), juce::jmax (1, totalHeight()));
    repaint();
}

int IssueList::Content::rowHeightAt (int index) const noexcept
{
    if (index < 0 || index >= (int) rowHeights.size())
        return 0;

    return rowHeights[(size_t) index];
}

int IssueList::Content::rowTop (int index) const noexcept
{
    int y = 0;

    for (int i = 0; i < index && i < (int) rowHeights.size(); ++i)
        y += rowHeights[(size_t) i];

    return y;
}

int IssueList::Content::totalHeight() const noexcept
{
    int h = 0;

    for (int v : rowHeights)
        h += v;

    return h;
}

int IssueList::Content::indexAtY (int y) const noexcept
{
    int top = 0;

    for (int i = 0; i < (int) items.size(); ++i)
    {
        const int h = rowHeightAt (i);

        if (y >= top && y < top + h)
            return i;

        top += h;
    }

    return -1;
}

void IssueList::Content::mouseDown (const juce::MouseEvent& e)
{
    const int index = indexAtY (e.y);

    if (index >= 0 && onRowClicked != nullptr)
        onRowClicked (items[(size_t) index].id);
}

//==============================================================================
void IssueList::Content::paint (juce::Graphics& g)
{
    g.fillAll (theme::card);

    if (items.empty())
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (12.5f));
        g.drawFittedText (T ("等待音频输入 — 将插件插在总线 / 母带上开始分析",
                             "Waiting for audio - insert the plug-in on a bus or the master"),
                          getLocalBounds().reduced (24), juce::Justification::centred, 2);
        return;
    }

    int y = 0;

    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const int h = rowHeightAt ((int) i);
        const bool open = expandedIds != nullptr && expandedIds->count (item.id) > 0;
        const auto row = juce::Rectangle<int> (0, y, getWidth(), h);
        const auto colour = severityColour (item.severity);

        if (i > 0)
        {
            g.setColour (theme::borderSoft);
            g.drawHorizontalLine (row.getY(), 12, getWidth() - 12);
        }

        g.setColour (colour);
        g.fillEllipse (12.0f, (float) row.getY() + 17.0f, 8.0f, 8.0f);

        g.setColour (theme::text);
        g.setFont (uiFont (13.5f, true));
        g.drawText (item.title, 30, row.getY() + 11, getWidth() - 30 - 156, 20,
                    juce::Justification::centredLeft);

        const juce::String tag = item.severity == Severity::critical ? T ("需要处理", "Action")
                               : item.severity == Severity::warn ? T ("建议检查", "Review")
                               : item.severity == Severity::info ? T ("可优化", "Improve")
                                                                  : T ("良好", "Good");

        const auto chip = juce::Rectangle<int> (getWidth() - 152, row.getY() + 13, 68, 19);
        g.setColour (colour.withAlpha (0.12f));
        g.fillRoundedRectangle (chip.toFloat(), 9.0f);
        g.setColour (colour);
        g.setFont (uiFont (9.5f));
        g.drawText (tag, chip, juce::Justification::centred);

        if (item.timed)
        {
            g.setColour (theme::textFaint);
            g.setFont (monoFont (9.5f));
            g.drawText (fmtTime (item.detectedAt),
                        juce::Rectangle<int> (getWidth() - 82, row.getY() + 16, 50, 14),
                        juce::Justification::centredRight);
        }

        const float cx = (float) getWidth() - 16.0f;
        const float cy = (float) row.getY() + 22.0f;
        juce::Path chevron;

        if (open)
        {
            chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
            chevron.lineTo (cx, cy + 2.5f);
            chevron.lineTo (cx + 4.0f, cy - 2.0f);
        }
        else
        {
            chevron.startNewSubPath (cx - 2.5f, cy - 4.0f);
            chevron.lineTo (cx + 2.0f, cy);
            chevron.lineTo (cx - 2.5f, cy + 4.0f);
        }

        g.setColour (theme::textFaint);
        g.strokePath (chevron, juce::PathStrokeType (1.5f));

        if (open)
        {
            g.setColour (theme::textDim);
            g.setFont (uiFont (11.8f));
            g.drawFittedText (item.detail, 30, row.getY() + 38, getWidth() - 60,
                              juce::jmax (16, h - 44), juce::Justification::topLeft, 4);
        }

        y += h;
    }
}

//==============================================================================
static juce::Rectangle<int> aboutCardBounds (juce::Rectangle<int> b)
{
    const int w = juce::jmin (470, b.getWidth() - 40);
    const int h = juce::jmin (306, b.getHeight() - 40);
    return b.withSizeKeepingCentre (w, h);
}

AboutPanel::AboutPanel()
{
    setOpaque (false);

    closeButton.setButtonText (T ("关闭", "Close"));
    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
    addAndMakeVisible (closeButton);
}

void AboutPanel::setVisibleAndFocus (bool shouldBeVisible)
{
    setVisible (shouldBeVisible);

    if (shouldBeVisible)
    {
        toFront (true);
        closeButton.setButtonText (T ("关闭", "Close"));
    }
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.24f));

    const auto card = aboutCardBounds (getLocalBounds()).toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillRoundedRectangle (card.translated (0.0f, 3.0f), 18.0f);
    g.setColour (theme::card);
    g.fillRoundedRectangle (card, 18.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (card.reduced (0.5f), 18.0f, 1.0f);

    auto inner = card.reduced (28.0f, 24.0f).toNearestInt();

    auto logo = inner.removeFromTop (48).removeFromLeft (48).toFloat();

    g.setGradientFill (juce::ColourGradient (theme::accent, logo.getX(), logo.getY(),
                                             theme::violet, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 13.0f);

    const float bars[] { 0.34f, 0.66f, 0.92f, 0.5f };
    float bx = logo.getX() + 9.0f;

    for (float bh : bars)
    {
        const float hgt = logo.getHeight() * 0.44f * bh + 4.0f;
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillRoundedRectangle (bx, logo.getBottom() - 10.0f - hgt, 4.5f, hgt, 2.0f);
        bx += 8.5f;
    }

    g.setColour (theme::text);
    g.setFont (uiFont (22.0f, true));
    g.drawText (ml::kProductName, (int) logo.getRight() + 14, (int) logo.getY() - 4, 300, 28,
                juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (T ("母带响度与混音质检", "Mastering loudness & mix inspector"),
                (int) logo.getRight() + 14, (int) logo.getY() + 24, 320, 18,
                juce::Justification::centredLeft);

    inner.removeFromTop (16);

    g.setColour (theme::borderSoft);
    g.drawHorizontalLine (inner.getY(), (float) inner.getX(), (float) inner.getRight());
    inner.removeFromTop (10);

    const auto drawRow = [&g, &inner] (const juce::String& label, const juce::String& value,
                                       juce::Colour colour)
    {
        auto r = inner.removeFromTop (27);
        g.setColour (theme::textFaint);
        g.setFont (uiFont (11.0f));
        g.drawText (label, r.removeFromLeft (128), juce::Justification::centredLeft);
        g.setColour (colour);
        g.setFont (uiFont (12.5f, true));
        g.drawText (value, r, juce::Justification::centredLeft);
    };

    drawRow (T ("版本", "Version"), ml::kVersion, theme::text);
    drawRow (T ("开发者", "Developer"), ml::kDeveloper, theme::accent);
    drawRow (T ("机构", "Organisation"), ml::kOrganisation, theme::violet);

    inner.removeFromTop (8);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (10.2f));
    g.drawFittedText (T ("测量标准：ITU-R BS.1770-4 · EBU R128 · EBU Tech 3342\n"
                         "真峰值 4× 过采样插值 · 零延迟 · 不修改音频\n"
                         "Built with JUCE 8",
                         "Standards: ITU-R BS.1770-4 · EBU R128 · EBU Tech 3342\n"
                         "4x oversampled true peak · zero latency · never alters audio\n"
                         "Built with JUCE 8"),
                      inner, juce::Justification::topLeft, 4);
}

void AboutPanel::resized()
{
    const auto card = aboutCardBounds (getLocalBounds());
    closeButton.setBounds (card.getRight() - 124, card.getBottom() - 48, 96, 30);
}

void AboutPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! aboutCardBounds (getLocalBounds()).contains (e.getPosition()))
        if (onClose != nullptr)
            onClose();
}
} // namespace ml









