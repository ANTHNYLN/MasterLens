#include "PluginEditor.h"

using ml::T;

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

    addAndMakeVisible (spectrumCard);
    addAndMakeVisible (issuesCard);
    addAndMakeVisible (loudnessCard);
    addAndMakeVisible (historyCard);
    addAndMakeVisible (metersCard);

    spectrumCard.onLayout = [this] (juce::Rectangle<int> r)
    {
        bands.setBounds (r.removeFromBottom (84));
        spectrum.setBounds (r.withTrimmedBottom (4));
    };

    issuesCard.onLayout = [this] (juce::Rectangle<int> r)
    {
        issueList.setBounds (r);
    };

    loudnessCard.onLayout = [this] (juce::Rectangle<int> r)
    {
        levelBars.setBounds (r.removeFromBottom (52));
        tiles.setBounds (r.withTrimmedBottom (6));
    };

    historyCard.onLayout = [this] (juce::Rectangle<int> r)
    {
        historyGraph.setBounds (r);
    };

    metersCard.onLayout = [this] (juce::Rectangle<int> r)
    {
        meters.setBounds (r);
    };

    spectrumCard.addAndMakeVisible (spectrum);
    spectrumCard.addAndMakeVisible (bands);
    issuesCard.addAndMakeVisible (issueList);
    loudnessCard.addAndMakeVisible (tiles);
    loudnessCard.addAndMakeVisible (levelBars);
    historyCard.addAndMakeVisible (historyGraph);
    metersCard.addAndMakeVisible (meters);

    spectrum.setAnalyzer (&analyzer);

    // ---- header controls ----------------------------------------------------
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
    targetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
    targetSlider.setColour (juce::Slider::textBoxTextColourId, ml::theme::text);
    addAndMakeVisible (targetSlider);

    ceilingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    ceilingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
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

    exportButton.onClick = [this] { exportReport(); };
    addAndMakeVisible (exportButton);

    languageButton.onClick = [this]
    {
        const auto next = ml::Localisation::isEnglish() ? ml::Language::chinese : ml::Language::english;
        ml::Localisation::set (next);
        processor.apvts.state.setProperty ("language", next == ml::Language::english ? 1 : 0, nullptr);
        updateTexts();
        repaint();
    };
    addAndMakeVisible (languageButton);

    {
        const int storedLanguage = (int) processor.apvts.state.getProperty ("language", 0);
        ml::Localisation::set (storedLanguage == 1 ? ml::Language::english : ml::Language::chinese);
    }

    // ---- window -------------------------------------------------------------
    const int storedW = (int) processor.apvts.state.getProperty ("editorWidth", 1180);
    const int storedH = (int) processor.apvts.state.getProperty ("editorHeight", 760);

    setResizable (true, true);
    setResizeLimits (1120, 700, 1800, 1200);
    setSize (juce::jlimit (1120, 1800, storedW), juce::jlimit (700, 1200, storedH));

    updateTexts();
    syncPresetToParameters();

    analyzerSampleRate = processor.getSampleRateValue();
    analyzer.prepare (analyzerSampleRate, 12);

    startTimerHz (60);
}

