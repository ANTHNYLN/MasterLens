#include "Widgets.h"

#include <cmath>

namespace ml
{
/** Builds the pass / fail list shown on the diagnostics page.

    Every threshold below is a hard-coded, documented number (or the BS.1770
    gating result) - nothing is learned, fetched or guessed.
*/
std::vector<CheckResult> buildChecklist (const StatsSnapshot& s,
                                         float targetLufs,
                                         float ceilingDb,
                                         const std::vector<float>& bandDevs)
{
    std::vector<CheckResult> rows;
    rows.reserve (9);

    const bool live = s.hasSignal;

    const auto add = [&rows] (juce::String label, juce::String value,
                              Severity severity, juce::String hint)
    {
        CheckResult r;
        r.label = std::move (label);
        r.value = std::move (value);
        r.severity = severity;
        r.hint = std::move (hint);
        rows.push_back (std::move (r));
    };

    // 1) integrated loudness vs user target (hard limit: +/- 2 LU)
    {
        const bool valid = live && s.integrated > -70.0f;
        const float diff = s.integrated - targetLufs;
        const auto sev = ! valid ? Severity::info
                       : (std::abs (diff) <= 1.0f ? Severity::good
                       : (std::abs (diff) <= 2.0f ? Severity::info : Severity::warn));

        add (T ("综合响度 / 目标", "Integrated / target"),
             valid ? num (s.integrated) + " LUFS  (" + sgn (diff) + " LU)" : noValue(), sev,
             T ("目标 ", "Target ") + num (targetLufs) + T (" LUFS，容许 ±2 LU", " LUFS, +/- 2 LU allowed"));
    }

    // 2) true peak vs ceiling (hard limit)
    {
        const bool valid = live && s.truePeakDb > -70.0f;
        const float headroom = ceilingDb - s.truePeakDb;
        const auto sev = ! valid ? Severity::info
                       : (headroom < 0.0f ? Severity::critical
                       : (headroom < 1.0f ? Severity::warn : Severity::good));

        add (T ("真峰值 / 上限", "True peak / ceiling"),
             valid ? num (s.truePeakDb) + " dBTP  (" + T ("余量 ", "headroom ") + num (headroom) + ")"
                   : noValue(), sev,
             T ("上限 ", "Ceiling ") + num (ceilingDb) + " dBTP · 4× oversampling");
    }

    // 3) loudness range 3 - 16 LU
    {
        const bool valid = live && s.lra > 0.05f;
        const auto sev = ! valid ? Severity::info
                       : (s.lra < 3.0f ? Severity::warn
                       : (s.lra > 16.0f ? Severity::info : Severity::good));

        add (T ("响度范围 / LRA", "Loudness range"),
             valid ? num (s.lra) + " LU" : noValue(), sev,
             T ("参考 ", "Typical ") + fmtRange (3.0f, 16.0f) + " LU");
    }

    // 4) peak to loudness ratio >= 10 dB
    {
        const bool valid = live && s.integrated > -70.0f && s.truePeakDb > -70.0f;
        const float plr = s.getPlr();
        const auto sev = ! valid ? Severity::info
                       : (plr < 8.0f ? Severity::warn
                       : (plr < 10.0f ? Severity::info : Severity::good));

        add (T ("峰响比 / PLR", "Peak to loudness"),
             valid ? num (plr) + " dB" : noValue(), sev,
             T ("参考 ", "Typical ") + fmtRange (10.0f, 16.0f) + " dB");
    }

    // 5) phase correlation
    {
        const bool valid = live && s.stereo;
        const auto sev = ! valid ? Severity::info
                       : (s.correlation < 0.0f ? Severity::critical
                       : (s.correlation < 0.15f ? Severity::warn : Severity::good));

        add (T ("声道相关性", "Phase correlation"),
             valid ? sgn (s.correlation, 2) : noValue(), sev,
             T ("≥ 0.15 为健康，< 0 表示反相", ">= 0.15 is healthy, < 0 means out of phase"));
    }

    // 6) low-end side energy
    {
        const bool valid = live && s.stereo;
        const auto sev = ! valid ? Severity::info
                       : (s.lowSidePct > 50.0f ? Severity::critical
                       : (s.lowSidePct > 35.0f ? Severity::warn : Severity::good));

        add (T ("低频侧向能量", "Low-end side energy"),
             valid ? num (s.lowSidePct) + "%" : noValue(), sev,
             T ("低于 35%，单声道折叠不损失低频", "Below 35% so mono folding keeps the low end"));
    }

    // 7) DC offset
    {
        const bool valid = live && s.dcOffsetDb > -99.0f;
        const auto sev = ! valid ? Severity::info
                       : (s.dcOffsetDb > -46.0f ? Severity::warn : Severity::good);

        add (T ("直流偏移 / DC", "DC offset"),
             valid ? num (s.dcOffsetDb) + " dB" : noValue(), sev,
             T ("低于 −46 dB 视为正常", "Below -46 dB is normal"));
    }

    // 8) distortion events
    {
        const int events = s.clipEvents + s.clickEvents;
        const auto sev = ! live ? Severity::info
                       : (s.clipEvents > 0 ? Severity::critical
                       : (s.clickEvents > 0 ? Severity::warn : Severity::good));

        add (T ("削波 / 爆音事件", "Clip / click events"),
             live ? juce::String (events) + T (" 次", " events") : noValue(), sev,
             T ("削波 0 次、爆音 0 次为合格", "0 clips and 0 clicks is the target"));
    }

    // 9) tonal balance (reference deviation)
    {
        float worst = 0.0f;

        for (float d : bandDevs)
            worst = juce::jmax (worst, std::abs (d));

        const bool valid = live && bandDevs.size() == (size_t) SpectrumAnalyzer::numBands;
        const auto sev = ! valid ? Severity::info
                       : (worst < 4.5f ? Severity::good
                       : (worst < 6.0f ? Severity::info : Severity::warn));

        add (T ("频段平衡 / 参考", "Tonal balance / reference"),
             valid ? T ("最大偏差 ", "Max deviation ") + num (worst) + " dB" : noValue(), sev,
             T ("各频段相对参考 ≤ ±4.5 dB", "Every band within +/- 4.5 dB of the reference"));
    }

    return rows;
}
} // namespace ml
