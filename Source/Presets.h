#pragma once

#include <juce_core/juce_core.h>

namespace ml::presets
{
/** Static delivery presets. Every number here is a documented target, nothing
    is fetched or learned. The last entry is the "custom" slot.
*/
inline constexpr int count = 8;

inline const char* const namesZh[count]
{
    "流媒体通用  -14 LUFS", "Spotify  -14 LUFS", "Apple Music  -16 LUFS",
    "YouTube  -14 LUFS", "播客  -16 LUFS", "广播 EBU R128  -23 LUFS",
    "俱乐部 / 舞曲  -8 LUFS", "自定义"
};

inline const char* const namesEn[count]
{
    "Streaming  -14 LUFS", "Spotify  -14 LUFS", "Apple Music  -16 LUFS",
    "YouTube  -14 LUFS", "Podcast  -16 LUFS", "Broadcast EBU R128  -23 LUFS",
    "Club / dance  -8 LUFS", "Custom"
};

inline constexpr float targets[count]  { -14.0f, -14.0f, -16.0f, -14.0f, -16.0f, -23.0f, -8.0f, 0.0f };
inline constexpr float ceilings[count] { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -0.8f, 0.0f };
} // namespace ml::presets
