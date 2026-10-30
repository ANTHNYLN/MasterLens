#include "Modules.h"
#include "../Delivery.h"

namespace ml
{
//==============================================================================
DetailArea::DetailArea()
{
    addChildComponent (tiles);
    addChildComponent (levelBars);
    addChildComponent (history);
    addChildComponent (spectrum);
    addChildComponent (bands);
    addChildComponent (meters);
    addChildComponent (issues);
    addChildComponent (refImport);
    addChildComponent (refClear);
    addChildComponent (reportButton);
    addChildComponent (markButton);

    refImport.onClick = [this] { if (onImportReference != nullptr) onImportReference(); };
    refClear.onClick = [this] { if (onClearReference != nullptr) onClearReference(); };
    reportButton.onClick = [this] { if (onExportReport != nullptr) onExportReport(); };
    markButton.onClick = [this] { if (onAddMarkerNow != nullptr) onAddMarkerNow(); };

    refreshTexts();
}

void DetailArea::refreshTexts()
{
    refImport.setButtonText (T ("导入参考轨", "Import reference"));
    refClear.setButtonText (T ("清除", "Clear"));
    reportButton.setButtonText (T ("导出报告", "Export report"));
    markButton.setButtonText (T ("+ 打标记", "+ Marker"));
}

void DetailArea::invalidateDisplays()
{
    spectrum.invalidateBackground();
    bands.invalidateBackground();
    history.invalidateBackground();
    repaint();
}

void DetailArea::hideAllChildren()
{
    tiles.setVisible (false);
    levelBars.setVisible (false);
    history.setVisible (false);
    spectrum.setVisible (false);
    bands.setVisible (false);
    meters.setVisible (false);
    issues.setVisible (false);
    refImport.setVisible (false);
    refClear.setVisible (false);
    reportButton.setVisible (false);
    markButton.setVisible (false);
}

juce::Rectangle<int> DetailArea::contentArea() const
{
    return getLocalBounds().reduced (16, 12);
}

void DetailArea::setPage (PageId p)
{
    if (page == p)
        return;

    page = p;
    resized();
    repaint();
}

void DetailArea::setHistory (const std::vector<float>& curve, float targetLufs, float lra)
{
    if (page == PageId::loudness)
    {
        history.setData (curve, targetLufs);
        history.setBand (lra);
    }
}

void DetailArea::expandFirstIssueForSnapshot()
{
    page = PageId::diagnostics;
    resized();
    issues.expandFirstForSnapshot();
    repaint();
}

void DetailArea::setIssues (std::vector<Issue> items)
{
    issues.setIssues (std::move (items));
    repaint();
}

void DetailArea::setReferenceInfo (bool hasRef, const juce::String& status)
{
    refReady = hasRef;
    refStatus = status;
    repaint();
}

void DetailArea::setState (const UiState& s)
{
    state = s;
    ++tick;

    const bool empty = ! s.stats.hasSignal;

    if (empty != emptyMode)
    {
        emptyMode = empty;
        resized();
        repaint();
    }

    if (emptyMode)
        return;

    switch (page)
    {
        case PageId::loudness:
            if ((tick % 2) == 0)
            {
                tiles.setValues (s.stats, s.target, s.ceiling, s.stats.hasSignal);
                levelBars.setValues (s.stats, s.target);
            }
            break;

        case PageId::spectrum:
            if (s.analyzer != nullptr)
                spectrum.setAnalyzer (s.analyzer);

            spectrum.repaint();

            if ((tick % 2) == 0)
                bands.setDeviations (s.bandDevs);
            break;

        case PageId::stereo:
            if ((tick % 2) == 0)
                meters.setValues (s.stats);
            break;

        default:
            if ((tick % 2) == 0)
                repaint();
            break;
    }
}

void DetailArea::resized()
{
    hideAllChildren();

    auto area = contentArea();
    area.removeFromTop (30);   // page header

    if (emptyMode)
        return;

    switch (page)
    {
        case PageId::loudness:
        {
            const int cardsH = area.getWidth() >= 560 ? tokens::cardHeight
                                                      : tokens::cardHeight * 2 + tokens::gap;

            tiles.setBounds (area.removeFromTop (juce::jmin (cardsH, area.getHeight() / 2)));
            area.removeFromTop (tokens::gap);

            const int metersH = juce::jlimit (88, 96, area.getHeight() / 3);
            levelBars.setBounds (area.removeFromTop (metersH));
            area.removeFromTop (tokens::gap);

            history.setBounds (area);
            tiles.setVisible (true);
            levelBars.setVisible (true);
            history.setVisible (true);
            break;
        }

        case PageId::spectrum:
        {
            const int footerH = juce::jlimit (86, 118, area.getHeight() / 4);
            const int buttonsH = 34;

            auto buttons = area.removeFromBottom (buttonsH);
            area.removeFromBottom (6);
            auto footer = area.removeFromBottom (footerH);
            area.removeFromBottom (tokens::gap);

            auto chipRow = footer.removeFromBottom (28);

            spectrum.setBounds (area);
            bands.setBounds (footer);
            juce::ignoreUnused (chipRow);

            auto row = buttons.withTrimmedRight (4);
            refClear.setBounds (row.removeFromRight (72));
            row.removeFromRight (8);
            refImport.setBounds (row.removeFromRight (118));

            spectrum.setVisible (true);
            bands.setVisible (true);
            refImport.setVisible (true);
            refClear.setVisible (true);
            break;
        }

        case PageId::stereo:
            meters.setBounds (area);
            meters.setVisible (true);
            break;

        case PageId::diagnostics:
        {
            auto summary = area.removeFromTop (34);
            juce::ignoreUnused (summary);

            issues.setBounds (area.withWidth (juce::roundToInt ((float) area.getWidth() * 0.56f)));
            issues.setVisible (true);
            break;
        }

        case PageId::timeline:
        {
            auto buttons = area.removeFromBottom (34);
            markButton.setBounds (buttons.removeFromRight (118).withSizeKeepingCentre (118, 32));
            markButton.setVisible (true);
            break;
        }

        default:
            break;
    }
}
} // namespace ml
