#include "PluginEditor.h"
#include "Delivery.h"
#include "Version.h"

using ml::T;
using ml::fmt1;

namespace
{
const char* const presetNamesZh[]
{
    "流媒体通用  -14 LUFS", "Spotify  -14 LUFS", "Apple Music  -16 LUFS",
    "YouTube  -14 LUFS", "播客  -16 LUFS", "广播 EBU R128  -23 LUFS",
    "俱乐部 / 舞曲  -8 LUFS", "自定义"
};

const char* const presetNamesEn[]
{
    "Streaming  -14 LUFS", "Spotify  -14 LUFS", "Apple Music  -16 LUFS",
    "YouTube  -14 LUFS", "Podcast  -16 LUFS", "Broadcast EBU R128  -23 LUFS",
    "Club / dance  -8 LUFS", "Custom"
};

constexpr int numPresets = (int) (sizeof (presetNamesZh) / sizeof (presetNamesZh[0]));

const float presetTargets[]   { -14.0f, -14.0f, -16.0f, -14.0f, -16.0f, -23.0f, -8.0f, 0.0f };
const float presetCeilings[]  { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -0.8f, 0.0f };
} // namespace

//==============================================================================
MasterLensAudioProcessorEditor::MasterLensAudioProcessorEditor (MasterLensAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    addAndMakeVisible (rail);
    addAndMakeVisible (detail);
    addAndMakeVisible (timeline);

    rail.onSelect = [this] (int index)
    {
        detail.setPage ((ml::PageId) index);
        repaint();
    };

    detail.onImportReference = [this] { importReference(); };
    detail.onClearReference  = [this] { clearReference(); };
    detail.onExportReport    = [this] { exportReport(); };

    // ---- header -------------------------------------------------------------
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    const auto captionFont = ml::uiFont (10.0f);

    targetCaption.setFont (captionFont);
    targetCaption.setColour (juce::Label::textColourId, ml::theme::textDim);
    addAndMakeVisible (targetCaption);

    ceilingCaption.setFont (captionFont);
    ceilingCaption.setColour (juce::Label::textColourId, ml::theme::textDim);
    addAndMakeVisible (ceilingCaption);

    targetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    targetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    targetSlider.setColour (juce::Slider::textBoxTextColourId, ml::theme::text);
    addAndMakeVisible (targetSlider);

    ceilingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    ceilingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    ceilingSlider.setColour (juce::Slider::textBoxTextColourId, ml::theme::text);
    addAndMakeVisible (ceilingSlider);

    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "targetLufs", targetSlider);

    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "tpCeiling", ceilingSlider);

    targetSlider.onValueChange = [this] { syncPresetToParameters(); };
    ceilingSlider.onValueChange = [this] { syncPresetToParameters(); };

    resetButton.onClick = [this] { resetAnalysis(); };
    addAndMakeVisible (resetButton);

    freezeButton.setClickingTogglesState (true);
    freezeButton.onClick = [this]
    {
        frozen = freezeButton.getToggleState();
        updateTexts();
        repaint();
    };
    addAndMakeVisible (freezeButton);

    languageButton.onClick = [this]
    {
        const auto next = ml::Localisation::isEnglish() ? ml::Language::chinese : ml::Language::english;
        ml::Localisation::set (next);
        processor.apvts.state.setProperty ("language", next == ml::Language::english ? 1 : 0, nullptr);
        updateTexts();
        repaint();
    };
    addAndMakeVisible (languageButton);

    aboutButton.onClick = [this] { aboutPanel.setVisibleAndFocus (! aboutPanel.isVisible()); };
    addAndMakeVisible (aboutButton);

    aboutPanel.onClose = [this] { aboutPanel.setVisibleAndFocus (false); };
    addChildComponent (aboutPanel);
    aboutPanel.setVisible (false);

    {
        const int storedLanguage = (int) processor.apvts.state.getProperty ("language", 0);
        ml::Localisation::set (storedLanguage == 1 ? ml::Language::english : ml::Language::chinese);
    }

    // ---- window -------------------------------------------------------------
    const int storedW = (int) processor.apvts.state.getProperty ("editorWidth", 1280);
    const int storedH = (int) processor.apvts.state.getProperty ("editorHeight", 820);

    setResizable (true, true);
    setResizeLimits (1080, 700, 1900, 1200);
    setSize (juce::jlimit (1080, 1900, storedW), juce::jlimit (700, 1200, storedH));

    updateTexts();
    syncPresetToParameters();

    analyzerSampleRate = processor.getSampleRateValue();
    analyzer.prepare (analyzerSampleRate, 13);

    // restore reference tracks saved with the session
    {
        const auto saved = processor.apvts.state.getProperty ("referenceFiles").toString();

        if (saved.isNotEmpty())
        {
            juce::StringArray files;
            files.addTokens (saved, "|", {});
            reference.load (files, analyzerSampleRate, 13);
        }
    }

    startTimerHz (30);
}

