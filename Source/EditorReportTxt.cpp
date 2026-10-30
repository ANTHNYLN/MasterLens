#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "Version.h"

//==============================================================================
juce::String MasterLensAudioProcessorEditor::buildReport() const
{
    juce::String r;
    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();
    const auto& s = stats;

    r << "MasterLens " << ml::kVersion << T (" · 母带交付检查报告\n", " - mastering QC report\n");
    r << T ("生成时间：", "Generated: ") << juce::Time::getCurrentTime().toISO8601 (true) << "\n";
    r << T ("采样率：", "Sample rate: ") << (int) processor.getSampleRateValue() << " Hz   "
      << T ("声道：", "Channels: ") << (s.stereo ? T ("立体声", "stereo") : T ("单声道", "mono")) << "\n\n";

    r << T ("【响度与动态】\n", "[Loudness & dynamics]\n");
    r << "  I            : " << ml::fmtDb (s.integrated) << " LUFS"
      << T ("（目标 ", " (target ") << ml::num (target) << " LUFS)\n";
    r << "  S / M        : " << ml::fmtDb (s.shortTerm) << " / " << ml::fmtDb (s.momentary) << " LUFS\n";
    r << "  LRA / PLR    : " << ml::fmt1 (s.lra) << " LU / " << ml::fmt1 (s.getPlr()) << " dB\n";
    r << "  PSR (min)    : " << ml::fmtDb (s.psr) << " / " << ml::fmtDb (s.psrMin) << " dB"
      << " @ " << ml::fmtTime (s.psrMinTime) << "\n\n";

    r << T ("【真峰值】\n", "[True peak]\n");
    r << "  TP max       : " << ml::fmtDb (s.truePeakDb) << " dBTP"
      << T ("（上限 ", " (ceiling ") << ml::num (ceiling) << " dBTP)\n";
    r << T ("  最大值位置   : ", "  Maximum at   : ") << ml::fmtTime (s.truePeakMaxTime) << "\n";
    r << T ("  跨采样过载   : ", "  Over windows : ") << s.oversCount << "\n";

    for (const auto& e : tpEvents)
        r << "    " << ml::fmtTime (e.time) << "  +" << ml::fmt1 (e.overshootDb) << " dB\n";

    r << "\n" << T ("【频谱 / 立体声】\n", "[Spectrum / stereo]\n");
    r << "  20-250 / 250-2k / 2k-8k / 8k-20k : "
      << ml::fmt1 (bandRatios[0]) << "% / " << ml::fmt1 (bandRatios[1]) << "% / "
      << ml::fmt1 (bandRatios[2]) << "% / " << ml::fmt1 (bandRatios[3]) << "%\n";
    r << "  corr / width / low-side : " << ml::sgn (s.correlation, 2) << " / "
      << ml::fmt1 (s.widthPct) << "% / " << ml::fmt1 (s.lowSidePct) << "%\n";

    r << "\n" << T ("【平台交付预测】\n", "[Delivery prediction]\n");

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto res = ml::evaluate (s, p);

        r << "  " << T (p.nameZh, p.nameEn) << " : "
          << T ("增益 ", "gain ") << ml::sgn (res.gainDb) << " dB, "
          << T ("预测 TP ", "predicted TP ") << ml::fmt1 (res.predictedTp) << " dBTP, "
          << (res.state == ml::DeliveryState::over ? T ("超限", "OVER")
            : res.state == ml::DeliveryState::warning ? T ("临界", "WARNING") : T ("可行", "OK")) << "\n";
    }

    r << "\n" << T ("【时间线标记】\n", "[Timeline markers]\n");

    if (markers.empty())
        r << T ("  无\n", "  none\n");

    for (const auto& m : markers)
        r << "  " << ml::fmtTime (m.time) << "  " << m.text << "\n";

    r << "\n" << T ("【诊断】\n", "[Diagnostics]\n");

    if (issueCache.empty())
        r << T ("  未发现需要处理的项目\n", "  No action items\n");

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
