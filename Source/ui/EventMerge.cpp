#include "Modules.h"

#include <algorithm>
#include <utility>

namespace ml
{
std::vector<MergedMarkerEvent> mergeMarkerEvents (const std::vector<Marker>& markers,
                                                  double windowSeconds)
{
    std::vector<MergedMarkerEvent> out;

    if (markers.size() < 2)
        return out;

    auto sorted = markers;
    std::sort (sorted.begin(), sorted.end(),
               [] (const Marker& a, const Marker& b) { return a.time < b.time; });

    size_t i = 0;

    while (i < sorted.size())
    {
        size_t j = i;
        double groupEnd = sorted[i].time;

        while (j + 1 < sorted.size()
               && sorted[j + 1].time - groupEnd <= windowSeconds
               && sorted[j + 1].time - sorted[i].time <= 2.0)
        {
            ++j;
            groupEnd = sorted[j].time;
        }

        const int count = (int) (j - i + 1);

        if (count >= 2)
        {
            bool tp = false, loud = false, phase = false, spec = false, manual = false;
            int severity = 0;

            for (size_t k = i; k <= j; ++k)
            {
                const auto& m = sorted[k];
                severity = juce::jmax (severity, m.severity);

                if (m.category == 0) tp = true;
                else if (m.category == 1) loud = true;
                else if (m.category == 2) phase = true;
                else if (m.category == 3) spec = true;
                else manual = true;
            }

            juce::String cause;

            if (tp && (spec || loud))
                cause = T ("限幅链：真峰值与响度 / 频谱异常同时出现，根因通常在末级限幅或削波",
                           "Limiter chain: true-peak plus loudness/spectral events - the root cause is "
                           "usually the final limiter or clipper");
            else if (phase && (loud || tp))
                cause = T ("低频 / 相位联动：侧向能量与响度事件同源，检查低频单声道化",
                           "Low-end link: side energy and loudness events share a cause - check low-end mono");
            else if (spec && loud)
                cause = T ("频段与动态同时异常：饱和 / 限幅正在改变音色",
                           "Spectral and dynamics events together - saturation or limiting is changing the tone");
            else if (manual)
                cause = T ("手动标记与自动事件重合，可作为同一段落复查",
                           "Manual and automatic markers overlap - review them as one section");
            else
                cause = T ("同时间点出现多个异常", "Several anomalies at the same moment");

            MergedMarkerEvent e;
            e.start = sorted[i].time;
            e.end = groupEnd;
            e.severity = severity;
            e.count = count;
            e.text = fmtTime (e.start)
                   + (e.end > e.start + 0.05 ? " " + enDash() + " " + fmtTime (e.end) : juce::String())
                   + "  " + cause + "  (" + juce::String (count) + ")";
            out.push_back (std::move (e));
        }

        i = j + 1;
    }

    return out;
}
} // namespace ml