MasterLensAudioProcessorEditor::~MasterLensAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::String MasterLensAudioProcessorEditor::moduleTitle (int index) const
{
    switch (index)
    {
        case 0: return T ("响度与动态", "Loudness & dynamics");
        case 1: return T ("真峰值", "True peak");
        case 2: return T ("频谱平衡", "Spectral balance");
        case 3: return T ("立体声 / 相位", "Stereo / phase");
        case 4: return T ("平台交付", "Delivery check");
        case 5: return T ("混音诊断", "Mix diagnostics");
        case 6: return T ("时间线", "Timeline");
        default: return {};
    }
}

void MasterLensAudioProcessorEditor::updateTexts()
{
    const bool en = ml::Localisation::isEnglish();

    resetButton.setButtonText (T ("重置", "Reset"));
    freezeButton.setButtonText (frozen ? T ("恢复", "Resume") : T ("冻结", "Freeze"));
    aboutButton.setButtonText (T ("关于", "About"));
    languageButton.setButtonText (en ? juce::String::fromUTF8 ("中文") : juce::String ("EN"));

    targetCaption.setText (T ("目标响度 LUFS", "Target LUFS"), juce::dontSendNotification);
    ceilingCaption.setText (T ("真峰值上限 dBTP", "Ceiling dBTP"), juce::dontSendNotification);

    const int selected = juce::jlimit (0, numPresets - 1, presetBox.getSelectedItemIndex());
    updatingPreset = true;
    presetBox.clear (juce::dontSendNotification);

    for (int i = 0; i < numPresets; ++i)
        presetBox.addItem (ml::U8 (en ? presetNamesEn[i] : presetNamesZh[i]), i + 1);

    presetBox.setSelectedItemIndex (selected, juce::dontSendNotification);
    updatingPreset = false;

    detail.refreshTexts();
    updateRail();
    repaint();
}

