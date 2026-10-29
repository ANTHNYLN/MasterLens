#include "Modules.h"
#include "../Delivery.h"
#include "../Version.h"

namespace ml
{
//==============================================================================
ModuleRail::ModuleRail()
{
    setOpaque (true);
}

void ModuleRail::setModule (int index, juce::String title, juce::String value, juce::Colour colour)
{
    if (index < 0 || index >= numPages)
        return;

    items[(size_t) index].title = std::move (title);
    items[(size_t) index].value = std::move (value);
    items[(size_t) index].colour = colour;
    repaint();
}

void ModuleRail::setSelected (int index)
{
    selected = juce::jlimit (0, numPages - 1, index);
    repaint();
}

void ModuleRail::paint (juce::Graphics& g)
{
    g.fillAll (theme::card);

    const int rowH = rowHeight();

    for (int i = 0; i < numPages; ++i)
    {
        auto row = juce::Rectangle<int> (4, i * rowH + 2, getWidth() - 8, rowH - 5);
        const bool sel = (i == selected);

        if (sel)
        {
            g.setColour (theme::accentSoft);
            g.fillRoundedRectangle (row.toFloat(), 10.0f);
        }

        g.setColour (items[(size_t) i].colour);
        g.fillEllipse (12.0f, (float) row.getCentreY() - 4.0f, 8.0f, 8.0f);

        g.setColour (sel ? theme::text : theme::textDim);
        g.setFont (uiFont (12.0f, true));
        g.drawText (items[(size_t) i].title, 28, row.getY() + 7, row.getWidth() - 34, 18,
                    juce::Justification::centredLeft);

        g.setColour (theme::textFaint);
        g.setFont (monoFont (10.5f));
        g.drawText (items[(size_t) i].value, 28, row.getY() + 25, row.getWidth() - 34, 16,
                    juce::Justification::centredLeft);
    }
}

void ModuleRail::mouseDown (const juce::MouseEvent& e)
{
    const int index = juce::jlimit (0, numPages - 1, e.y / rowHeight());

    if (index != selected)
    {
        selected = index;
        repaint();

        if (onSelect != nullptr)
            onSelect (index);
    }
}

//==============================================================================
void TimelineStrip::setMarkers (const std::vector<Marker>& m, double totalSeconds)
{
    markers = m;
    total = juce::jmax (1.0, totalSeconds);
    repaint();
}

void TimelineStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (b, 8.0f);

    const float x0 = b.getX() + 12.0f;
    const float x1 = b.getRight() - 12.0f;
    const float y = b.getCentreY() + 3.0f;

    g.setColour (theme::border);
    g.drawHorizontalLine ((int) y, x0, x1);

    for (const auto& m : markers)
    {
        const float t = (float) juce::jlimit (0.0, 1.0, m.time / total);
        const float x = x0 + t * (x1 - x0);
        const auto c = m.severity >= 2 ? theme::bad : (m.severity == 1 ? theme::warn : theme::info);

        g.setColour (c);
        g.fillRoundedRectangle (x - 1.5f, y - 11.0f, 3.0f, 11.0f, 1.5f);
    }

    g.setColour (theme::textFaint);
    g.setFont (monoFont (9.0f));
    g.drawText ("00:00", (int) x0, (int) b.getBottom() - 14, 40, 12, juce::Justification::centredLeft);
    g.drawText (fmtTime (total), (int) x1 - 40, (int) b.getBottom() - 14, 40, 12,
                juce::Justification::centredRight);
}

