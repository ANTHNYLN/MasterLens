#pragma once

#include "Presets.h"

namespace ml::presets::war
{
/** The "loudness war" context behind every target in presets::entries.

    `lufsLo` / `lufsHi` describe what released masters of that style actually
    measure before a streaming platform turns them down (observed ranges from
    public loudness databases and release analyses, rounded to whole LU). The
    one-line note explains how that style ended up there.

    This is a static table - nothing is fetched or learned at runtime. The rows
    are indexed exactly like presets::entries, so note[i] always belongs to
    entries[i].
*/
struct Note
{
    float lufsLo;   // released masters: quiet end
    float lufsHi;   // released masters: loud end
    const char* zh;
    const char* en;
};

inline constexpr Note notes[]
{
    // ---- platforms / specs -----------------------------------------------
    { -16.0f, -9.0f,
      "流媒体不会让歌变响，只会把过响的歌压下来：超过 -14 LUFS 的部分被等量衰减，响度战争在这里没有收益。",
      "Streaming does not make a master louder, it turns loud ones down: anything above -14 LUFS is attenuated equally." },
    { -16.0f, -9.0f,
      "Spotify 默认 -14 LUFS 归一化并提供 5 档响度偏好，-1 dBTP 是它建议的真峰值上限。",
      "Spotify normalises to -14 LUFS by default with five loudness presets and recommends -1 dBTP." },
    { -18.0f, -11.0f,
      "Apple Music 的 Sound Check 目标更保守（约 -16 LUFS），动态大的母带在这里损失最小。",
      "Apple Music's Sound Check target is more conservative (about -16 LUFS), so dynamic masters lose the least." },
    { -16.0f, -9.0f,
      "YouTube 按 -14 LUFS 归一化；视频里常见的 -8 LUFS 母带会被直接拉低约 6 dB。",
      "YouTube normalises to -14 LUFS; the -8 LUFS masters common in video get pulled down roughly 6 dB." },
    { -16.0f, -9.0f,
      "Tidal 使用与主流一致约 -14 LUFS 的归一化，真峰值按 -1 dBTP 计。",
      "Tidal uses the same roughly -14 LUFS normalisation, with a -1 dBTP ceiling." },
    { -19.0f, -14.0f,
      "播客生态（Apple / Spotify）以 -16 LUFS 为准，人声清晰度优先于峰值响度。",
      "The podcast ecosystem (Apple / Spotify) targets -16 LUFS, where intelligibility beats peak level." },
    { -23.0f, -18.0f,
      "ACX 一类有声书规范按 RMS 计量（约 -23 到 -18 dBFS），换算后接近 -19 LUFS，上限 -3 dBTP。",
      "Audiobook specs such as ACX measure RMS (about -23 to -18 dBFS), landing near -19 LUFS with -3 dBTP." },
    { -26.0f, -20.0f,
      "广播规范 EBU R128 用 -23 LUFS 取代峰值计量，是响度战争的制度性终点。",
      "EBU R128 replaced peak metering with -23 LUFS - the institutional end of the loudness war." },

    // ---- club / electronic ------------------------------------------------
    { -9.0f, -6.0f,
      "俱乐部母带为 PA 而非流媒体做：-6 到 -9 LUFS 很常见，LRA 被压到 3-6 LU，靠低频冲击取胜。",
      "Club masters are cut for a PA: -6 to -9 LUFS is normal, LRA squeezed to 3-6 LU, impact coming from low end." },
    { -9.0f, -6.0f,
      "节拍音乐是响度战争最激烈的战场，-6 LUFS 的发行作品不少，代价是镲片发糊、瞬态被限幅吃掉。",
      "Beat music is the loudest battlefield: plenty of releases sit at -6 LUFS, at the cost of smeared cymbals." },
    { -10.0f, -7.0f,
      "地下舞曲略保守于 EDM，-7 到 -10 LUFS，重点是稳定低频与不刺耳的高频。",
      "Underground dance sits slightly under EDM at -7 to -10 LUFS, prioritising steady low end." },
    { -9.0f, -6.0f,
      "DnB 的鼓组在 -6 到 -9 LUFS 时最具冲击力，代价是 LRA 常低到 2-5 LU。",
      "DnB hits hardest at -6 to -9 LUFS, usually with an LRA down at 2-5 LU." },
    { -8.0f, -5.0f,
      "Dubstep / bass 常见 -5 到 -8 LUFS，几乎完全由限幅与削波塑形，动态信息极少。",
      "Bass music often lands at -5 to -8 LUFS, shaped almost entirely by limiting and clipping." },
    { -11.0f, -8.0f,
      "80 年代合成器流行宽而中等响度；现代重制版通常再压 2-3 dB 换取「更现代」。",
      "80s synth-pop was wide and mid-loud; modern remasters squeeze another 2-3 dB to sound current." },
    { -16.0f, -12.0f,
      "Lo-fi 的魅力来自动态与噪声底，压到 -9 LUFS 反而失去风格，-12 到 -16 LUFS 更对味。",
      "Lo-fi lives on dynamics and a noisy floor; crushing it to -9 LUFS kills the style." },
    { -22.0f, -16.0f,
      "氛围音乐几乎不参与响度战争：-16 到 -22 LUFS、LRA 10-18 LU 才是常态。",
      "Ambient barely joins the war: -16 to -22 LUFS with 10-18 LU of range is the norm." },

    // ---- urban / pop ------------------------------------------------------
    { -11.0f, -8.0f,
      "主流流行长期在 -8 到 -11 LUFS，混音阶段就把鼓与贝斯压得很紧，母带只剩 1-2 dB 余量。",
      "Mainstream pop has long sat at -8 to -11 LUFS; drums and bass are tight before mastering even starts." },
    { -10.0f, -7.0f,
      "Trap 与 808 文化把低频响度推到极限，-7 到 -10 LUFS 常见，808 常被直接削成方波。",
      "Trap and 808 culture push low end to the limit: -7 to -10 LUFS is common and the 808 is often clipped square." },
    { -12.0f, -9.0f,
      "R&B 保留人声细节与呼吸，-9 到 -12 LUFS、LRA 6-10 LU 比较常见。",
      "R&B keeps vocal detail and breath: -9 to -12 LUFS with 6-10 LU of range is typical." },
    { -9.0f, -6.0f,
      "K-Pop 母带处在响度战争前线，-6 到 -9 LUFS，段落张力靠自动化而非动态对比。",
      "K-Pop masters are on the front line at -6 to -9 LUFS; tension comes from automation, not dynamics." },
    { -11.0f, -8.0f,
      "拉丁流行 / 雷击顿通常在 -8 到 -11 LUFS，人声与打击乐都要在响度上站住。",
      "Latin pop and reggaeton usually sit at -8 to -11 LUFS, where vocal and percussion both have to hold up." },
    { -13.0f, -10.0f,
      "雷鬼的贝斯线需要空间，-10 到 -13 LUFS、LRA 7-11 LU 更接近经典听感。",
      "Reggae needs room for the bass line: -10 to -13 LUFS with 7-11 LU of range sounds closer to the classics." },

    // ---- guitar bands -----------------------------------------------------
    { -11.0f, -8.0f,
      "90 年代 CD 时代是响度战争主场，经典摇滚专辑多在 -8 到 -11 LUFS；再压就会丢掉鼓的打击感。",
      "Guitar rock was the CD-era battlefield: classics sit at -8 to -11 LUFS, and more squeeze flattens the drum hit." },
    { -10.0f, -7.0f,
      "2000 年代后的现代摇滚普遍更响（-7 到 -10 LUFS），吉他墙靠饱和与限幅堆出来。",
      "Post-2000 modern rock runs hotter (-7 to -10 LUFS), the guitar wall built from saturation and limiting." },
    { -10.0f, -7.0f,
      "金属是响度战争最极端的分支之一，-7 到 -10 LUFS 常见；双踩瞬态靠削波保形。",
      "Metal is one of the most extreme branches at -7 to -10 LUFS; kick transients keep their shape through clipping." },
    { -11.0f, -8.0f,
      "朋克的粗粝感来自失真而非削波，-8 到 -11 LUFS 仍有冲击；压过头会变成一堵墙。",
      "Punk grit comes from distortion rather than clipping: -8 to -11 LUFS still hits, over-compression does not." },
    { -13.0f, -10.0f,
      "独立 / 另类对流媒体归一化更友好，-10 到 -13 LUFS，动态留得多一些。",
      "Indie and alternative are friendlier to normalisation: -10 to -13 LUFS with more dynamics intact." },
    { -16.0f, -12.0f,
      "布鲁斯以动态与演奏细节取胜，-12 到 -16 LUFS、LRA 9-14 LU 更受认可。",
      "Blues wins on dynamics and performance detail: -12 to -16 LUFS with 9-14 LU of range is the norm." },
    { -13.0f, -10.0f,
      "现代乡村在 -10 到 -13 LUFS，人声靠前但不靠限幅，弦乐与和声需要呼吸。",
      "Modern country sits at -10 to -13 LUFS: the vocal rides forward without heavy limiting." },
    { -16.0f, -12.0f,
      "民谣的亲密感来自动态与房间声，-12 到 -16 LUFS、LRA 8-14 LU 更像现场。",
      "Folk intimacy comes from dynamics and room: -12 to -16 LUFS with 8-14 LU of range." },

    // ---- acoustic / orchestral / spoken ----------------------------------
    { -18.0f, -14.0f,
      "爵士基本不参与响度战争，-14 到 -18 LUFS、LRA 10-16 LU，鼓刷与贝斯的动态都要留住。",
      "Jazz largely stays out of the war: -14 to -18 LUFS at 10-16 LU of range keeps brush and bass alive." },
    { -18.0f, -14.0f,
      "原声器乐靠瞬态与残响塑造空间，-14 到 -18 LUFS 是安全区间。",
      "Acoustic instruments build space from transients and decay: -14 to -18 LUFS is the safe zone." },
    { -24.0f, -18.0f,
      "古典录音几乎不受响度战争影响，-18 到 -24 LUFS、LRA 12-20 LU；压到 -14 会被视为严重失真。",
      "Classical barely feels the war: -18 to -24 LUFS at 12-20 LU of range; -14 LUFS would read as damaged." },
    { -24.0f, -18.0f,
      "配乐跟着对白走（对白约 -27 LKFS），母带通常比音乐发行物安静 6 dB 以上。",
      "Score follows dialogue (about -27 LKFS), typically 6 dB or more quieter than a music release." },
    { -18.0f, -14.0f,
      "游戏混音以 -16 LUFS 左右为基准，玩家自己调音量，留动态比抢响度重要。",
      "Game mixes sit near -16 LUFS; players set their own volume, so headroom beats max loudness." },
    { -16.0f, -12.0f,
      "短视频的响度战争最没意义：手机小喇叭分不出 6 dB，平台最后还是统一压到 -14 LUFS。",
      "The war is pointless in short-form video: phone speakers cannot show 6 dB and the platform flattens to -14 LUFS." },
    { 0.0f, 0.0f,
      "手动模式：自己设定目标响度、真峰值上限与 LRA 目标，适合有明确交付规范的商业项目。",
      "Manual mode: set your own loudness target, true-peak ceiling and LRA goal, for commercial specs." }
};

static_assert ((int) (sizeof (notes) / sizeof (notes[0])) == presets::count,
               "GenreWar::notes must have exactly one row per Presets::entries row");

/** Loudness target for a style and a "loudness-war participation" percentage.

    50 % is exactly the published genre reference (presets::entries::target);
    0 % moves to the quietest observed release, 100 % to the loudest observed
    release. Everything is static table data - no runtime learning.
*/
inline float warTargetLufs (int index, float percent) noexcept
{
    index = juce::jlimit (0, presets::customIndex - 1, index);
    percent = juce::jlimit (0.0f, 100.0f, percent);

    const auto& e = presets::entries[index];
    const auto& n = notes[index];

    if (e.kind == 2 || n.lufsHi <= n.lufsLo)
        return e.target;

    if (percent <= 50.0f)
        return n.lufsLo + (e.target - n.lufsLo) * (percent / 50.0f);

    return e.target + (n.lufsHi - e.target) * ((percent - 50.0f) / 50.0f);
}

/** Recommended LRA for the same participation: louder participation = less LRA. */
inline float warTargetLra (int index, float percent) noexcept
{
    index = juce::jlimit (0, presets::customIndex - 1, index);
    percent = juce::jlimit (0.0f, 100.0f, percent);

    const auto& e = presets::entries[index];
    return e.lraHi - (e.lraHi - e.lraLo) * (percent / 100.0f);
}
} // namespace ml::presets::war