void MasterLensAudioProcessorEditor::updateRail()
{
    const auto& s = stats;

    {
        const float diff = std::abs (s.integrated - processor.getTargetLufs());
        const auto c = s.integrated < -99.0f ? ml::theme::textFaint
                     : (diff <= 2.0f ? ml::theme::good : (diff <= 6.0f ? ml::theme::warn : ml::theme::bad));
        rail.setModule (0, moduleTitle (0), fmt1 (s.integrated) + "  /  LRA " + fmt1 (s.lra), c);
    }

    {
        const auto c = s.truePeakDb < -99.0f ? ml::theme::textFaint
                     : (s.truePeakDb > processor.getCeilingDb() ? ml::theme::bad
                     : (s.truePeakDb > processor.getCeilingDb() - 0.6f ? ml::theme::warn : ml::theme::good));
        rail.setModule (1, moduleTitle (1), fmt1 (s.truePeakDb) + " dBTP", c);
    }

    {
        const int n = (int) analyzer.getResonances().size();
        rail.setModule (2, moduleTitle (2),
                        n > 0 ? (juce::String (n) + T (" 处谐振", " resonances")) : T ("平衡", "balanced"),
                        n > 0 ? ml::theme::warn : ml::theme::good);
    }

    {
        const auto c = s.correlation < 0.0f ? ml::theme::bad
                     : (s.correlation < 0.15f ? ml::theme::warn : ml::theme::good);
        rail.setModule (3, moduleTitle (3),
                        (s.correlation >= 0.0f ? "+" : "") + juce::String (s.correlation, 2)
                            + "  ·  " + fmt1 (s.widthPct) + "%", c);
    }

    {
        auto worst = ml::DeliveryState::ok;
        juce::String name;

        for (int i = 0; i < ml::numPlatforms; ++i)
        {
            const auto& p = ml::platform (i);
            const auto r = ml::evaluate (s, p);

            if ((int) r.state > (int) worst)
            {
                worst = r.state;
                name = T (p.nameZh, p.nameEn);
            }
        }

        const auto c = worst == ml::DeliveryState::over ? ml::theme::bad
                     : (worst == ml::DeliveryState::warning ? ml::theme::warn : ml::theme::good);

        rail.setModule (4, moduleTitle (4),
                        worst == ml::DeliveryState::over ? (name + T (" 超限", " over"))
                      : worst == ml::DeliveryState::warning ? T ("临界", "warning")
                                                           : T ("全部可行", "all OK"), c);
    }

    {
        const auto sev = advisor.getOverallSeverity();
        rail.setModule (5, moduleTitle (5), juce::String ((int) issueCache.size()) + T (" 项", " items"),
                        ml::severityColour (sev));
    }

    rail.setModule (6, moduleTitle (6), juce::String ((int) markers.size()) + T (" 标记", " markers"),
                    markers.empty() ? ml::theme::good : ml::theme::info);
}

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);

    juce::ColourGradient wash (juce::Colour (0xffe8f0ff), 0.0f, 0.0f,
                               ml::theme::background.withAlpha (0.0f), 0.0f, 170.0f, false);
    g.setGradientFill (wash);
    g.fillRect (0, 0, getWidth(), 170);

    auto header = getLocalBounds().removeFromTop (62).reduced (16, 9);
    auto logo = header.removeFromLeft (44).toFloat();

    g.setGradientFill (juce::ColourGradient (ml::theme::accent, logo.getX(), logo.getY(),
                                             ml::theme::violet, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 12.0f);

    const float barHeights[] { 0.30f, 0.62f, 0.90f, 0.48f };
    float barX = logo.getX() + 8.5f;

    for (float h : barHeights)
    {
        const float bh = logo.getHeight() * 0.44f * h + 4.0f;
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillRoundedRectangle (barX, logo.getBottom() - 9.0f - bh, 4.0f, bh, 2.0f);
        barX += 8.0f;
    }

    const int textX = (int) logo.getRight() + 14;

    g.setColour (ml::theme::text);
    g.setFont (ml::uiFont (20.0f, true));
    g.drawText ("MasterLens", textX, header.getY() - 3, 240, 26, juce::Justification::centredLeft);

    g.setColour (ml::theme::textFaint);
    g.setFont (ml::uiFont (10.0f));
    g.drawText ("v" + juce::String (ml::kVersion), textX + 156, header.getY() + 1, 90, 18,
                juce::Justification::centredLeft);

    g.setColour (ml::theme::textDim);
    g.setFont (ml::uiFont (10.5f));
    g.drawText (T ("母带交付检查 · 不修改音频", "Mastering QC · never alters audio"),
                textX, header.getY() + 23, 380, 17, juce::Justification::centredLeft);

    if (frozen)
    {
        const auto pill = juce::Rectangle<int> (textX + 372, header.getY() + 23, 58, 18);
        g.setColour (ml::theme::warn.withAlpha (0.16f));
        g.fillRoundedRectangle (pill.toFloat(), 9.0f);
        g.setColour (ml::theme::warn);
        g.setFont (ml::uiFont (9.5f));
        g.drawText (T ("已冻结", "Frozen"), pill, juce::Justification::centred);
    }

    g.setColour (ml::theme::border);
    g.drawHorizontalLine (105, 16.0f, (float) getWidth() - 16.0f);

    if (! pageTitleArea.isEmpty())
    {
        g.setColour (ml::theme::text);
        g.setFont (ml::uiFont (15.0f, true));
        g.drawText (moduleTitle ((int) detail.getPage()), pageTitleArea, juce::Justification::centredLeft);
    }
}

void MasterLensAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    auto row1 = b.removeFromTop (62).reduced (16, 9);
    auto row2 = b.removeFromTop (44).reduced (16, 5);

    languageButton.setBounds (row1.removeFromRight (46).withSizeKeepingCentre (46, 28));
    row1.removeFromRight (8);
    aboutButton.setBounds (row1.removeFromRight (58).withSizeKeepingCentre (58, 28));
    row1.removeFromRight (8);

    const int presetW = juce::jmin (206, juce::jmax (120, row1.getWidth()));
    presetBox.setBounds (row1.removeFromRight (presetW).withSizeKeepingCentre (presetW, 28));

    auto targetBlock = row2.removeFromLeft (juce::jlimit (180, 260, row2.getWidth() / 3));
    row2.removeFromLeft (16);
    auto ceilingBlock = row2.removeFromLeft (juce::jlimit (170, 240, row2.getWidth() / 2));

    freezeButton.setBounds (row2.removeFromRight (74).withSizeKeepingCentre (74, 28));
    row2.removeFromRight (8);
    resetButton.setBounds (row2.removeFromRight (68).withSizeKeepingCentre (68, 28));

    targetCaption.setBounds (targetBlock.removeFromTop (14));
    targetSlider.setBounds (targetBlock.removeFromTop (22));

    ceilingCaption.setBounds (ceilingBlock.removeFromTop (14));
    ceilingSlider.setBounds (ceilingBlock.removeFromTop (22));

    auto content = b.reduced (16, 0).withTrimmedBottom (14);

    auto railArea = content.removeFromLeft (juce::jlimit (168, 208, content.getWidth() / 6));
    rail.setBounds (railArea);
    content.removeFromLeft (12);

    pageTitleArea = content.removeFromTop (28);

    auto tl = content.removeFromBottom (38);
    content.removeFromBottom (6);

    timeline.setBounds (tl);
    detail.setBounds (content);

    aboutPanel.setBounds (getLocalBounds());

    processor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    processor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

//==============================================================================
void MasterLensAudioProcessorEditor::timerCallback()
{
    ++frameCounter;

    const double sr = processor.getSampleRateValue();

    if (std::abs (sr - analyzerSampleRate) > 0.5)
    {
        analyzerSampleRate = sr;
        analyzer.prepare (sr, 13);
    }

    analyzer.setSmoothing (processor.getSmoothing());
    analyzer.update (processor.spectrumRing);

    if (reference.isReady() && ! analyzer.hasReference())
        analyzer.setReferenceSpectrum (reference.getSpectrumDb());

    if (frozen)
        return;

    stats = processor.getStatsSnapshot();

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    {
        const auto& ring = processor.engine.getTpEvents();
        const int n = ring.size();

        tpEvents.clear();
        tpEvents.reserve ((size_t) n);

        for (int i = 0; i < n; ++i)
            tpEvents.push_back (ring.get (i));
    }

    bandRatios = analyzer.getBandRatios();

    const auto& devs = analyzer.getBandDeviations();
    const auto& resos = analyzer.getResonances();

    // ---- unified timeline markers ------------------------------------------
    const auto addMarker = [this] (double t, int sev, int cat, const juce::String& text)
    {
        if (t <= 0.05)
            return;

        for (const auto& m : markers)
            if (m.category == cat && std::abs (m.time - t) < 0.45)
                return;

        markers.push_back ({ t, sev, cat, text });

        if (markers.size() > 64)
            markers.erase (markers.begin());
    };

    for (const auto& e : tpEvents)
        addMarker (e.time, 2, 0, T ("真峰值 +", "TP +") + fmt1 (e.overshootDb) + " dBTP");

    if (stats.psrMin > -99.0f && stats.psrMinTime > 0.0)
        addMarker (stats.psrMinTime, 1, 1, T ("PSR 最低 ", "PSR minimum ") + fmt1 (stats.psrMin) + " dB");

    if (stats.lastSpikeTime > 0.0)
        addMarker (stats.lastSpikeTime, 1, 1, T ("响度瞬时尖峰", "Loudness spike"));

    if (stats.correlation < 0.0f && stats.elapsed > 1.0)
        addMarker (std::floor (stats.elapsed), 2, 2, T ("相关性为负", "Negative correlation"));

    if (stats.elapsed > 3.0)
        for (const auto& r : resos)
            addMarker (std::floor (stats.elapsed / 5.0) * 5.0, 1, 3,
                       T ("谐振 ", "Resonance ") + juce::String (juce::roundToInt (r.freq))
                           + " Hz +" + fmt1 (r.excessDb));

    ml::UiState us;
    us.stats = stats;
    us.bandDevs = devs;
    us.ratios = bandRatios;
    us.resonances = resos;
    us.tpEvents = tpEvents;
    us.markers = markers;
    us.target = target;
    us.ceiling = ceiling;
    us.hasReference = analyzer.hasReference();
    us.referenceStatus = reference.getStatus();
    us.analyzer = &analyzer;

    detail.setState (us);
    detail.setReferenceInfo (analyzer.hasReference(), reference.getStatus());
    timeline.setMarkers (markers, juce::jmax (30.0, stats.elapsed));

    if ((frameCounter % 2) == 0)
    {
        processor.copyShortTermCurve (curve, 1800);
        detail.setHistory (curve, target);

        advisor.update (stats, devs, target, ceiling);
        issueCache = advisor.getIssues();

        detail.setIssues (issueCache);
        updateRail();

        if (! pageTitleArea.isEmpty())
            repaint (pageTitleArea);
    }
}

