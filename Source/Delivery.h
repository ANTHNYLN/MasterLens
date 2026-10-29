#pragma once

#include <juce_core/juce_core.h>
#include "dsp/AnalysisEngine.h"

namespace ml
{
enum class DeliveryState { ok = 0, warning = 1, over = 2 };

struct PlatformPreset
{
    const char* id;
    const char* nameZh;
    const char* nameEn;
    float targetLufs;
    float tpCeiling;
};

/** Static, hand-written delivery targets. Nothing here is learned or fetched. */
inline const PlatformPreset& platform (int index)
{
    static const PlatformPreset table[] =
    {
        { "spotify", "Spotify",        "Spotify",        -14.0f, -1.0f },
        { "apple",   "Apple Music",    "Apple Music",    -16.0f, -1.0f },
        { "youtube", "YouTube",        "YouTube",        -14.0f, -1.0f },
        { "tidal",   "Tidal",          "Tidal",          -14.0f, -1.0f },
        { "podcast", "播客 / 广播",     "Podcast / R128", -16.0f, -1.0f },
        { "club",    "俱乐部 / 舞曲",   "Club / dance",    -8.0f, -0.8f }
    };

    return table[juce::jlimit (0, 5, index)];
}

inline constexpr int numPlatforms = 6;

struct DeliveryResult
{
    float gainDb = 0.0f;
    float predictedTp = 0.0f;
    DeliveryState state = DeliveryState::ok;
};

/** Deterministic prediction: gain = target - integrated, then check TP. */
inline DeliveryResult evaluate (const StatsSnapshot& s, const PlatformPreset& p)
{
    DeliveryResult r;

    if (s.integrated <= -70.0f || s.truePeakDb <= -70.0f)
        return r;

    r.gainDb = p.targetLufs - s.integrated;
    r.predictedTp = s.truePeakDb + r.gainDb;

    if (r.predictedTp > p.tpCeiling)
        r.state = DeliveryState::over;
    else if (r.predictedTp > p.tpCeiling - 0.5f)
        r.state = DeliveryState::warning;
    else
        r.state = DeliveryState::ok;

    return r;
}
} // namespace ml