//==============================================================================
static void computePageLayout (PageId page, juce::Rectangle<int> b,
                               juce::Rectangle<int>& main, juce::Rectangle<int>& secondary,
                               juce::Rectangle<int>& footer, juce::Rectangle<int>& buttons)
{
    main = b;
    secondary = {};
    footer = {};
    buttons = {};

    switch (page)
    {
        case PageId::loudness:
            footer = main.removeFromBottom (juce::roundToInt ((float) main.getHeight() * 0.46f));
            secondary = main.removeFromBottom (juce::jlimit (42, 62, main.getHeight() / 5));
            break;

        case PageId::spectrum:
        {
            // explicit sizing keeps the bands, ratio row and buttons from overlapping
            const int buttonsH = 34;
            const int footerH = juce::jmin (96, juce::jmax (74, juce::roundToInt ((float) main.getHeight() * 0.18f)));
            const int bandH = juce::jlimit (52, 78, juce::roundToInt ((float) main.getHeight() * 0.14f));

            buttons = main.removeFromBottom (buttonsH);
            footer = main.removeFromBottom (footerH);
            secondary = main.removeFromBottom (bandH);
            break;
        }

        case PageId::stereo:
            secondary = main.removeFromBottom (juce::roundToInt ((float) main.getHeight() * 0.40f));
            break;

        case PageId::diagnostics:
        case PageId::timeline:
            buttons = main.removeFromBottom (34);
            break;

        default:
            break;
    }
}

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

    refImport.onClick = [this] { if (onImportReference != nullptr) onImportReference(); };
    refClear.onClick = [this] { if (onClearReference != nullptr) onClearReference(); };
    reportButton.onClick = [this] { if (onExportReport != nullptr) onExportReport(); };

    refreshTexts();
}

void DetailArea::refreshTexts()
{
    refImport.setButtonText (T ("导入参考轨", "Import reference"));
    refClear.setButtonText (T ("清除", "Clear"));
    reportButton.setButtonText (T ("导出报告", "Export report"));
}

void DetailArea::setPage (PageId p)
{
    page = p;
    resized();
    repaint();
}

void DetailArea::setState (const UiState& s)
{
    state = s;
    ++tick;

    switch (page)
    {
        case PageId::loudness:
            if ((tick % 2) == 0)
            {
                tiles.setValues (s.stats, s.target, s.ceiling);
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
            repaint();
            break;
    }
}

void DetailArea::setHistory (const std::vector<float>& curve, float targetLufs)
{
    if (page == PageId::loudness)
        history.setData (curve, targetLufs);
}

void DetailArea::expandFirstIssueForSnapshot()
{
    page = PageId::diagnostics;
    resized();
    issues.expandFirstForSnapshot();
    repaint();
}

void DetailArea::setIssues (std::vector<Issue> issues)
{
    this->issues.setIssues (std::move (issues));
}

void DetailArea::setReferenceInfo (bool hasRef, const juce::String& status)
{
    refReady = hasRef;
    refStatus = status;
    repaint();
}

void DetailArea::resized()
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

    juce::Rectangle<int> main, secondary, footer, buttons;
    computePageLayout (page, getLocalBounds().reduced (4), main, secondary, footer, buttons);

    switch (page)
    {
        case PageId::loudness:
            tiles.setBounds (main);
            levelBars.setBounds (secondary.reduced (0, 4));
            history.setBounds (footer);
            tiles.setVisible (true);
            levelBars.setVisible (true);
            history.setVisible (true);
            break;

        case PageId::spectrum:
        {
            spectrum.setBounds (main);
            bands.setBounds (secondary);

            auto row = buttons.withTrimmedRight (4);
            refClear.setBounds (row.removeFromRight (64));
            row.removeFromRight (6);
            refImport.setBounds (row.removeFromRight (104));

            spectrum.setVisible (true);
            bands.setVisible (true);
            refImport.setVisible (true);
            refClear.setVisible (true);
            break;
        }

        case PageId::stereo:
            meters.setBounds (main);
            meters.setVisible (true);
            break;

        case PageId::diagnostics:
            issues.setBounds (main);
            issues.setVisible (true);
            reportButton.setBounds (buttons.removeFromRight (112).withSizeKeepingCentre (112, 28));
            reportButton.setVisible (true);
            break;

        case PageId::timeline:
            reportButton.setBounds (buttons.removeFromRight (112).withSizeKeepingCentre (112, 28));
            reportButton.setVisible (true);
            break;

        default:
            break;
    }
}