//==============================================================================
void MasterLensAudioProcessorEditor::applyPreset (int index)
{
    if (updatingPreset || index < 0 || index >= 7)
        return;

    setParameter ("targetLufs", presetTargets[index]);
    setParameter ("tpCeiling", presetCeilings[index]);
}

void MasterLensAudioProcessorEditor::syncPresetToParameters()
{
    if (updatingPreset)
        return;

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    int match = 7;

    for (int i = 0; i < 7; ++i)
    {
        if (std::abs (target - presetTargets[i]) < 0.05f
            && std::abs (ceiling - presetCeilings[i]) < 0.05f)
        {
            match = i;
            break;
        }
    }

    updatingPreset = true;
    presetBox.setSelectedItemIndex (match, juce::dontSendNotification);
    updatingPreset = false;
}

void MasterLensAudioProcessorEditor::setParameter (const char* id, float value)
{
    if (auto* param = processor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void MasterLensAudioProcessorEditor::resetAnalysis()
{
    processor.requestReset();
    analyzer.reset();
    advisor.reset();

    curve.clear();
    stats = ml::StatsSnapshot {};
    markers.clear();
    tpEvents.clear();
    issueCache.clear();

    detail.setIssues ({});
    detail.setHistory ({}, processor.getTargetLufs());
    updateRail();
}

void MasterLensAudioProcessorEditor::importReference()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("选择 1–4 条参考轨（WAV / AIFF / FLAC / MP3）",
           "Choose 1-4 reference tracks (WAV / AIFF / FLAC / MP3)"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto results = fc.getResults();

                                  if (results.isEmpty())
                                      return;

                                  juce::StringArray files;

                                  for (int i = 0; i < juce::jmin (4, results.size()); ++i)
                                      files.add (results[i].getFullPathName());

                                  reference.load (files, analyzerSampleRate, 13);
                                  processor.apvts.state.setProperty ("referenceFiles", files.joinIntoString ("|"), nullptr);
                                  analyzer.clearReference();
                              });
}

void MasterLensAudioProcessorEditor::clearReference()
{
    reference.clear();
    analyzer.clearReference();
    processor.apvts.state.setProperty ("referenceFiles", juce::String(), nullptr);
    detail.setReferenceInfo (false, {});
}

//==============================================================================
void MasterLensAudioProcessorEditor::exportReport()
{
    const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                 .getChildFile ("MasterLens-Report.txt");

    fileChooser = std::make_unique<juce::FileChooser> (T ("导出检查报告", "Export QC report"),
                                                      defaultFile, "*.txt");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (file == juce::File {})
                                      return;

                                  file.replaceWithText (buildReport());
                                  file.withFileExtension (".json").replaceWithText (buildReportJson());
                              });
}

