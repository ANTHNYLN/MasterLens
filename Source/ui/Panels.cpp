#include "Widgets.h"
#include "../Delivery.h"

namespace ml
{
//==============================================================================
void paintEmptyState (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto centre = area.withSizeKeepingCentre (juce::jmin (520, area.getWidth() - 32), 168);

    auto icon = juce::Rectangle<float> (0.0f, 0.0f, 56.0f, 56.0f)
                    .withCentre ({ centre.toFloat().getCentreX(), centre.toFloat().getY() + 18.0f });

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (icon, 14.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (icon.reduced (0.5f), 14.0f, 1.0f);

    const float heights[] { 0.32f, 0.66f, 0.92f, 0.52f };
    float x = icon.getX() + 12.0f;

    for (float h : heights)
    {
        const float bh = 26.0f * h + 4.0f;
        g.setColour (theme::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (x, icon.getBottom() - 15.0f - bh, 5.0f, bh, 2.5f);
        x += 9.0f;
    }

    g.setColour (theme::text);
    g.setFont (uiFont (tokens::section, true));
    g.drawText (T ("播放音频后开始测量", "Play audio to start measuring"),
                centre.withTrimmedTop (82).withHeight (22), juce::Justification::centred);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("若长时间无响应，检查 Stereo Out 是否被旁路或静音",
                   "If nothing happens, check that Stereo Out is not bypassed or muted"),
                centre.withTrimmedTop (106).withHeight (20), juce::Justification::centred);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));
    g.drawText (T ("仅测量，不修改音频 · 零延迟", "Measurement only · zero latency"),
                centre.withTrimmedTop (128).withHeight (18), juce::Justification::centred);
}

//==============================================================================
void MetricGrid::setValues (const StatsSnapshot& s, float targetLufs, float ceilingDb, bool signal)
{
    stats = s;
    target = targetLufs;
    ceiling = ceilingDb;
    hasSignal = signal;
    repaint();
}

void MetricGrid::paintCard (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                            const juce::String& value, const juce::String& unit,
                            const juce::String& hint, const juce::String& pill, juce::Colour colour)
{
    const auto r = area.toFloat().reduced (0.5f);

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (r, tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, tokens::radiusCard, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::label));
    g.drawText (title, area.getX() + 12, area.getY() + 8, area.getWidth() - 24, 16,
                juce::Justification::centredLeft);

    if (pill.isNotEmpty())
    {
        const auto pillFont = uiFont (tokens::unit);
        const int pw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (pillFont, pill)) + 14;
        auto pr = juce::Rectangle<int> (area.getRight() - 12 - pw, area.getY() + 8, pw, 18);

        g.setColour (colour.withAlpha (0.14f));
        g.fillRoundedRectangle (pr.toFloat(), 9.0f);
        g.setColour (colour.withAlpha (0.30f));
        g.drawRoundedRectangle (pr.toFloat().reduced (0.5f), 9.0f, 1.0f);
        g.setColour (colour);
        g.setFont (pillFont);
        g.drawText (pill, pr, juce::Justification::centred);
    }

    const auto valueFont = monoFont (tokens::value, true);
    const int valueWidth = juce::roundToInt (
        juce::GlyphArrangement::getStringWidth (valueFont, value));

    auto valueRow = juce::Rectangle<int> (area.getX() + 12, area.getY() + 30,
                                          area.getWidth() - 24, 38);

    g.setColour (colour);
    g.setFont (valueFont);
    g.drawText (value, valueRow.withWidth (valueWidth + 6), juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText (unit, valueRow.withTrimmedLeft (valueWidth + 10).withTrimmedTop (12),
                juce::Justification::centredLeft);

    g.setFont (uiFont (tokens::aux));
    g.setColour (theme::textFaint);
    g.drawText (hint, area.getX() + 12, area.getY() + 80, area.getWidth() - 24, 18,
                juce::Justification::centredLeft);
}

void MetricGrid::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds();
    const int gap = tokens::gap;
    const bool fourAcross = b.getWidth() >= 560;

    const int cols = fourAcross ? 4 : 2;
    const int rows = fourAcross ? 1 : 2;
    const int w = (b.getWidth() - gap * (cols - 1)) / cols;
    const int h = (b.getHeight() - gap * (rows - 1)) / rows;

    const auto cell = [&] (int c, int r)
    {
        return juce::Rectangle<int> (b.getX() + c * (w + gap), b.getY() + r * (h + gap), w, h);
    };

    const auto noReading = noValue();
    const bool live = hasSignal;

    // --- integrated loudness --------------------------------------------------
    {
        const bool valid = live && stats.integrated > -70.0f;
        const float diff = stats.integrated - target;
        const auto colour = ! valid ? theme::textFaint
                          : (std::abs (diff) <= 1.0f ? theme::good
                          : (std::abs (diff) <= 2.0f ? theme::info : theme::warn));

        paintCard (g, cell (0, 0), T ("综合响度", "INTEGRATED"),
                   valid ? num (stats.integrated) : noReading, "LUFS",
                   valid ? T ("目标 ", "Target ") + num (target) + " LUFS"
                         : T ("等待信号", "Waiting for signal"),
                   valid ? sgn (diff) + " LU" : juce::String(), colour);
    }

    // --- true peak ------------------------------------------------------------
    {
        const bool valid = live && stats.truePeakDb > -70.0f;
        const float headroom = ceiling - stats.truePeakDb;
        const auto colour = ! valid ? theme::textFaint
                          : (headroom < 0.0f ? theme::bad
                          : (headroom < 1.0f ? theme::warn : theme::good));

        paintCard (g, cell (1, 0), T ("真峰值 · 4×", "TRUE PEAK · 4×"),
                   valid ? num (stats.truePeakDb) : noReading, "dBTP",
                   valid ? T ("上限 ", "Ceiling ") + num (ceiling) + ml::sep()
                               + "OVERS " + juce::String (stats.oversCount)
                         : T ("等待信号", "Waiting for signal"),
                   valid ? T ("余量 ", "Headroom ") + num (headroom) : juce::String(), colour);
    }

    // --- loudness range -------------------------------------------------------
    {
        const bool valid = live && stats.lra > 0.05f;
        const auto colour = ! valid ? theme::textFaint
                          : (stats.lra < 3.0f ? theme::warn
                          : (stats.lra > 16.0f ? theme::info : theme::good));

        const juce::String rangeHint = T ("参考 ", "Typical ") + fmtRange (5.0f, 12.0f) + " LU";

        paintCard (g, cell (2, 0), T ("响度范围 / LRA", "LOUDNESS RANGE"),
                   valid ? num (stats.lra) : noReading, "LU",
                   valid ? rangeHint : T ("等待信号", "Waiting for signal"),
                   juce::String(), colour);
    }

    // --- peak to loudness ratio ----------------------------------------------
    {
        const bool valid = live && stats.integrated > -70.0f && stats.truePeakDb > -70.0f;
        const float plr = stats.getPlr();
        const auto colour = ! valid ? theme::textFaint
                          : (plr < 8.0f ? theme::warn
                          : (plr < 10.0f ? theme::info : theme::good));

        const juce::String rangeHint = T ("参考 ", "Typical ") + fmtRange (10.0f, 16.0f) + " dB";

        paintCard (g, cell (3, 0), T ("峰响比 / PLR", "PEAK / LOUDNESS"),
                   valid ? num (plr) : noReading, "dB",
                   valid ? rangeHint : T ("等待信号", "Waiting for signal"),
                   juce::String(), colour);
    }
}

} // namespace ml