MasterLensAudioProcessorEditor::~MasterLensAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void MasterLensAudioProcessorEditor::updateTexts()
{
    const bool en = ml::Localisation::isEnglish();

    spectrumCard.setTitle (T ("实时频谱", "Real-time spectrum"));
    spectrumCard.setSubtitle (T ("FFT 4096 · 60 fps · 对数频率", "FFT 4096 · 60 fps · log scale"));

    loudnessCard.setTitle (T ("母带响度", "Mastering loudness"));
    loudnessCard.setSubtitle (T ("ITU-R BS.1770-4 · EBU R128", "ITU-R BS.1770-4 · EBU R128"));

    historyCard.setTitle (T ("短时响度历史", "Short-term history"));
    historyCard.setSubtitle (T ("最近 3 分钟", "last 3 minutes"));

    metersCard.setTitle (T ("声场与失真", "Stereo & distortion"));
    metersCard.setSubtitle (T ("相关性 · 宽度 · 低频 · 事件", "correlation · width · low end · events"));

    issuesCard.setTitle (T ("混音质量诊断", "Mix quality diagnostics"));

    resetButton.setButtonText (T ("重置", "Reset"));
    freezeButton.setButtonText (frozen ? T ("恢复", "Resume") : T ("冻结", "Freeze"));
    exportButton.setButtonText (T ("导出报告", "Export"));
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

    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);

    juce::ColourGradient wash (juce::Colour (0xffe8f0ff), 0.0f, 0.0f,
                               ml::theme::background.withAlpha (0.0f), 0.0f, 170.0f, false);
    g.setGradientFill (wash);
    g.fillRect (0, 0, getWidth(), 170);

    auto header = getLocalBounds().reduced (16, 12).removeFromTop (48);
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

    g.setColour (ml::theme::text);
    g.setFont (ml::uiFont (19.0f, true));
    g.drawText ("MasterLens", (int) logo.getRight() + 12, header.getY() - 1, 240, 24,
                juce::Justification::centredLeft);

    g.setColour (ml::theme::textDim);
    g.setFont (ml::uiFont (10.5f));
    g.drawText (T ("母带响度检查 · 混音质量诊断 · 实时频谱",
                   "Mastering loudness · mix diagnostics · real-time spectrum"),
                (int) logo.getRight() + 12, header.getY() + 22, 340, 16,
                juce::Justification::centredLeft);

    if (frozen)
    {
        const auto pill = juce::Rectangle<int> ((int) logo.getRight() + 352, header.getY() + 22, 54, 17);
        g.setColour (ml::theme::warn.withAlpha (0.16f));
        g.fillRoundedRectangle (pill.toFloat(), 8.0f);
        g.setColour (ml::theme::warn);
        g.setFont (ml::uiFont (9.5f));
        g.drawText (T ("已冻结", "Frozen"), pill, juce::Justification::centred);
    }
}

void MasterLensAudioProcessorEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (72).reduced (16, 12);
    auto controls = header;

    languageButton.setBounds (controls.removeFromRight (46).withSizeKeepingCentre (46, 30));
    controls.removeFromRight (8);
    exportButton.setBounds (controls.removeFromRight (84).withSizeKeepingCentre (84, 30));
    controls.removeFromRight (8);
    freezeButton.setBounds (controls.removeFromRight (68).withSizeKeepingCentre (68, 30));
    controls.removeFromRight (8);
    resetButton.setBounds (controls.removeFromRight (62).withSizeKeepingCentre (62, 30));
    controls.removeFromRight (16);

    auto ceilingBlock = controls.removeFromRight (140);
    controls.removeFromRight (12);
    auto targetBlock = controls.removeFromRight (152);
    controls.removeFromRight (12);

    const int presetW = juce::jmin (170, juce::jmax (120, controls.getWidth()));
    presetBox.setBounds (controls.removeFromRight (presetW).withSizeKeepingCentre (presetW, 30));

    ceilingCaption.setBounds (ceilingBlock.removeFromTop (16));
    ceilingSlider.setBounds (ceilingBlock.removeFromTop (22));

    targetCaption.setBounds (targetBlock.removeFromTop (16));
    targetSlider.setBounds (targetBlock.removeFromTop (22));

    auto content = getLocalBounds().reduced (16, 0).withTrimmedTop (66).withTrimmedBottom (14);

    const int gap = 12;
    const int leftW = juce::roundToInt ((content.getWidth() - gap) * 0.585f);

    auto left = content.removeFromLeft (leftW);
    content.removeFromLeft (gap);
    auto right = content;

    const int specH = juce::roundToInt ((left.getHeight() - gap) * 0.505f);
    spectrumCard.setBounds (left.removeFromTop (specH));
    left.removeFromTop (gap);
    issuesCard.setBounds (left);

    const int usableH = right.getHeight() - 2 * gap;
    const int loudH = juce::roundToInt (usableH * 0.36f);
    const int histH = juce::roundToInt (usableH * 0.26f);

    loudnessCard.setBounds (right.removeFromTop (loudH));
    right.removeFromTop (gap);
    historyCard.setBounds (right.removeFromTop (histH));
    right.removeFromTop (gap);
    metersCard.setBounds (right);

    processor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    processor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