juce::String MasterLensAudioProcessorEditor::buildReport() const
{
    juce::String r;
    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();
    const auto& s = stats;

    r << "MasterLens " << ml::kVersion << T (" · 母带交付检查报告\n", " - mastering QC report\n");
    r << T ("生成时间：", "Generated: ")
      << juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") << "\n";
    r << T ("采样率：", "Sample rate: ") << juce::String (analyzerSampleRate, 0) << " Hz   "
      << T ("声道：", "Channels: ") << (s.stereo ? T ("立体声", "stereo") : T ("单声道", "mono")) << "\n\n";

    r << T ("【响度与动态】\n", "[Loudness & dynamics]\n");
    r << "  I            : " << ml::fmtDb (s.integrated) << " LUFS"
      << T ("（目标 ", " (target ") << fmt1 (target) << " LUFS)\n";
    r << "  S / M        : " << ml::fmtDb (s.shortTerm) << " / " << ml::fmtDb (s.momentary) << " LUFS\n";
    r << "  LRA / PLR    : " << fmt1 (s.lra) << " LU / " << fmt1 (s.getPlr()) << " dB\n";
    r << "  PSR (min)    : " << ml::fmtDb (s.psr) << " / " << ml::fmtDb (s.psrMin) << " dB"
      << " @ " << ml::fmtTime (s.psrMinTime) << "\n\n";

    r << T ("【真峰值】\n", "[True peak]\n");
    r << "  TP max       : " << ml::fmtDb (s.truePeakDb) << " dBTP"
      << T ("（上限 ", " (ceiling ") << fmt1 (ceiling) << " dBTP)\n";
    r << T ("  最大值位置   : ", "  Maximum at   : ") << ml::fmtTime (s.truePeakMaxTime) << "\n";
    r << T ("  过载事件     : ", "  Over events  : ") << (int) tpEvents.size() << "\n";

    for (const auto& e : tpEvents)
        r << "    " << ml::fmtTime (e.time) << "  +" << fmt1 (e.overshootDb) << " dB\n";

    r << "\n" << T ("【频谱 / 立体声】\n", "[Spectrum / stereo]\n");
    r << "  20-250 / 250-2k / 2k-8k / 8k-20k : "
      << fmt1 (bandRatios[0]) << "% / " << fmt1 (bandRatios[1]) << "% / "
      << fmt1 (bandRatios[2]) << "% / " << fmt1 (bandRatios[3]) << "%\n";
    r << "  corr / width / low-side : " << juce::String (s.correlation, 2) << " / "
      << fmt1 (s.widthPct) << "% / " << fmt1 (s.lowSidePct) << "%\n";

    if (! analyzer.getResonances().empty())
    {
        r << T ("  谐振：", "  Resonances: ");

        for (const auto& res : analyzer.getResonances())
            r << juce::String (juce::roundToInt (res.freq)) << "Hz +" << fmt1 (res.excessDb) << "  ";

        r << "\n";
    }

    r << "\n" << T ("【平台交付预测】\n", "[Delivery prediction]\n");

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (s, p);

        r << "  " << T (p.nameZh, p.nameEn) << " : "
          << T ("增益 ", "gain ") << (res.gainDb >= 0 ? "+" : "") << juce::String (res.gainDb, 1)
          << " dB, " << T ("预测 TP ", "predicted TP ") << fmt1 (res.predictedTp) << " dBTP, "
          << (res.state == ml::DeliveryState::over ? T ("超限", "OVER")
            : res.state == ml::DeliveryState::warning ? T ("临界", "WARNING") : T ("可行", "OK")) << "\n";
    }

    r << "\n" << T ("【时间线标记】\n", "[Timeline markers]\n");

    if (markers.empty())
        r << T ("  无\n", "  none\n");

    for (const auto& m : markers)
        r << "  " << ml::fmtTime (m.time) << "  " << m.text << "\n";

    r << "\n" << T ("【诊断】\n", "[Diagnostics]\n");

    for (const auto& i : issueCache)
    {
        const juce::String tag = i.severity == ml::Severity::critical ? T ("需要处理", "Action")
                               : i.severity == ml::Severity::warn ? T ("建议检查", "Review")
                               : i.severity == ml::Severity::info ? T ("可优化", "Improve")
                                                                  : T ("良好", "Good");
        r << "  [" << tag << "] " << i.title << "\n    " << i.detail << "\n";
    }

    r << "\n" << T ("开发者 ", "Developer ") << ml::kDeveloper
      << T (" · 机构 ", " · Organisation ") << ml::kOrganisation << " · v" << ml::kVersion << "\n";

    return r;
}

