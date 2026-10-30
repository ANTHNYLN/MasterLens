#include "Modules.h"
#include "../Delivery.h"

namespace ml
{
void DetailArea::paintSpectrumExtras (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (30);

    const int footerH = juce::jlimit (86, 118, area.getHeight() / 4);

    area.removeFromBottom (34);
    area.removeFromBottom (6);
    auto footer = area.removeFromBottom (footerH);
    auto chipRow = footer.removeFromBottom (28);

    const char* names[4] { "20-250 Hz", "250 Hz-2k", "2k-8k", "8k-20k" };
    const int chipW = (chipRow.getWidth() - tokens::gap * 3) / 4;

    for (int i = 0; i < 4; ++i)
    {
        auto cell = chipRow.removeFromLeft (chipW);

        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (cell.toFloat(), tokens::radiusControl);
        g.setColour (theme::border);
        g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), tokens::radiusControl, 1.0f);

        auto inner = cell.reduced (10, 0);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::unit));
        g.drawText (names[i], inner.removeFromLeft (inner.getWidth() - 52),
                    juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (monoFont (12.0f, true));
        g.drawText (num (state.ratios[(size_t) i]) + "%", inner, juce::Justification::centredRight);

        if (i < 3)
            chipRow.removeFromLeft (tokens::gap);
    }
}

void DetailArea::paintStereoExtras (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

//==============================================================================
void DetailArea::paintDelivery (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (30);

    int okCount = 0, warnCount = 0, overCount = 0;

    for (int i = 0; i < numPlatforms; ++i)
    {
        const auto r = evaluate (state.stats, platform (i));

        if (r.state == DeliveryState::over)         ++overCount;
        else if (r.state == DeliveryState::warning) ++warnCount;
        else                                        ++okCount;
    }

    auto summary = area.removeFromTop (30);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("可行 ", "OK ") + juce::String (okCount) + ml::sep()
                + T ("临界 ", "Warning ") + juce::String (warnCount) + ml::sep()
                + T ("超限 ", "Over ") + juce::String (overCount),
                summary, juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText (T ("当前 ", "Current ") + num (state.stats.integrated) + " LUFS" + ml::sep()
                + num (state.stats.truePeakDb) + " dBTP",
                summary, juce::Justification::centredRight);

    area.removeFromTop (6);

    const int rowH = 32;
    auto header = area.removeFromTop (26);
    const int wPlatform = juce::roundToInt ((float) area.getWidth() * 0.26f);
    const int wTarget   = juce::roundToInt ((float) area.getWidth() * 0.12f);
    const int wCeiling  = juce::roundToInt ((float) area.getWidth() * 0.12f);
    const int wGain     = juce::roundToInt ((float) area.getWidth() * 0.14f);
    const int wPredicted = juce::roundToInt ((float) area.getWidth() * 0.18f);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::unit));

    auto h = header;
    g.drawText (T ("平台", "Platform"), h.removeFromLeft (wPlatform), juce::Justification::centredLeft);
    g.drawText (T ("目标 LUFS", "Target LUFS"), h.removeFromLeft (wTarget), juce::Justification::centredRight);
    g.drawText (T ("上限 dBTP", "Ceiling"), h.removeFromLeft (wCeiling), juce::Justification::centredRight);
    g.drawText (T ("需增益", "Gain"), h.removeFromLeft (wGain), juce::Justification::centredRight);
    g.drawText (T ("预测 TP", "Predicted TP"), h.removeFromLeft (wPredicted), juce::Justification::centredRight);
    g.drawText (T ("状态", "Status"), h, juce::Justification::centredRight);

    for (int i = 0; i < numPlatforms; ++i)
    {
        const auto& p = platform (i);
        const auto r = evaluate (state.stats, p);

        if (area.getHeight() < rowH)
            break;

        auto row = area.removeFromTop (rowH);
        const auto colour = r.state == DeliveryState::over ? theme::bad
                          : (r.state == DeliveryState::warning ? theme::warn : theme::good);

        if (r.state == DeliveryState::over)
        {
            g.setColour (theme::bad.withAlpha (0.06f));
            g.fillRect (row);
            g.setColour (theme::bad);
            g.fillRect (row.getX(), row.getY(), 2, row.getHeight());
        }
        else if (i % 2 == 0)
        {
            g.setColour (theme::borderSoft.withAlpha (0.35f));
            g.fillRect (row);
        }

        auto r0 = row;

        g.setColour (theme::text);
        g.setFont (uiFont (tokens::label));
        g.drawText (T (p.nameZh, p.nameEn), r0.removeFromLeft (wPlatform),
                    juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (monoFont (12.0f));
        g.drawText (num (p.targetLufs), r0.removeFromLeft (wTarget), juce::Justification::centredRight);
        g.drawText (num (p.tpCeiling), r0.removeFromLeft (wCeiling), juce::Justification::centredRight);

        g.setColour (theme::text);
        g.setFont (monoFont (12.0f, true));
        g.drawText (sgn (r.gainDb) + " dB", r0.removeFromLeft (wGain), juce::Justification::centredRight);

        g.setColour (colour);
        g.drawText (num (r.predictedTp) + " dBTP", r0.removeFromLeft (wPredicted),
                    juce::Justification::centredRight);

        g.setFont (uiFont (tokens::unit));
        g.drawText (r.state == DeliveryState::over ? T ("超限", "Over")
                  : r.state == DeliveryState::warning ? T ("临界", "Warning") : T ("可行", "OK"),
                    r0, juce::Justification::centredRight);
    }
}
} // namespace ml
