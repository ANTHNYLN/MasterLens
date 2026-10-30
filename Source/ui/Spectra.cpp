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

    const float leftPad = 40.0f, bottomPad = 20.0f, topPad = 16.0f, rightPad = 8.0f;
    auto plot = area.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                    .withTrimmedTop (topPad).withTrimmedBottom (bottomPad);

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    const auto yForDb = [plot] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 106.0f);
        return plot.getBottom() - t * plot.getHeight();
    };

    g.setFont (monoFont (tokens::unit));

    for (float db = -100.0f; db <= 0.1f; db += 20.0f)
    {
        const float y = yForDb (db);

        g.setColour (db == 0.0f ? theme::border : theme::borderSoft);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (theme::textFaint);
        g.drawText (juce::String ((int) db),
                    juce::Rectangle<float> (0.0f, y - 6.0f, leftPad - 8.0f, 12.0f),
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

    g.setFont (uiFont (tokens::unit));
    g.setColour (theme::textFaint);
    g.drawText ("dBFS", juce::Rectangle<float> (0.0f, plot.getY() - 14.0f, leftPad, 12.0f),
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
    cachedThemeRevision = theme::revision();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull() || cachedThemeRevision != theme::revision())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    if (analyzer == nullptr)
        return;

    auto area = getLocalBounds().toFloat();

    const float leftPad = 40.0f, bottomPad = 20.0f, topPad = 16.0f, rightPad = 8.0f;
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

    const int steps = juce::jlimit (140, 760, getWidth());

    const double logRange = std::log (20000.0 / 20.0);

    std::vector<float> values ((size_t) steps + 1, -100.0f);
    std::vector<float> holds ((size_t) steps + 1, -100.0f);

    // Sample the already fractional-octave smoothed curve with cubic
    // interpolation. No per-pixel max() any more - that was what produced the
    // jagged low end even after the FFT resolution was raised.
    for (int px = 0; px <= steps; ++px)
    {
        const double f = 20.0 * std::exp (logRange * ((double) px + 0.5) / (double) steps);

        values[(size_t) px] = analyzer->getMagnitudeAt (f);
        holds[(size_t) px] = analyzer->getPeakAt (f);
    }

    const auto xAt = [&] (int px)
    {
        return xFor ((double) px / (double) steps);
    };

    const auto makeSmoothPath = [&] (const std::vector<float>& data)
    {
        juce::Path p;

        if (steps < 3)
            return p;

        const auto pointAt = [&] (int index)
        {
            const int clamped = juce::jlimit (0, steps, index);
            return juce::Point<float> (xAt (clamped), yForDb (data[(size_t) clamped]));
        };

        p.startNewSubPath (pointAt (0));

        for (int i = 0; i < steps; ++i)
        {
            const auto p0 = pointAt (i - 1);
            const auto p1 = pointAt (i);
            const auto p2 = pointAt (i + 1);
            const auto p3 = pointAt (i + 2);

            const auto c1 = p1 + (p2 - p0) / 6.0f;
            const auto c2 = p2 - (p3 - p1) / 6.0f;

            p.cubicTo (c1, c2, p2);
        }

        return p;
    };

    const auto curve = makeSmoothPath (values);
    const auto holdPath = makeSmoothPath (holds);

    juce::Path filled (curve);
    filled.lineTo (plot.getRight(), plot.getBottom());
    filled.lineTo (plot.getX(), plot.getBottom());
    filled.closeSubPath();

    g.setColour (theme::accent.withAlpha (0.10f));
    g.fillPath (filled);

    g.setColour (theme::accent);
    g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    g.setColour (theme::violet.withAlpha (0.45f));
    g.strokePath (holdPath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    int peakBin = 1;
    float peakDb = -100.0f;

    for (int i = 1; i < numBins; ++i)
        if (mags[i] > peakDb)
        {
            peakDb = mags[i];
            peakBin = i;
        }

    g.setFont (monoFont (tokens::unit));
    g.setColour (theme::textDim);
    g.drawText (T ("峰值 ", "Peak ") + fmt1 (peakDb) + " dBFS  @ " + formatHz (peakBin * binHz),
                plot.withHeight (14.0f).translated (0.0f, -15.0f),
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
    const int labelH = 16;

    auto plot = getLocalBounds().withTrimmedTop (14).withTrimmedBottom (labelH);
    const float zeroY = (float) plot.getCentreY();

    g.setColour (theme::borderSoft);
    g.drawHorizontalLine (juce::roundToInt (zeroY), 0.0f, (float) w);

    for (int i = 0; i < n; ++i)
    {
        const auto& info = SpectrumAnalyzer::getBandInfo (i);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (T (info.label, info.labelEn),
                    juce::Rectangle<int> (i * colW, h - labelH, colW, labelH),
                    juce::Justification::centred);
    }

    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText ("dB", 4, 0, 24, 13, juce::Justification::centredLeft);

    backgroundDirty = false;
    cachedThemeRevision = theme::revision();
}

void BandBalanceDisplay::paint (juce::Graphics& g)
{
    if (backgroundDirty || background.isNull() || cachedThemeRevision != theme::revision())
        ensureBackground();

    g.drawImage (background, getLocalBounds().toFloat());

    const int n = SpectrumAnalyzer::numBands;
    devs.resize ((size_t) n, 0.0f);

    const int colW = getWidth() / n;
    const int labelH = 16;
    const int valueH = 14;

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
            g.setFont (monoFont (tokens::unit));
            g.drawText (sgn (raw, 1),
                        juce::Rectangle<int> (cell.getX() - 2, 0, cell.getWidth() + 4, valueH),
                        juce::Justification::centred);
        }
    }
}

} // namespace ml