//==============================================================================
void MasterLensAudioProcessorEditor::timerCallback()
{
    const double sr = processor.getSampleRateValue();

    if (std::abs (sr - analyzerSampleRate) > 0.5)
    {
        analyzerSampleRate = sr;
        analyzer.prepare (sr, 12);
    }

    analyzer.setSmoothing (processor.getSmoothing());
    analyzer.update (processor.spectrumRing);

    if (frozen)
        return;

    stats = processor.getStatsSnapshot();

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    advisor.update (stats, analyzer.getBandDeviations(), target, ceiling);
    processor.copyShortTermCurve (curve, 1800);

    spectrum.repaint();
    bands.setDeviations (analyzer.getBandDeviations());
    tiles.setValues (stats, target, ceiling);
    levelBars.setValues (stats, target);
    historyGraph.setData (curve, target);
    meters.setValues (stats);
    issueList.setIssues (advisor.getIssues());

    if (++frameCounter % 8 == 0)
    {
        juce::String sub;

        if (! stats.hasSignal)
        {
            sub = T ("等待音频输入", "Waiting for audio input");
        }
        else if (ml::Localisation::isEnglish())
        {
            sub = advisor.getHeadline()
                  + " · " + juce::String (advisor.count (ml::Severity::critical)) + " action"
                  + " · " + juce::String (advisor.count (ml::Severity::warn)) + " review";
        }
        else
        {
            sub = advisor.getHeadline()
                  + T (" · 需处理 ", " · ") + juce::String (advisor.count (ml::Severity::critical))
                  + T (" · 建议 ", " · ") + juce::String (advisor.count (ml::Severity::warn));
        }

        issuesCard.setSubtitle (sub);
        issuesCard.setAccent (ml::severityColour (advisor.getOverallSeverity()));
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

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    tiles.setValues (stats, target, ceiling);
    levelBars.setValues (stats, target);
    meters.setValues (stats);
    historyGraph.setData ({}, target);
    issueList.setIssues ({});
    bands.setDeviations (std::vector<float> ((size_t) ml::SpectrumAnalyzer::numBands, 0.0f));

    issuesCard.setSubtitle (T ("等待信号", "Waiting for signal"));
    issuesCard.setAccent (ml::theme::accent);
}

//==============================================================================
void MasterLensAudioProcessorEditor::exportReport()
{
    const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                 .getChildFile ("MasterLens-Report.txt");

    fileChooser = std::make_unique<juce::FileChooser> (T ("导出 MasterLens 分析报告", "Export MasterLens report"),
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
                              });
}

