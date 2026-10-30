#pragma once

#include <juce_core/juce_core.h>

namespace ml::presets
{
/** Static delivery presets. Every number here is a documented target, nothing
    is fetched or learned. The last entry is the "custom" slot.
*/
struct Entry
{
    const char* zh;
    const char* en;
    float target;   // integrated LUFS
    float ceiling;  // true-peak ceiling, dBTP
    float lraLo;    // recommended LRA window, lower bound
    float lraHi;    // recommended LRA window, upper bound
    int   kind;     // 0 = platform / spec, 1 = genre reference, 2 = custom
};

inline constexpr Entry entries[]
{
    // ---- streaming / broadcast specs -------------------------------------
    { "流媒体通用  -14 LUFS", "Streaming  -14 LUFS",      -14.0f, -1.0f,  5.0f,  9.0f, 0 },
    { "Spotify  -14 LUFS",    "Spotify  -14 LUFS",        -14.0f, -1.0f,  5.0f,  9.0f, 0 },
    { "Apple Music  -16 LUFS","Apple Music  -16 LUFS",     -16.0f, -1.0f,  6.0f, 11.0f, 0 },
    { "YouTube  -14 LUFS",    "YouTube  -14 LUFS",        -14.0f, -1.0f,  5.0f,  9.0f, 0 },
    { "Tidal  -14 LUFS",      "Tidal  -14 LUFS",          -14.0f, -1.0f,  5.0f,  9.0f, 0 },
    { "播客  -16 LUFS",       "Podcast  -16 LUFS",        -16.0f, -1.0f,  3.0f,  7.0f, 0 },
    { "有声书  -19 LUFS",     "Audiobook  -19 LUFS",      -19.0f, -3.0f,  3.0f,  7.0f, 0 },
    { "广播 EBU R128  -23 LUFS", "Broadcast EBU R128  -23 LUFS", -23.0f, -1.0f, 4.0f, 9.0f, 0 },

    // ---- club / electronic -----------------------------------------------
    { "俱乐部 / 舞曲  -8 LUFS", "Club / dance  -8 LUFS",   -8.0f, -0.8f,  3.0f,  6.0f, 1 },
    { "EDM / 电子  -8 LUFS",   "EDM / electronic  -8 LUFS", -8.0f, -0.8f,  3.0f,  6.0f, 1 },
    { "浩室 / 科技舞曲  -8.5", "House / techno  -8.5",     -8.5f, -0.8f,  3.0f,  6.0f, 1 },
    { "鼓打贝斯  -7.5 LUFS",   "Drum & bass  -7.5 LUFS",   -7.5f, -0.8f,  2.0f,  5.0f, 1 },
    { "Dubstep  -7.5 LUFS",    "Dubstep  -7.5 LUFS",       -7.5f, -0.8f,  2.0f,  5.0f, 1 },
    { "合成器流行  -9 LUFS",   "Synth-pop  -9 LUFS",        -9.0f, -1.0f,  4.0f,  7.0f, 1 },
    { "Lo-fi / Chill  -14",    "Lo-fi / chill  -14 LUFS",  -14.0f, -1.0f,  6.0f, 10.0f, 1 },
    { "环境 / 氛围  -18 LUFS", "Ambient  -18 LUFS",        -18.0f, -1.0f, 10.0f, 18.0f, 1 },

    // ---- urban / pop ------------------------------------------------------
    { "流行  -9.5 LUFS",       "Pop  -9.5 LUFS",            -9.5f, -1.0f,  4.0f,  8.0f, 1 },
    { "嘻哈 / Trap  -9 LUFS",  "Hip-hop / trap  -9 LUFS",   -9.0f, -1.0f,  4.0f,  8.0f, 1 },
    { "R&B / Soul  -11 LUFS",  "R&B / soul  -11 LUFS",     -11.0f, -1.0f,  6.0f, 10.0f, 1 },
    { "K-Pop  -9 LUFS",        "K-Pop  -9 LUFS",            -9.0f, -1.0f,  4.0f,  7.0f, 1 },
    { "拉丁  -10 LUFS",        "Latin  -10 LUFS",          -10.0f, -1.0f,  5.0f,  9.0f, 1 },
    { "雷鬼  -12 LUFS",        "Reggae  -12 LUFS",         -12.0f, -1.0f,  7.0f, 11.0f, 1 },

    // ---- guitar bands -----------------------------------------------------
    { "摇滚  -10 LUFS",        "Rock  -10 LUFS",           -10.0f, -1.0f,  6.0f, 10.0f, 1 },
    { "现代摇滚  -9 LUFS",     "Modern rock  -9 LUFS",      -9.0f, -1.0f,  5.0f,  8.0f, 1 },
    { "金属  -9 LUFS",         "Metal  -9 LUFS",            -9.0f, -1.0f,  4.0f,  7.0f, 1 },
    { "朋克  -9.5 LUFS",       "Punk  -9.5 LUFS",           -9.5f, -1.0f,  4.0f,  7.0f, 1 },
    { "独立 / 另类  -11 LUFS", "Indie / alternative  -11 LUFS", -11.0f, -1.0f, 7.0f, 11.0f, 1 },
    { "布鲁斯  -14 LUFS",      "Blues  -14 LUFS",          -14.0f, -1.0f,  9.0f, 14.0f, 1 },
    { "乡村  -12 LUFS",        "Country  -12 LUFS",        -12.0f, -1.0f,  7.0f, 12.0f, 1 },
    { "民谣  -14 LUFS",        "Folk  -14 LUFS",           -14.0f, -1.0f,  8.0f, 14.0f, 1 },

    // ---- acoustic / orchestral / spoken ----------------------------------
    { "爵士  -16 LUFS",        "Jazz  -16 LUFS",           -16.0f, -1.0f, 10.0f, 16.0f, 1 },
    { "原声 / 器乐  -16 LUFS", "Acoustic / instrumental  -16 LUFS", -16.0f, -1.0f, 10.0f, 16.0f, 1 },
    { "古典  -20 LUFS",        "Classical  -20 LUFS",      -20.0f, -1.0f, 12.0f, 20.0f, 1 },
    { "影视配乐  -20 LUFS",    "Film score  -20 LUFS",     -20.0f, -1.0f, 12.0f, 20.0f, 1 },
    { "游戏音频  -16 LUFS",    "Game audio  -16 LUFS",     -16.0f, -1.0f,  8.0f, 14.0f, 1 },
    { "短视频  -14 LUFS",      "Short-form video  -14 LUFS", -14.0f, -1.0f, 3.0f,  6.0f, 1 },

    // ---- fallback --------------------------------------------------------
    { "自定义", "Custom",                                    0.0f,  0.0f,  0.0f,  0.0f, 2 }
};

inline constexpr int count = (int) (sizeof (entries) / sizeof (entries[0]));
inline constexpr int customIndex = count - 1;
} // namespace ml::presets
