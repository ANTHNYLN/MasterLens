#include "Advisor.h"
#include "../Text.h"
#include "../dsp/SpectrumAnalyzer.h"
#include "Localisation.h"

#include <algorithm>

namespace ml
{
static juce::String one (double v) { return juce::String (v, 1); }
static juce::String oneSigned (double v) { return (v >= 0.0 ? "+" : "") + juce::String (v, 1); }
static juce::String twoSigned (double v) { return (v >= 0.0 ? "+" : "") + juce::String (v, 2); }

void Advisor::reset()
{
    conditions.clear();
    visible.clear();
    overall = Severity::good;
    frame = 0;
}

juce::String Advisor::getHeadline() const
{
    switch (overall)
    {
        case Severity::critical: return T ("需要处理", "Action needed");
        case Severity::warn:     return T ("建议检查", "Review suggested");
        case Severity::info:     return T ("可优化", "Could improve");
        default:                 return T ("状态良好", "Healthy");
    }
}

//==============================================================================
void Advisor::update (const StatsSnapshot& stats,
                      const std::vector<float>& bandDevs,
                      float targetLufs,
                      float ceilingDb)
{
    ++frame;

    std::vector<Issue> found;
    const bool signal = stats.hasSignal && stats.shortTerm > -65.0f;

    const auto add = [&found] (juce::String id, juce::String title, const juce::String& detail,
                               Severity sev, bool timed = false)
    {
        Issue i;
        i.id = std::move (id);
        i.title = std::move (title);
        i.detail = detail;
        i.severity = sev;
        i.timed = timed;
        found.push_back (std::move (i));
    };

    // ---------------------------------------------------------------- loudness
    if (stats.integrated > -70.0f)
    {
        const double diff = (double) stats.integrated - (double) targetLufs;

        if (diff > 2.0)
            add ("loud-high", T ("整体响度高于目标", "Louder than target"),
                 TF (T ("综合响度 {i} LUFS，比目标 {t} LUFS 高 {d} LU。流媒体归一化后会整体衰减，压缩余量被浪费。",
                        "Integrated {i} LUFS, {d} LU louder than the {t} LUFS target. Streaming "
                        "normalisation will turn it down, so the extra limiting buys nothing."),
                     { { "i", one (stats.integrated) }, { "t", one (targetLufs) }, { "d", one (diff) } }),
                 Severity::warn);
        else if (diff < -2.5)
            add ("loud-low", T ("整体响度低于目标", "Quieter than target"),
                 TF (T ("综合响度 {i} LUFS，比目标低 {d} LU。以流媒体为目标时可适度提升；若要保留动态则无需处理。",
                        "Integrated {i} LUFS, {d} LU below the {t} LUFS target. Raise it if you are "
                        "mastering for streaming; keep it if you want the dynamics."),
                     { { "i", one (stats.integrated) }, { "t", one (targetLufs) }, { "d", one (-diff) } }),
                 Severity::info);
        else
            add ("loud-ok", T ("综合响度已对齐目标", "Integrated loudness on target"),
                 TF (T ("综合响度 {i} LUFS，处于目标 ±2 LU 范围内。",
                        "Integrated {i} LUFS, within ±2 LU of the {t} LUFS target."),
                     { { "i", one (stats.integrated) }, { "t", one (targetLufs) } }),
                 Severity::good);
    }

    if (stats.truePeakDb > (double) ceilingDb + 0.05)
        add ("tp-over", T ("真峰值过载", "True peak over ceiling"),
             TF (T ("真峰值 {tp} dBTP，超出上限 {ceiling} dBTP 共 {d} dB。转为 AAC / MP3 后会出现削波失真，请在限制器预留余量。",
                    "True peak {tp} dBTP is {d} dB above the {ceiling} dBTP ceiling. Lossy encoding "
                    "will clip - leave headroom on the limiter."),
                 { { "tp", one (stats.truePeakDb) }, { "ceiling", one (ceilingDb) },
                   { "d", one (stats.truePeakDb - ceilingDb) } }),
             Severity::critical);
    else if (stats.truePeakDb > -70.0f && stats.truePeakDb > ceilingDb - 0.1f)
        add ("tp-edge", T ("真峰值贴近上限", "True peak close to ceiling"),
             TF (T ("真峰值 {tp} dBTP，与上限仅差 {d} dB，编码后可能越过 0 dBTP。",
                    "True peak {tp} dBTP is only {d} dB below the ceiling; encoding may push it past 0 dBTP."),
                 { { "tp", one (stats.truePeakDb) }, { "d", one (ceilingDb - stats.truePeakDb) } }),
             Severity::info);

    // ---------------------------------------------------------------- dynamics
    if (stats.lra > 0.0f)
    {
        if (stats.lra < 3.0f)
            add ("lra-flat", T ("动态被过度压缩", "Dynamics over-compressed"),
                 TF (T ("响度范围仅 {v} LU（参考 5–12 LU）。持续压缩会让音乐失去起伏与呼吸感。",
                        "Loudness range is only {v} LU (5-12 LU is typical). Constant compression "
                        "flattens the rise and fall of the music."),
                     { { "v", one (stats.lra) } }),
                 Severity::warn);
        else if (stats.lra > 16.0f)
            add ("lra-wide", T ("响度范围偏大", "Loudness range very wide"),
                 TF (T ("响度范围 {v} LU，段落音量差异明显，检查是否有段落未做电平对齐。",
                        "Loudness range is {v} LU. Sections differ a lot in level - check whether "
                        "one of them was left unaligned."),
                     { { "v", one (stats.lra) } }),
                 Severity::info);
    }

    if (stats.integrated > -70.0f && stats.truePeakDb > -70.0f)
    {
        const float plr = stats.getPlr();

        if (plr < 8.0f)
            add ("plr-low", T ("瞬态被压扁", "Transients squashed"),
                 TF (T ("峰响比仅 {v} dB（参考 10–16 dB）。鼓点与拨弦的冲击力被限制器吃掉，声音趋于扁平。",
                        "Peak-to-loudness ratio is only {v} dB (10-16 dB is typical). The limiter "
                        "is eating the punch of kicks and plucks."),
                     { { "v", one (plr) } }),
                 Severity::warn);
    }

    // ------------------------------------------------------------------ stereo
    if (stats.stereo)
    {
        if (stats.correlation < 0.0f)
            add ("corr-neg", T ("存在相位冲突", "Phase conflict"),
                 TF (T ("左右声道相关性 {v}，低于 0。折叠为单声道时中低频会被抵消，手机 / 蓝牙音箱上会明显变薄。",
                        "Channel correlation is {v}, below zero. Folding to mono cancels the "
                        "low-mids and the mix will sound thin on phones and Bluetooth speakers."),
                     { { "v", twoSigned (stats.correlation) } }),
                 Severity::critical);
        else if (stats.correlation < 0.15f && stats.shortTerm > -50.0f)
            add ("corr-low", T ("单声道兼容性偏低", "Mono compatibility weak"),
                 TF (T ("声道相关性仅 {v}（理想 0.3–0.8）。折叠单声道后侧向内容会减少，检查过宽的立体声效果器与 Haas 延迟。",
                        "Correlation is only {v} (0.3-0.8 is typical). Mono folding will lose side "
                        "content - check very wide stereo effects and Haas delays."),
                     { { "v", twoSigned (stats.correlation) } }),
                 Severity::warn);

        if (stats.shortTerm > -50.0f)
        {
            if (stats.widthPct < 15.0f)
                add ("width-narrow", T ("立体声宽度偏窄", "Stereo image very narrow"),
                     TF (T ("侧向能量仅 {v}%，接近单声道。检查是否有声道重复、双单声道录音或过度中置。",
                            "Side energy is only {v}% - almost mono. Check for doubled channels, "
                            "dual-mono recordings or heavy mono-ing."),
                         { { "v", one (stats.widthPct) } }),
                     Severity::info);
            else if (stats.widthPct > 150.0f)
                add ("width-wide", T ("立体声宽度过度", "Stereo image too wide"),
                     TF (T ("侧向能量 {v}%，远超中置。单声道折叠损失大，注意相位与兼容性。",
                            "Side energy is {v}%, far above the mid. Mono folding loses a lot - "
                            "watch phase and compatibility."),
                         { { "v", one (stats.widthPct) } }),
                     Severity::warn);

            if (stats.lowSidePct > 35.0f)
                add ("low-side", T ("低频不是单声道", "Low end is not mono"),
                     TF (T ("120 Hz 以下侧向能量 {v}%。俱乐部 / 黑胶系统上低频会不稳定，甚至相位抵消；建议对 120 Hz 以下做单声道处理。",
                            "Side energy below 120 Hz is {v}%. Club and vinyl systems make the low "
                            "end unstable or even cancel it - mono the band below 120 Hz."),
                         { { "v", one (stats.lowSidePct) } }),
                     Severity::warn);
        }
    }

    // --------------------------------------------------------------- artifacts
    if (stats.clipEvents > 0)
        add ("clip", T ("检出削波失真", "Clipping detected"),
             TF (T ("共 {n} 次采样级削波（波形顶部被削平）。降低增益或限制器阈值 1–2 dB 后重新检查。",
                    "{n} sample-level clipping events (the waveform tops are flattened). Lower the "
                    "gain or the limiter threshold by 1-2 dB and check again."),
                 { { "n", juce::String (stats.clipEvents) } }),
             Severity::critical, true);

    if (stats.oversCount > 0)
        add ("overs", T ("检出跨采样过载", "Inter-sample overs detected"),
             TF (T ("{n} 个 100 ms 区间内重建峰值超过 0 dBTP。编码为有损格式后会被削波，建议上限设为 -1 dBTP。",
                    "Reconstructed peaks exceed 0 dBTP in {n} of the 100 ms windows. Lossy encoders "
                    "will clip them - set the ceiling to -1 dBTP."),
                 { { "n", juce::String (stats.oversCount) } }),
             Severity::warn, true);

    if (stats.clickEvents > 0)
        add ("clicks", T ("检出瞬态异常", "Clicks / edit points detected"),
             TF (T ("检测到 {n} 次采样跳变（可能是剪辑点、爆音或脉冲噪声）。放大波形检查接缝处。",
                    "{n} sudden sample jumps (edit points, pops or impulsive noise). Zoom into the "
                    "waveform and inspect the joins."),
                 { { "n", juce::String (stats.clickEvents) } }),
             Severity::info, true);

    if (stats.dcOffsetDb > -46.0f)
        add ("dc", T ("存在直流偏移", "DC offset present"),
             TF (T ("直流电平 {v} dBFS（4 秒均值）。会侵占动态余量、导致不对称削波，建议加 30 Hz 高通。",
                    "DC level is {v} dBFS (4 s mean). It eats headroom and causes asymmetric "
                    "clipping - add a 30 Hz high-pass."),
                 { { "v", one (stats.dcOffsetDb) } }),
             Severity::warn);

    // ----------------------------------------------------------------- spectral
    if (signal && bandDevs.size() == (size_t) SpectrumAnalyzer::numBands)
    {
        const auto dev = [&bandDevs] (int i) { return (double) bandDevs[(size_t) i]; };
        const auto label = [] (int i)
        {
            const auto& info = SpectrumAnalyzer::getBandInfo (i);
            return T (info.label, info.labelEn);
        };

        if (dev (3) > 4.5)
            add ("mud", T ("中低频堆积（浑浊）", "Low-mids muddy"),
                 TF (T ("250–500 Hz 能量比参考高 {v} dB。鼓组与贝斯容易糊在一起；尝试衰减 300–400 Hz 或给非低频乐器加高通。",
                        "250-500 Hz sits {v} dB above the reference. Kicks and bass blur together - "
                        "try cutting 300-400 Hz or high-passing the non-bass instruments."),
                     { { "v", oneSigned (dev (3)) } }),
                 Severity::warn);

        if (dev (6) > 4.5)
            add ("harsh", T ("中高频过硬（刺耳）", "Harsh upper mids"),
                 TF (T ("2–5 kHz 比参考高 {v} dB。长时间聆听容易听觉疲劳；检查失真吉他、人声 3 kHz 附近与过载的激励器。",
                        "2-5 kHz sits {v} dB above the reference. Long listening sessions become "
                        "fatiguing - check distorted guitars, the 3 kHz vocal area and driven exciters."),
                     { { "v", oneSigned (dev (6)) } }),
                 Severity::warn);

        if (dev (7) > 4.5)
            add ("sib", T ("齿音过重", "Sibilance too strong"),
                 TF (T ("5–9 kHz 比参考高 {v} dB（齿音区）。对人声使用动态均衡或齿音消除器。",
                        "5-9 kHz sits {v} dB above the reference (the sibilance band). Use a dynamic "
                        "EQ or a de-esser on the vocals."),
                     { { "v", oneSigned (dev (7)) } }),
                 Severity::warn);

        if (dev (8) < -5.0)
            add ("air-low", T ("高频空气感不足", "Lacks air"),
                 TF (T ("9–16 kHz 比参考低 {v} dB，整体听感偏闷。可适度提升高频搁架，注意不要制造齿音。",
                        "9-16 kHz sits {v} dB below the reference and the mix feels dull. A gentle "
                        "high shelf helps - but do not create sibilance."),
                     { { "v", oneSigned (dev (8)) } }),
                 Severity::info);

        if (dev (0) > 5.5)
            add ("sub", T ("超低频过重", "Too much sub"),
                 TF (T ("20–60 Hz 比参考高 {v} dB。小尺寸设备上会浪费功率并提前触发限制器，建议收紧 40 Hz 以下。",
                        "20-60 Hz sits {v} dB above the reference. It wastes power on small speakers "
                        "and trips the limiter early - tighten everything below 40 Hz."),
                     { { "v", oneSigned (dev (0)) } }),
                 Severity::info);

        if (dev (2) < -5.0)
            add ("lowmid-hole", T ("低中频塌陷", "Low-mid hole"),
                 TF (T ("120–250 Hz 比参考低 {v} dB，声音容易发空、缺乏厚度；检查高通频率与相位抵消。",
                        "120-250 Hz sits {v} dB below the reference, so the mix sounds hollow. Check "
                        "high-pass frequencies and phase cancellation."),
                     { { "v", oneSigned (dev (2)) } }),
                 Severity::info);

        const double brightTilt = (dev (6) + dev (7) + dev (8)) / 3.0
                                - (dev (1) + dev (2) + dev (3)) / 3.0;

        if (brightTilt > 6.0)
            add ("tilt-bright", T ("整体音色偏亮", "Overall tone is bright"),
                 TF (T ("高频段平均比中低频高 {v} dB，长时间聆听偏刺；适当降低高频搁架或中高频动态。",
                        "The top end averages {v} dB above the low-mids, which gets fatiguing. Ease "
                        "off the high shelf or tame the upper-mid dynamics."),
                     { { "v", oneSigned (brightTilt) } }),
                 Severity::warn);
        else if (brightTilt < -6.0)
            add ("tilt-dark", T ("整体音色偏暗", "Overall tone is dark"),
                 TF (T ("高频段平均比中低频低 {v} dB，声音偏闷、缺乏细节；检查高频衰减与母带均衡。",
                        "The top end averages {v} dB below the low-mids, so the mix sounds dull. Check "
                        "high-frequency attenuation and mastering EQ."),
                     { { "v", oneSigned (brightTilt) } }),
                 Severity::warn);

        for (int b = 1; b <= 8; ++b)
        {
            if (dev (b) < -8.0)
            {
                add ("hole-" + juce::String (b), T ("频段空缺", "Spectral hole"),
                     TF (T ("{band} 频段比参考低 {v} dB，可能是滤波过度、梳状滤波或相位抵消。",
                            "The {band} band sits {v} dB below the reference - likely over-filtering, "
                            "comb filtering or phase cancellation."),
                         { { "band", label (b) }, { "v", oneSigned (dev (b)) } }),
                     Severity::info);
                break;
            }
        }
    }

    bool anyBad = false;

    for (const auto& f : found)
        if (f.severity == Severity::warn || f.severity == Severity::critical)
            anyBad = true;

    // ---- positive confirmations (kept after the warnings in the list) ------
    if (signal)
    {
        if (stats.clipEvents == 0 && stats.oversCount == 0 && stats.clickEvents == 0)
            add ("no-clip", T ("未检出削波或爆音", "No clipping or clicks"),
                 T ("采样级削波、跨采样过载与瞬态跳变检查均为 0，波形完整。",
                    "Sample clipping, inter-sample overs and click checks all read zero - the "
                    "waveform is intact."),
                 Severity::good);

        if (stats.stereo && stats.correlation >= 0.15f && stats.lowSidePct <= 35.0f
            && stats.shortTerm > -50.0f)
            add ("mono-ok", T ("单声道兼容性良好", "Mono compatible"),
                 TF (T ("声道相关性 {c}，120 Hz 以下侧向能量仅 {s}%，折叠单声道不会明显抵消。",
                        "Correlation is {c} and side energy below 120 Hz is only {s}%, so mono "
                        "folding will not cancel noticeably."),
                     { { "c", twoSigned (stats.correlation) }, { "s", one (stats.lowSidePct) } }),
                 Severity::good);

        if (stats.integrated > -70.0f && stats.lra >= 3.0f && stats.lra <= 16.0f
            && stats.getPlr() >= 10.0f)
            add ("dyn-ok", T ("动态范围健康", "Dynamics healthy"),
                 TF (T ("响度范围 {lra} LU、峰响比 {plr} dB，瞬态与段落起伏保留良好。",
                        "Loudness range {lra} LU and peak-to-loudness {plr} dB - transients and "
                        "section dynamics are preserved."),
                     { { "lra", one (stats.lra) }, { "plr", one (stats.getPlr()) } }),
                 Severity::good);
    }

    if (signal && bandDevs.size() == (size_t) SpectrumAnalyzer::numBands)
    {
        float worst = 0.0f;

        for (int b = 0; b < SpectrumAnalyzer::numBands; ++b)
            worst = juce::jmax (worst, std::abs (bandDevs[(size_t) b]));

        if (worst < 4.5f)
            add ("tonal-ok", T ("频段平衡正常", "Tonal balance is even"),
                 T ("各频段相对参考的偏差都在 ±4.5 dB 内，未发现浑浊、刺耳或齿音过重。",
                    "Every band stays within ±4.5 dB of the reference - no mud, harshness or "
                    "sibilance issues."),
                 Severity::good);
    }

    if (! anyBad && signal)
        add ("summary", T ("混音质检通过", "Mix check passed"),
             T ("响度、动态、声场、频段平衡与失真检查均处于合理范围，可按 -1 dBTP 上限输出。",
                "Loudness, dynamics, stereo image, tonal balance and distortion checks are all in "
                "range - the mix is ready at a -1 dBTP ceiling."),
             Severity::good);

    apply (found, stats);
}

//==============================================================================
void Advisor::apply (const std::vector<Issue>& found, const StatsSnapshot& stats)
{
    std::map<juce::String, bool> present;

    for (const auto& f : found)
        present[f.id] = true;

    for (const auto& f : found)
    {
        auto& c = conditions[f.id];

        if (c.onTicks == 0)
            c.firstSeen = stats.elapsed;

        c.onTicks = juce::jmin (100000, c.onTicks + 1);
        c.offTicks = 0;
        c.issue = f;
        c.issue.detectedAt = c.firstSeen;
    }

    for (auto& kv : conditions)
    {
        if (present.find (kv.first) == present.end())
        {
            kv.second.onTicks = 0;
            ++kv.second.offTicks;
        }
    }

    visible.clear();

    for (auto& kv : conditions)
    {
        const auto& c = kv.second;
        const int required = c.issue.timed ? 1 : 8;

        if (c.onTicks >= required)
            visible.push_back (c.issue);
    }

    std::sort (visible.begin(), visible.end(), [] (const Issue& a, const Issue& b)
    {
        if (a.severity != b.severity)
            return (int) a.severity > (int) b.severity;

        return a.detectedAt < b.detectedAt;
    });

    if (visible.size() > 32)
        visible.resize (32);

    overall = Severity::good;

    for (const auto& i : visible)
        overall = (Severity) juce::jmax ((int) overall, (int) i.severity);

    for (auto it = conditions.begin(); it != conditions.end();)
    {
        if (it->second.onTicks == 0 && it->second.offTicks > 60)
            it = conditions.erase (it);
        else
            ++it;
    }
}
} // namespace ml





