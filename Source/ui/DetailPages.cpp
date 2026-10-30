#include "Modules.h"
#include "../Delivery.h"

namespace ml
{
static juce::String pageTitle (PageId p)
{
    switch (p)
    {
        case PageId::loudness:    return T ("响度与动态", "Loudness & dynamics");
        case PageId::truePeak:    return T ("真峰值", "True peak");
        case PageId::spectrum:    return T ("频谱平衡", "Spectral balance");
        case PageId::stereo:      return T ("立体声 / 相位", "Stereo / phase");
        case PageId::delivery:    return T ("平台交付", "Delivery check");
        case PageId::diagnostics: return T ("混音诊断", "Mix diagnostics");
        case PageId::timeline:    return T ("时间线", "Timeline");
    }

    return {};
}

void DetailArea::paintPageHeader (juce::Graphics& g, const juce::String& meta)
{
    auto header = contentArea().removeFromTop (30);

    g.setColour (theme::text);
    g.setFont (uiFont (tokens::section, true));
    g.drawText (pageTitle (page), header.removeFromLeft (juce::jmin (260, header.getWidth() / 2)),
                juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (monoFont (tokens::unit));
    g.drawText (meta, header, juce::Justification::centredRight);
}

void DetailArea::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (theme::card);
    g.fillRoundedRectangle (b, tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (b, tokens::radiusCard, 1.0f);

    juce::String meta;

    switch (page)
    {
        case PageId::loudness:
            meta = juce::String ("M 400 ms") + ml::sep() + "S 3 s" + ml::sep() + "I " + T ("门限", "gated")
                 + ml::sep() + T ("目标 ", "target ") + num (state.target) + " LUFS";

            if (state.snapshotSlot >= 0)
                meta += ml::sep() + (state.snapshotSlot == 0 ? "A" : "B") + T (" 快照", " snapshot");
            break;

        case PageId::truePeak:
            meta = juce::String ("4") + ml::times() + " " + T ("过采样", "oversampling") + ml::sep()
                 + "OVERS " + juce::String (state.stats.oversCount);
            break;

        case PageId::spectrum:
            meta = (state.hasReference ? T ("参考: ", "Ref: ") + state.referenceStatus
                                       : T ("无参考轨", "no reference"))
                 + ml::sep() + T ("谐振 ", "res ") + juce::String ((int) state.resonances.size())
                 + ml::sep() + "8192 FFT";
            break;

        case PageId::stereo:
            meta = (state.stats.stereo ? T ("立体声", "stereo") : T ("单声道", "mono"))
                 + ml::sep() + T ("三段分频", "three bands") + ml::sep() + "L/R";
            break;

        case PageId::delivery:
            meta = T ("静态目标表", "static targets") + ml::sep() + T ("不联网", "offline");
            break;

        case PageId::diagnostics:
            meta = juce::String ((int) state.markers.size()) + T (" 标记", " markers");
            break;

        case PageId::timeline:
            meta = T ("双击时间条添加标记", "double-click to add a marker");
            break;
    }

    paintPageHeader (g, meta);

    if (emptyMode)
    {
        paintEmptyState (g, contentArea().withTrimmedTop (30));
        return;
    }

    switch (page)
    {
        case PageId::loudness:    paintLoudness (g);       break;
        case PageId::truePeak:    paintTruePeak (g);       break;
        case PageId::spectrum:    paintSpectrumExtras (g); break;
        case PageId::stereo:      paintStereoExtras (g);   break;
        case PageId::delivery:    paintDelivery (g);       break;
        case PageId::diagnostics: paintDiagnostics (g);    break;
        case PageId::timeline:    paintTimeline (g);       break;
    }
}

//==============================================================================
void DetailArea::paintLoudness (juce::Graphics& g)
{
    // Snapshot comparison is folded into the page header; nothing else to draw
    // here because the cards, meters and graph are real components.
    juce::ignoreUnused (g);
}
} // namespace ml