juce::String MasterLensAudioProcessorEditor::buildReport() const
{
    juce::String r;

    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();

    r << T ("MasterLens · 母带与混音质量报告\n", "MasterLens - mastering & mix report\n");
    r << T ("生成时间：", "Generated: ")
      << juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") << "\n";
    r << T ("采样率：", "Sample rate: ") << juce::String (analyzerSampleRate, 0) << " Hz   "
      << T ("声道：", "Channels: ")
      << (stats.stereo ? T ("立体声", "stereo") : T ("单声道", "mono")) << "\n\n";

    r << T ("【母带响度】\n", "[Mastering loudness]\n");
    r << T ("  综合响度 I      : ", "  Integrated (I)   : ") << ml::fmtDb (stats.integrated) << " LUFS"
      << T ("（目标 ", " (target ") << ml::fmt1 (target) << T (" LUFS）\n", " LUFS)\n");
    r << T ("  短时响度 S      : ", "  Short-term (S)   : ") << ml::fmtDb (stats.shortTerm) << " LUFS\n";
    r << T ("  瞬时响度 M      : ", "  Momentary  (M)   : ") << ml::fmtDb (stats.momentary) << " LUFS\n";
    r << T ("  最大短时 / 瞬时 : ", "  Max S / M        : ") << ml::fmtDb (stats.maxShortTerm) << " / "
      << ml::fmtDb (stats.maxMomentary) << " LUFS\n";
    r << T ("  响度范围 LRA    : ", "  Loudness range   : ")
      << (stats.lra > 0.05f ? ml::fmt1 (stats.lra) : juce::String ("--")) << " LU\n";
    r << T ("  峰响比 PLR      : ", "  Peak / loudness  : ")
      << (stats.integrated > -70.0f ? ml::fmt1 (stats.getPlr()) : juce::String ("--")) << " dB\n";
    r << T ("  真峰值 TP       : ", "  True peak        : ") << ml::fmtDb (stats.truePeakDb) << " dBTP"
      << T ("（上限 ", " (ceiling ") << ml::fmt1 (ceiling) << T (" dBTP）\n", " dBTP)\n");
    r << T ("  采样峰值        : ", "  Sample peak      : ") << ml::fmtDb (stats.samplePeakDb) << " dBFS\n\n";

    r << T ("【声场与失真】\n", "[Stereo & distortion]\n");
    r << T ("  声道相关性      : ", "  Correlation      : ")
      << juce::String ((double) stats.correlation, 2) << "\n";
    r << T ("  立体声宽度      : ", "  Stereo width     : ") << ml::fmt1 (stats.widthPct) << " %\n";
    r << T ("  120 Hz 以下侧向 : ", "  Side below 120 Hz: ") << ml::fmt1 (stats.lowSidePct) << " %\n";
    r << T ("  直流偏移        : ", "  DC offset        : ") << ml::fmtDb (stats.dcOffsetDb) << " dBFS\n";
    r << T ("  削波 / 过载 / 瞬态异常 : ", "  Clip / overs / clicks : ") << stats.clipEvents << " / "
      << stats.oversCount << " / " << stats.clickEvents << "\n\n";

    r << T ("【频段平衡（相对参考，dB）】\n", "[Tonal balance (dB vs reference)]\n");

    const auto& devs = analyzer.getBandDeviations();

    for (int i = 0; i < ml::SpectrumAnalyzer::numBands; ++i)
    {
        const auto& info = ml::SpectrumAnalyzer::getBandInfo (i);
        const float dev = i < (int) devs.size() ? devs[(size_t) i] : 0.0f;

        r << "  " << T (info.label, info.labelEn) << " ("
          << juce::String (info.lo, 0) << "-" << juce::String (info.hi, 0) << " Hz): "
          << (dev >= 0.0f ? "+" : "") << juce::String ((double) dev, 1) << "\n";
    }

    r << T ("\n【诊断】\n", "\n[Diagnostics]\n");

    const auto& issues = advisor.getIssues();

    if (issues.empty())
        r << T ("  等待音频输入或尚无可评估内容。\n",
                "  Waiting for audio or nothing to evaluate yet.\n");

    for (const auto& i : issues)
    {
        const juce::String tag = i.severity == ml::Severity::critical ? T ("需要处理", "Action")
                               : i.severity == ml::Severity::warn ? T ("建议检查", "Review")
                               : i.severity == ml::Severity::info ? T ("可优化", "Improve")
                                                                  : T ("良好", "Good");

        r << "  [" << tag << "] " << i.title;

        if (i.timed)
            r << T ("（", " (") << ml::fmtTime (i.detectedAt) << T (" 检出）", " detected)");

        r << "\n    " << i.detail << "\n";
    }

    r << T ("\n—— 由 MasterLens 生成（ITU-R BS.1770-4 / EBU R128 测量）\n",
           "\n-- Generated by MasterLens (ITU-R BS.1770-4 / EBU R128 measurement)\n");

    return r;
}