void DetailArea::paint (juce::Graphics& g)
{
    g.fillAll (theme::card);

    switch (page)
    {
        case PageId::truePeak:  paintTruePeak (g);       break;
        case PageId::spectrum:  paintSpectrumExtras (g); break;
        case PageId::stereo:    paintStereoExtras (g);   break;
        case PageId::delivery:  paintDelivery (g);       break;
        case PageId::timeline:  paintTimeline (g);       break;
        default: break;
    }
}

void DetailArea::mouseDown (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
}

//==============================================================================
void DetailArea::paintTruePeak (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (10, 8);
    const auto& s = state.stats;

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (T ("真峰值 / TRUE PEAK", "TRUE PEAK"), b.removeFromTop (22), juce::Justification::centredLeft);

    auto big = b.removeFromTop (92);
    const auto colour = s.truePeakDb > state.ceiling ? theme::bad
                      : (s.truePeakDb > state.ceiling - 0.6f ? theme::warn : theme::good);

    auto valueArea = big.removeFromLeft (juce::jmin (300, big.getWidth() / 2));
    g.setColour (colour);
    g.setFont (monoFont (46.0f, true));
    g.drawText (s.truePeakDb <= -99.0f ? "--" : fmt1 (s.truePeakDb),
                valueArea.removeFromLeft (210), juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (12.0f));
    g.drawText ("dBTP", valueArea.withTrimmedTop (24), juce::Justification::centredLeft);

    auto info = big;

    const auto row = [&] (const juce::String& key, const juce::String& value)
    {
        auto r = info.removeFromTop (26);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (11.0f));
        g.drawText (key, r.removeFromLeft (150), juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (monoFont (12.0f, true));
        g.drawText (value, r, juce::Justification::centredLeft);
    };

    row (T ("上限", "Ceiling"), fmt1 (state.ceiling) + " dBTP");
    row (T ("最大值位置", "Maximum at"), s.truePeakMaxTime > 0.01f ? fmtTime (s.truePeakMaxTime) : juce::String ("--"));
    row (T ("跨采样过载", "Inter-sample overs"), juce::String (s.oversCount) + T (" 段", " windows"));

    g.setColour (theme::borderSoft);
    g.drawHorizontalLine (b.getY(), (float) b.getX(), (float) b.getRight());
    b.removeFromTop (8);

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (T ("超限事件（时间戳 / 超出量）", "Over-limit events (time / overshoot)"),
                b.removeFromTop (20), juce::Justification::centredLeft);

    if (state.tpEvents.empty())
    {
        g.setColour (theme::good);
        g.setFont (uiFont (12.0f));
        g.drawText (T ("未检出真峰值超限", "No true-peak overs detected"),
                    b.removeFromTop (24), juce::Justification::centredLeft);
        return;
    }

    int shown = 0;

    for (auto it = state.tpEvents.rbegin(); it != state.tpEvents.rend() && shown < 9; ++it, ++shown)
    {
        auto r = b.removeFromTop (22);

        g.setColour (theme::bad);
        g.setFont (monoFont (11.5f, true));
        g.drawText (fmtTime (it->time), r.removeFromLeft (72), juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (monoFont (11.5f));
        g.drawText (fmt1 (it->overshootDb) + " dB", r.removeFromLeft (90), juce::Justification::centredLeft);
    }
}

//==============================================================================
void DetailArea::paintSpectrumExtras (juce::Graphics& g)
{
    juce::Rectangle<int> main, secondary, footer, buttons;
    computePageLayout (page, getLocalBounds().reduced (4), main, secondary, footer, buttons);

    auto area = footer.reduced (2, 1);

    const char* names[4] { "20-250 Hz", "250 Hz-2k", "2k-8k", "8k-20k" };
    auto ratioRow = area.removeFromTop (40);
    const int cellW = juce::jmax (60, ratioRow.getWidth() / 4);

    for (int i = 0; i < 4; ++i)
    {
        auto cell = ratioRow.removeFromLeft (cellW).reduced (2, 0);

        g.setColour (theme::textFaint);
        g.setFont (uiFont (9.5f));
        g.drawText (names[i], cell.removeFromTop (13), juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (monoFont (12.5f, true));
        g.drawText (fmt1 (state.ratios[(size_t) i]) + "%", cell.removeFromTop (17),
                    juce::Justification::centredLeft);

        auto bar = cell.reduced (0, 3);

        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);

        g.setColour (theme::accent);
        g.fillRoundedRectangle (bar.toFloat().withWidth (juce::jmax (2.0f, (float) bar.getWidth()
                                     * juce::jlimit (0.0f, 1.0f, state.ratios[(size_t) i] / 100.0f))), 3.0f);
    }

    auto resRow = area.removeFromTop (18);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (10.0f));
    g.drawText (T ("谐振", "Resonances"), resRow.removeFromLeft (60), juce::Justification::centredLeft);

    if (state.resonances.empty())
    {
        g.setColour (theme::good);
        g.setFont (uiFont (10.5f));
        g.drawText (T ("未检出 >6 dB 的窄带峰值", "No narrow peaks above +6 dB"), resRow,
                    juce::Justification::centredLeft);
    }
    else
    {
        juce::String txt;

        for (size_t i = 0; i < state.resonances.size() && i < 4; ++i)
        {
            if (i > 0)
                txt << "   ";

            txt << juce::String (juce::roundToInt (state.resonances[i].freq)) << " Hz "
                << (state.resonances[i].excessDb >= 0 ? "+" : "") << juce::String (state.resonances[i].excessDb, 1);
        }

        g.setColour (theme::warn);
        g.setFont (monoFont (10.5f));
        g.drawText (txt, resRow, juce::Justification::centredLeft);
    }

    // reference status sits above the buttons
    auto refRow = area.removeFromTop (14);
    g.setColour (refReady ? theme::good : theme::textFaint);
    g.setFont (uiFont (10.0f));
    g.drawText (refReady ? T ("参考轨已载入（实时偏差对比）", "Reference loaded (live deviation)")
                         : T ("未载入参考轨", "No reference loaded"),
                refRow, juce::Justification::centredLeft);
}

