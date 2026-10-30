#include "PluginEditor.h"

using ml::T;
#include "Delivery.h"
#include "ui/StatusText.h"

using ml::statusLabel;
using ml::statusColour;

//==============================================================================
void MasterLensAudioProcessorEditor::updateRail()
{
    const float target = processor.getTargetLufs();
    const float ceiling = processor.getCeilingDb();
    const bool live = stats.hasSignal;

    int worst = 0;

    const auto note = [&worst] (int level)
    {
        if (level > worst)
            worst = level;
    };

    // 0 loudness
    {
        const bool valid = live && stats.integrated > -70.0f;
        const float diff = stats.integrated - target;
        const int level = ! valid ? -1 : (std::abs (diff) <= 1.0f ? 0
                        : (std::abs (diff) <= 2.0f ? 1 : 2));

        if (valid) note (level);
        rail.setModule (0, T ("响度与动态", "Loudness"),
                        valid ? ml::num (stats.integrated) + " LUFS" : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 1 true peak
    {
        const bool valid = live && stats.truePeakDb > -70.0f;
        const float headroom = ceiling - stats.truePeakDb;
        const int level = ! valid ? -1 : (headroom < 0.0f ? 3 : (headroom < 1.0f ? 2 : 0));

        if (valid) note (level);
        rail.setModule (1, T ("真峰值", "True peak"),
                        valid ? ml::num (stats.truePeakDb) + " dBTP" : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 2 spectrum
    {
        float worstDev = 0.0f;

        for (float d : analyzer.getBandDeviations())
            worstDev = juce::jmax (worstDev, std::abs (d));

        const bool valid = live && worstDev > 0.0f;
        const int level = ! valid ? -1 : (worstDev < 4.5f ? 0 : (worstDev < 6.0f ? 1 : 2));

        if (valid) note (level);
        rail.setModule (2, T ("频谱平衡", "Spectrum"),
                        valid ? ml::plusMinus() + ml::num (worstDev) + " dB" : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 3 stereo
    {
        const bool valid = live && stats.stereo;
        const int level = ! valid ? -1 : (stats.correlation < 0.0f ? 3
                        : (stats.correlation < 0.15f ? 2 : 0));

        if (valid) note (level);
        rail.setModule (3, T ("立体声 / 相位", "Stereo / phase"),
                        valid ? T ("相关 ", "corr ") + ml::sgn (stats.correlation, 2) : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 4 delivery
    {
        int over = 0, warning = 0;

        for (int i = 0; i < ml::numPlatforms; ++i)
        {
            const auto r = ml::evaluate (stats, ml::platform (i));

            if (r.state == ml::DeliveryState::over)
                ++over;
            else if (r.state == ml::DeliveryState::warning)
                ++warning;
        }

        const bool valid = live && stats.integrated > -70.0f;
        const int level = ! valid ? -1 : (over > 0 ? 3 : (warning > 0 ? 2 : 0));

        if (valid) note (level);
        rail.setModule (4, T ("平台交付", "Delivery"),
                        valid ? juce::String (ml::numPlatforms - over - warning) + "/6" : ml::noValue(),
                        statusLabel (level), statusColour (level));
    }

    // 5 diagnostics
    {
        const int critical = advisor.count (ml::Severity::critical);
        const int warned = advisor.count (ml::Severity::warn);
        const int level = ! live ? -1 : (critical > 0 ? 3 : (warned > 0 ? 2 : 0));

        if (level >= 0) note (level);
        rail.setModule (5, T ("混音诊断", "Diagnostics"),
                        juce::String ((int) issueCache.size()) + T (" 项", " items"),
                        statusLabel (level), statusColour (level));
    }

    // 6 timeline
    rail.setModule (6, T ("时间线", "Timeline"),
                    juce::String ((int) markers.size()) + T (" 标记", " markers"),
                    markers.empty() ? statusLabel (-1) : statusLabel (1),
                    markers.empty() ? statusColour (-1) : statusColour (1));

    if (! live)
    {
        footerStatusText = ml::statusLabel (-1);
        footerStatusColour = ml::statusColour (-1);
        rail.setVerdict (T ("等待信号", "Waiting for signal"),
                         T ("播放音频后开始测量", "Play audio to start"),
                         ml::theme::textFaint);
    }
    else if (worst >= 3)
    {
        footerStatusText = ml::statusLabel (3);
        footerStatusColour = ml::statusColour (3);
        rail.setVerdict (T ("需要处理", "Action needed"),
                         T ("硬性超限，请先处理", "Hard limit exceeded"), ml::theme::bad);
    }
    else if (worst == 2)
    {
        footerStatusText = ml::statusLabel (2);
        footerStatusColour = ml::statusColour (2);
        rail.setVerdict (T ("建议检查", "Review suggested"),
                         T ("存在临界项", "Borderline readings"), ml::theme::warn);
    }
    else
    {
        footerStatusText = ml::statusLabel (0);
        footerStatusColour = ml::statusColour (0);
        rail.setVerdict (T ("可交付", "Ready to deliver"),
                         T ("全部静态阈值通过", "All static checks passed"), ml::theme::good);
    }
}
