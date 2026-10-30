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
    bool  normalises;
};

/** Static, hand-written delivery targets. Nothing here is learned or fetched. */
inline const PlatformPreset& platform (int index)
{
    static const PlatformPreset table[] =
    {
        { "spotify",   "Spotify",        "Spotify",        -14.0f, -1.0f, true  },
        { "apple",     "Apple Music",    "Apple Music",    -16.0f, -1.0f, true  },
        { "youtube",   "YouTube",        "YouTube",        -14.0f, -1.0f, true  },
        { "tidal",     "Tidal",          "Tidal",          -14.0f, -1.0f, true  },
        { "amazon",    "Amazon Music",   "Amazon Music",   -14.0f, -1.0f, true  },
        { "soundcloud","SoundCloud",     "SoundCloud",     -14.0f, -1.0f, true  },
        { "podcast",   "播客 / 广播",     "Podcast / R128", -16.0f, -1.0f, true  },
        { "club",      "俱乐部 / 舞曲",   "Club / dance",    -8.0f, -0.8f, false }
    };

    return table[juce::jlimit (0, 7, index)];
}

inline constexpr int numPlatforms = 8;

struct DeliveryResult
{
    float gainDb = 0.0f;             // gain the service will apply
    float predictedTpRaw = 0.0f;     // true peak after that gain, before its limiter
    float predictedTp = 0.0f;        // true peak after the service limiter
    float limitedDb = 0.0f;          // how much peak reduction the limiter has to do
    float wastedLoudnessDb = 0.0f;   // loudness bought that normalisation throws away
    DeliveryState state = DeliveryState::ok;
};

/** Deterministic consequence model: normalise to the target, then check the peak.
    A positive gain that would push the peak past the ceiling means the service has
    to limit, which costs transients; a negative gain measures how much limiting was
    spent for loudness nobody will hear. */
inline DeliveryResult evaluate (const StatsSnapshot& s, const PlatformPreset& p)
{
    DeliveryResult r;

    if (s.integrated <= -70.0f || s.truePeakDb <= -70.0f)
        return r;

    if (! p.normalises)
    {
        r.predictedTpRaw = s.truePeakDb;
        r.predictedTp = s.truePeakDb;

        r.state = r.predictedTp > p.tpCeiling
                    ? DeliveryState::over
                    : (r.predictedTp > p.tpCeiling - 0.5f ? DeliveryState::warning
                                                          : DeliveryState::ok);
        return r;
    }

    r.gainDb = p.targetLufs - s.integrated;
    r.predictedTpRaw = s.truePeakDb + r.gainDb;

    if (r.predictedTpRaw > p.tpCeiling)
    {
        r.limitedDb = r.predictedTpRaw - p.tpCeiling;
        r.predictedTp = p.tpCeiling;
    }
    else
    {
        r.predictedTp = r.predictedTpRaw;
    }

    r.wastedLoudnessDb = juce::jmax (0.0f, s.integrated - p.targetLufs);

    r.state = r.limitedDb > 0.05f ? DeliveryState::over
            : (r.predictedTp > p.tpCeiling - 0.5f ? DeliveryState::warning
                                                  : DeliveryState::ok);
    return r;
}
} // namespace ml