//==============================================================================
void DetailArea::paintLoudness (juce::Graphics& g)
{
    juce::ignoreUnused (g);   // the tiles / bars / history widgets draw themselves
}

void DetailArea::paintStereoExtras (juce::Graphics& g)
{
    juce::Rectangle<int> main, secondary, footer, buttons;
    computePageLayout (page, getLocalBounds().reduced (4), main, secondary, footer, buttons);

    auto area = secondary.reduced (10, 2);
    const auto& s = state.stats;

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.0f));
    g.drawText (T ("分频段相关性 / 侧向能量", "Per-band correlation / side energy"),
                area.removeFromTop (18), juce::Justification::centredLeft);

    struct BandRow { const char* zh; const char* en; float corr; float side; };
    const BandRow rows[] {
        { "低频 <150 Hz",      "Low <150 Hz",      s.bandCorrLow,  s.bandSideLow },
        { "中频 150 Hz–2 kHz", "Mid 150 Hz–2 kHz", s.bandCorrMid,  s.bandSideMid },
        { "高频 >2 kHz",       "High >2 kHz",      s.bandCorrHigh, s.bandSideHigh }
    };

    for (const auto& b : rows)
    {
        auto row = area.removeFromTop (juce::jmax (22, area.getHeight() / 3));

        g.setColour (theme::textDim);
        g.setFont (uiFont (10.5f));
        g.drawText (T (b.zh, b.en), row.removeFromLeft (150), juce::Justification::centredLeft);

        auto bar = row.removeFromLeft (juce::jmax (80, row.getWidth() - 190)).reduced (0, 9);

        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (b.corr + 1.0f) * 0.5f);

        g.setColour (b.corr < 0.0f ? theme::bad : (b.corr < 0.15f ? theme::warn : theme::good));
        g.fillRoundedRectangle (bar.toFloat().withWidth (juce::jmax (2.0f, (float) bar.getWidth() * norm)), 3.0f);

        g.setColour (theme::text.withAlpha (0.3f));
        g.fillRect ((float) bar.getCentreX() - 0.5f, (float) bar.getY() - 2.0f, 1.0f,
                    (float) bar.getHeight() + 4.0f);

        g.setColour (theme::text);
        g.setFont (monoFont (11.0f, true));
        g.drawText ((b.corr >= 0.0f ? "+" : "") + juce::String (b.corr, 2)
                        + T ("  |  侧 ", "  |  side ") + fmt1 (b.side) + "%",
                    row, juce::Justification::centredRight);
    }
}

