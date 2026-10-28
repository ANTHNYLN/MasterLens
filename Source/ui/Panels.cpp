#include "Widgets.h"

namespace ml
{
//==============================================================================
Card::Card (juce::String titleText, juce::String subtitleText)
    : title (std::move (titleText)), subtitle (std::move (subtitleText))
{
}

juce::Rectangle<int> Card::contentArea() const
{
    return getLocalBounds().reduced (16, 12).withTrimmedTop (28);
}

void Card::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (juce::Colours::black.withAlpha (0.035f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 15.0f);

    g.setColour (theme::card);
    g.fillRoundedRectangle (r, 15.0f);

    g.setColour (theme::border);
    g.drawRoundedRectangle (r, 15.0f, 1.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (16.0f, 17.0f, 3.0f, 13.0f, 1.5f);

    const auto titleFont = uiFont (14.0f, true);
    const auto subFont = uiFont (11.0f);

    float subWidth = 0.0f;

    if (subtitle.isNotEmpty())
        subWidth = juce::GlyphArrangement::getStringWidth (subFont, subtitle) + 12.0f;

    g.setColour (theme::text);
    g.setFont (titleFont);
    g.drawText (title, 26, 13, getWidth() - 42 - (int) subWidth, 21, juce::Justification::centredLeft);

    if (subtitle.isNotEmpty())
    {
        g.setColour (theme::textFaint);
        g.setFont (subFont);
        g.drawText (subtitle, getWidth() - 16 - (int) subWidth, 14, (int) subWidth, 19,
                    juce::Justification::centredRight);
    }
}

void Card::resized()
{
    if (onLayout != nullptr)
        onLayout (contentArea());
}

//==============================================================================
void LoudnessTiles::setValues (const StatsSnapshot& s, float targetLufs, float ceilingDb)
{
    stats = s;
    target = targetLufs;
    ceiling = ceilingDb;
    repaint();
}

void LoudnessTiles::paintTile (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                               const juce::String& value, const juce::String& unit,
                               const juce::String& hint, juce::Colour valueColour)
{
    const auto r = area.toFloat();

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (r, 11.0f);
    g.setColour (theme::borderSoft);
    g.drawRoundedRectangle (r.reduced (0.5f), 11.0f, 1.0f);

    auto inner = area.reduced (12, 6);

    g.setColour (theme::textDim);
    g.setFont (uiFont (10.5f));
    g.drawText (label, inner.removeFromTop (13), juce::Justification::topLeft);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (9.5f));
    g.drawText (hint, inner.removeFromBottom (12), juce::Justification::bottomLeft);

    const auto valueFont = monoFont (22.0f, true);
    const auto valueWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (valueFont, value));

    g.setColour (valueColour);
    g.setFont (valueFont);
    g.drawText (value, inner.removeFromLeft (valueWidth + 4), juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (9.5f));
    g.drawText (unit, inner.removeFromLeft (44).translated (0, 5), juce::Justification::centredLeft);
}