juce::String MasterLensAudioProcessorEditor::buildReportJson() const
{
    auto* root = new juce::DynamicObject();

    auto* loud = new juce::DynamicObject();
    loud->setProperty ("integratedLufs", stats.integrated);
    loud->setProperty ("shortTermLufs", stats.shortTerm);
    loud->setProperty ("momentaryLufs", stats.momentary);
    loud->setProperty ("lra", stats.lra);
    loud->setProperty ("plr", stats.getPlr());
    loud->setProperty ("psr", stats.psr);
    loud->setProperty ("psrMin", stats.psrMin);
    loud->setProperty ("psrMinTime", stats.psrMinTime);
    root->setProperty ("loudness", loud);

    auto* peak = new juce::DynamicObject();
    peak->setProperty ("truePeakDbtp", stats.truePeakDb);
    peak->setProperty ("samplePeakDbfs", stats.samplePeakDb);
    peak->setProperty ("truePeakMaxTime", stats.truePeakMaxTime);
    peak->setProperty ("ceilingDbtp", processor.getCeilingDb());

    juce::Array<juce::var> events;

    for (const auto& e : tpEvents)
    {
        auto* ev = new juce::DynamicObject();
        ev->setProperty ("time", e.time);
        ev->setProperty ("overshootDb", e.overshootDb);
        events.add (juce::var (ev));
    }

    peak->setProperty ("events", events);
    root->setProperty ("truePeak", peak);

    auto* stereo = new juce::DynamicObject();
    stereo->setProperty ("correlation", stats.correlation);
    stereo->setProperty ("widthPct", stats.widthPct);
    stereo->setProperty ("lowSidePct", stats.lowSidePct);
    stereo->setProperty ("bandCorrLow", stats.bandCorrLow);
    stereo->setProperty ("bandCorrMid", stats.bandCorrMid);
    stereo->setProperty ("bandCorrHigh", stats.bandCorrHigh);
    root->setProperty ("stereo", stereo);

    juce::Array<juce::var> ratios;

    for (float v : bandRatios)
        ratios.add (v);

    root->setProperty ("bandRatios", ratios);

    juce::Array<juce::var> platformResults;

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (stats, p);

        auto* o = new juce::DynamicObject();
        o->setProperty ("id", p.id);
        o->setProperty ("targetLufs", p.targetLufs);
        o->setProperty ("gainDb", res.gainDb);
        o->setProperty ("predictedTp", res.predictedTp);
        o->setProperty ("state", (int) res.state);
        platformResults.add (juce::var (o));
    }

    root->setProperty ("platforms", platformResults);

    juce::Array<juce::var> markerArray;

    for (const auto& m : markers)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("time", m.time);
        o->setProperty ("severity", m.severity);
        o->setProperty ("category", m.category);
        o->setProperty ("text", m.text);
        markerArray.add (juce::var (o));
    }

    root->setProperty ("markers", markerArray);
    root->setProperty ("version", ml::kVersion);
    root->setProperty ("developer", ml::kDeveloper);
    root->setProperty ("organisation", ml::kOrganisation);

    return juce::JSON::toString (juce::var (root), false);
}

void MasterLensAudioProcessorEditor::setPageForSnapshot (int page)
{
    rail.setSelected (page);
    detail.setPage ((ml::PageId) page);
    repaint();
}

void MasterLensAudioProcessorEditor::expandFirstIssueForSnapshot()
{
    detail.expandFirstIssueForSnapshot();
}