//==============================================================================
void DetailArea::paintDelivery (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (12, 8);

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (T ("平台交付预测（静态查表，不联网）", "Delivery prediction (static table, offline)"),
                area.removeFromTop (22), juce::Justification::centredLeft);

    auto head = area.removeFromTop (22);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (10.0f));
    g.drawText (T ("平台", "Platform"), head.removeFromLeft (170), juce::Justification::centredLeft);
    g.drawText (T ("目标", "Target"), head.removeFromLeft (96), juce::Justification::centredLeft);
    g.drawText (T ("预期增益", "Gain"), head.removeFromLeft (104), juce::Justification::centredLeft);
    g.drawText (T ("归一化后 TP", "Predicted TP"), head.removeFromLeft (136), juce::Justification::centredLeft);
    g.drawText (T ("状态", "Status"), head, juce::Justification::centredLeft);

    for (int i = 0; i < numPlatforms; ++i)
    {
        const auto& p = platform (i);
        const auto r = evaluate (state.stats, p);

        auto row = area.removeFromTop (30);
        const auto colour = r.state == DeliveryState::over ? theme::bad
                          : (r.state == DeliveryState::warning ? theme::warn : theme::good);

        g.setColour (theme::text);
        g.setFont (uiFont (11.5f));
        g.drawText (T (p.nameZh, p.nameEn), row.removeFromLeft (170), juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (monoFont (10.5f));
        g.drawText (fmt1 (p.targetLufs) + " LUFS", row.removeFromLeft (96), juce::Justification::centredLeft);

        g.setColour (theme::text);
        g.setFont (monoFont (11.0f, true));
        g.drawText ((r.gainDb >= 0.0f ? "+" : "") + juce::String (r.gainDb, 1) + " dB",
                    row.removeFromLeft (104), juce::Justification::centredLeft);

        g.setColour (colour);
        g.drawText (fmt1 (r.predictedTp) + " dBTP", row.removeFromLeft (136), juce::Justification::centredLeft);

        g.setFont (uiFont (10.0f, true));
        g.drawText (r.state == DeliveryState::over ? T ("超限", "Over")
                  : r.state == DeliveryState::warning ? T ("临界", "Warning") : T ("可行", "OK"),
                    row, juce::Justification::centredLeft);
    }
}

//==============================================================================
void DetailArea::paintTimeline (juce::Graphics& g)
{
    juce::Rectangle<int> main, secondary, footer, buttons;
    computePageLayout (page, getLocalBounds().reduced (4), main, secondary, footer, buttons);

    auto area = main.reduced (10, 6);

    g.setColour (theme::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (T ("问题时间线（统一标记）", "Unified problem timeline"),
                area.removeFromTop (20), juce::Justification::centredLeft);

    if (state.markers.empty())
    {
        g.setColour (theme::good);
        g.setFont (uiFont (12.0f));
        g.drawText (T ("暂无标记", "No markers yet"), area.removeFromTop (24),
                    juce::Justification::centredLeft);
        return;
    }

    int shown = 0;

    for (const auto& m : state.markers)
    {
        if (shown++ >= 12)
            break;

        auto row = area.removeFromTop (24);
        const auto colour = m.severity >= 2 ? theme::bad : (m.severity == 1 ? theme::warn : theme::info);

        g.setColour (colour);
        g.fillEllipse ((float) row.getX(), (float) row.getCentreY() - 3.0f, 6.0f, 6.0f);

        g.setColour (theme::text);
        g.setFont (monoFont (11.0f, true));
        g.drawText (fmtTime (m.time), row.withTrimmedLeft (14).removeFromLeft (58),
                    juce::Justification::centredLeft);

        g.setColour (theme::textDim);
        g.setFont (uiFont (11.0f));
        g.drawText (m.text, row.withTrimmedLeft (76), juce::Justification::centredLeft);
    }
}






} // namespace ml
