#include "Widgets.h"

namespace ml
{
//==============================================================================
void LevelMeters::setValues (const StatsSnapshot& s, float targetLufs)
{
    stats = s;
    target = targetLufs;
    repaint();
}

void LevelMeters::paintRow (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                            const juce::String& windowText, float valueDb, juce::Colour colour)
{
    auto row = area;

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (label, row.getX(), row.getY() + 2, 58, 16, juce::Justification::centredLeft);

    if (row.getHeight() >= 36)
    {
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (windowText, row.getX(), row.getY() + 18, 58, 14, juce::Justification::centredLeft);
    }

    auto readout = row.removeFromRight (64);
    const bool over = valueDb > 0.0f;

    g.setColour (over ? theme::bad : theme::text);
    g.setFont (monoFont (13.0f, true));
    g.drawText (valueDb <= -99.0f ? noValue() : num (valueDb), readout,
                juce::Justification::centredRight);

    auto trackArea = row.withTrimmedLeft (64).withTrimmedRight (12);
    auto track = juce::Rectangle<float> ((float) trackArea.getX(),
                                         (float) trackArea.getCentreY() - 3.0f,
                                         (float) trackArea.getWidth(), 6.0f);

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 3.0f);

    const auto normFor = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f); };

    if (valueDb > -99.0f)
    {
        const float w = juce::jmax (4.0f, track.getWidth() * normFor (valueDb));
        auto fill = track.withWidth (w);

        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.55f), fill.getX(), fill.getY(),
                                                 colour, fill.getRight(), fill.getY(), false));
        g.fillRoundedRectangle (fill, 3.0f);
    }

    // -14 LUFS style target line
    const float tx = track.getX() + track.getWidth() * normFor (target);
    g.setColour (theme::textDim.withAlpha (0.80f));
    g.fillRect (tx - 0.5f, track.getY() - 4.0f, 1.0f, track.getHeight() + 8.0f);

    // scale ticks: -36 / -24 / -12 / -6 / 0 (marks only, labels drawn once)
    const float tickDbs[] { -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };

    for (float db : tickDbs)
    {
        const float x = track.getX() + track.getWidth() * normFor (db);

        g.setColour (theme::border);
        g.fillRect (x - 0.5f, track.getBottom() + 2.0f, 1.0f, 4.0f);
    }
}

void LevelMeters::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    if (b.getHeight() < 40)
        return;

    auto scaleRow = b.removeFromBottom (14);
    const int rowH = b.getHeight() / 2;

    paintRow (g, b.removeFromTop (rowH), T ("M 瞬时", "M momentary"), "400 ms",
              stats.momentary, theme::accent);

    paintRow (g, b, T ("S 短时", "S short-term"), "3 s",
              stats.shortTerm, theme::cyan);

    // ---- shared scale labels -------------------------------------------------
    const auto trackArea = scaleRow.withTrimmedLeft (64).withTrimmedRight (76);
    const auto normFor = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f); };
    const float labelDbs[] { -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };

    g.setFont (monoFont (tokens::unit));

    for (float db : labelDbs)
    {
        const float x = (float) trackArea.getX() + (float) trackArea.getWidth() * normFor (db);
        const juce::String text = db == 0.0f ? juce::String ("0") : num (db, 0);
        const int tw = 30;

        auto lr = juce::Rectangle<int> (juce::roundToInt (x) - tw / 2, scaleRow.getY(), tw, 12);
        lr.setX (juce::jlimit (trackArea.getX(), trackArea.getRight() - tw, lr.getX()));

        g.setColour (theme::textFaint);
        g.drawText (text, lr, juce::Justification::centred);
    }
}
} // namespace ml