void LoudnessTiles::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds();
    const int gap = 10;
    const int w = (b.getWidth() - gap) / 2;
    const int h = (b.getHeight() - gap) / 2;

    const auto topLeft     = juce::Rectangle<int> (b.getX(), b.getY(), w, h);
    const auto topRight    = juce::Rectangle<int> (b.getX() + w + gap, b.getY(), w, h);
    const auto bottomLeft  = juce::Rectangle<int> (b.getX(), b.getY() + h + gap, w, h);
    const auto bottomRight = juce::Rectangle<int> (b.getX() + w + gap, b.getY() + h + gap, w, h);

    // --- integrated loudness -------------------------------------------------
    juce::String integratedText = "--";
    juce::Colour integratedColour = theme::textFaint;

    if (stats.integrated > -99.0f)
    {
        integratedText = fmt1 (stats.integrated);
        const float diff = std::abs (stats.integrated - target);
        integratedColour = diff <= 2.0f ? theme::good : (diff <= 6.0f ? theme::warn : theme::bad);
    }

    paintTile (g, topLeft, T ("综合响度", "INTEGRATED"), integratedText, "LUFS",
               T ("目标 ", "Target ") + fmt1 (target) + " LUFS", integratedColour);

    // --- true peak -----------------------------------------------------------
    juce::String tpText = "--";
    juce::Colour tpColour = theme::textFaint;

    if (stats.truePeakDb > -99.0f)
    {
        tpText = fmtDb (stats.truePeakDb);
        tpColour = stats.truePeakDb > ceiling ? theme::bad
                 : (stats.truePeakDb > ceiling - 0.6f ? theme::warn : theme::good);
    }

    paintTile (g, topRight, T ("真峰值", "TRUE PEAK"), tpText, "dBTP",
               T ("上限 ", "Ceiling ") + fmt1 (ceiling) + " dBTP", tpColour);

    // --- loudness range ------------------------------------------------------
    const bool lraValid = stats.lra > 0.05f;
    juce::Colour lraColour = theme::textFaint;

    if (lraValid)
        lraColour = stats.lra < 3.0f ? theme::warn
                  : (stats.lra > 16.0f ? theme::info : theme::good);

    paintTile (g, bottomLeft, T ("响度范围 / LRA", "LOUDNESS RANGE"), lraValid ? fmt1 (stats.lra) : "--", "LU",
               T ("参考 5 – 12 LU", "Typical 5 – 12 LU"), lraColour);

    // --- peak to loudness ratio ---------------------------------------------
    const bool plrValid = stats.integrated > -70.0f && stats.truePeakDb > -70.0f;
    const float plr = stats.getPlr();
    juce::Colour plrColour = theme::textFaint;

    if (plrValid)
        plrColour = plr < 8.0f ? theme::warn
                  : (plr < 10.0f ? theme::info : theme::good);

    paintTile (g, bottomRight, T ("峰响比 / PLR", "PEAK / LOUDNESS"), plrValid ? fmt1 (plr) : "--", "dB",
               T ("参考 10 – 16 dB", "Typical 10 – 16 dB"), plrColour);
}

//==============================================================================
void LevelBars::setValues (const StatsSnapshot& s, float targetLufs)
{
    stats = s;
    target = targetLufs;
    repaint();
}

void LevelBars::paintBar (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                          float valueDb, juce::Colour colour)
{
    auto row = area;

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.0f));
    g.drawText (label, row.removeFromLeft (86), juce::Justification::centredLeft);

    auto valueArea = row.removeFromRight (56);

    auto track = row.reduced (0, 7).toFloat();

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 4.0f);

    const float norm = juce::jlimit (0.0f, 1.0f, (valueDb + 36.0f) / 36.0f);

    if (valueDb > -99.0f)
    {
        auto fill = track.withWidth (juce::jmax (3.0f, track.getWidth() * norm));
        g.setColour (colour);
        g.fillRoundedRectangle (fill, 4.0f);
    }

    const float tx = track.getX() + track.getWidth()
                     * juce::jlimit (0.0f, 1.0f, (target + 36.0f) / 36.0f);
    g.setColour (theme::text.withAlpha (0.45f));
    g.fillRect (tx - 0.5f, track.getY() - 2.5f, 1.2f, track.getHeight() + 5.0f);

    g.setColour (theme::text);
    g.setFont (monoFont (11.5f, true));
    g.drawText (valueDb <= -99.0f ? "--" : fmt1 (valueDb), valueArea, juce::Justification::centredRight);
}

void LevelBars::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds();
    const int rowH = b.getHeight() / 2;

    paintBar (g, b.withHeight (rowH), T ("M 瞬时", "M momentary"), stats.momentary,
              stats.momentary > 0.0f ? theme::bad : theme::accent);

    paintBar (g, b.withTrimmedTop (rowH), T ("S 短时", "S short-term"), stats.shortTerm,
              stats.shortTerm > 0.0f ? theme::bad : theme::cyan);
}

//==============================================================================
void StereoMeters::setValues (const StatsSnapshot& s)
{
    stats = s;
    repaint();
}

void StereoMeters::paintMeter (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                               const juce::String& readout, float normalised, juce::Colour fill,
                               float marker)
{
    auto row = area;

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.0f));
    g.drawText (label, row.removeFromLeft (86), juce::Justification::centredLeft);

    auto readoutArea = row.removeFromRight (70);
    auto track = row.reduced (2, 8).toFloat();

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 4.0f);

    const float norm = juce::jlimit (0.0f, 1.0f, normalised);
    g.setColour (fill);
    g.fillRoundedRectangle (track.withWidth (juce::jmax (2.0f, track.getWidth() * norm)), 4.0f);

    if (marker >= 0.0f)
    {
        const float mx = track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, marker);
        g.setColour (theme::text.withAlpha (0.32f));
        g.fillRect (mx - 0.5f, track.getY() - 2.0f, 1.0f, track.getHeight() + 4.0f);
    }

    g.setColour (theme::text);
    g.setFont (monoFont (11.5f, true));
    g.drawText (readout, readoutArea, juce::Justification::centredRight);
}

void StereoMeters::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    auto chipRow = b.removeFromBottom (28);
    b.removeFromBottom (10);

    const int meterH = juce::jlimit (26, 44, b.getHeight() / 3);
    b = b.withSizeKeepingCentre (b.getWidth(), meterH * 3);

    const float corr = stats.stereo ? stats.correlation : 1.0f;
    const auto corrColour = ! stats.stereo ? theme::textFaint
                          : (corr < 0.0f ? theme::bad
                          : (corr < 0.15f ? theme::warn : theme::good));

    paintMeter (g, b.removeFromTop (meterH), T ("声道相关性", "Correlation"),
                stats.stereo ? ((corr >= 0.0f ? "+" : "") + juce::String ((double) corr, 2)) : "--",
                (corr + 1.0f) * 0.5f, corrColour, 0.5f);

    const float width = stats.stereo ? stats.widthPct : 0.0f;
    const auto widthColour = ! stats.stereo ? theme::textFaint
                           : (width < 15.0f ? theme::info
                           : (width > 150.0f ? theme::warn : theme::good));

    paintMeter (g, b.removeFromTop (meterH), T ("立体声宽度", "Stereo width"),
                stats.stereo ? fmt1 (width) + "%" : "--",
                width / 200.0f, widthColour, 0.075f);

    const auto lowColour = ! stats.stereo ? theme::textFaint
                         : (stats.lowSidePct > 35.0f ? theme::warn : theme::good);

    paintMeter (g, b.removeFromTop (meterH), T ("低频单声道", "Low-end mono"),
                stats.stereo ? fmt1 (stats.lowSidePct) + "%" : "--",
                stats.lowSidePct / 100.0f, lowColour, 0.35f);

    const auto drawChip = [&g] (juce::Rectangle<int> r, const juce::String& text, juce::Colour colour)
    {
        g.setColour (colour.withAlpha (0.10f));
        g.fillRoundedRectangle (r.toFloat(), 7.0f);
        g.setColour (colour.withAlpha (0.24f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 7.0f, 1.0f);
        g.setColour (colour);
        g.setFont (uiFont (10.5f));
        g.drawText (text, r, juce::Justification::centred);
    };

    const int chipW = (chipRow.getWidth() - 18) / 4;

    drawChip (chipRow.removeFromLeft (chipW), T ("直流 ", "DC ") + fmtDb (stats.dcOffsetDb) + " dB",
              stats.dcOffsetDb > -46.0f ? theme::warn : theme::good);

    chipRow.removeFromLeft (6);
    drawChip (chipRow.removeFromLeft (chipW), T ("削波 ", "Clip ") + juce::String (stats.clipEvents),
              stats.clipEvents > 0 ? theme::bad : theme::good);

    chipRow.removeFromLeft (6);
    drawChip (chipRow.removeFromLeft (chipW), T ("瞬态 ", "Clicks ") + juce::String (stats.clickEvents),
              stats.clickEvents > 0 ? theme::warn : theme::good);

    chipRow.removeFromLeft (6);
    drawChip (chipRow, T ("过载 ", "Overs ") + juce::String (stats.oversCount),
              stats.oversCount > 0 ? theme::warn : theme::good);
}
} // namespace ml


