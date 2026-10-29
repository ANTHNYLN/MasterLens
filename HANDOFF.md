# MasterLens — 任务交接（Handoff）

> 新会话请先读本文件，再开始工作。本文件替代全部历史对话上下文。

## 0. 一句话现状

macOS 母带交付检查插件 **MasterLens 1.2.0** 已完成并在本机验证；仓库 `ANTHNYLN/MasterLens` 已发布 v1.2.0（macOS DMG + Windows x64/ARM64 安装包）。当前无阻塞性 bug，剩余是打磨与验证项（见 §7）。

## 1. 环境事实（本机）

| 项 | 值 |
|---|---|
| 平台 | macOS 27 / arm64（M3 MacBook Air），无完整 Xcode，仅 Command Line Tools |
| CMake | `/Users/orch/Documents/github/vst/.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake` |
| JUCE | `/Users/orch/Documents/github/vst/.tools/JUCE`（8.0.15，已被 .gitignore 排除） |
| GitHub CLI | `/Users/orch/Documents/github/vst/.tools/gh-bin/gh`（已登录 ANTHNYLN，凭据在钥匙串） |
| 仓库 | https://github.com/ANTHNYLN/MasterLens （remote 为 SSH：`git@github.com:ANTHNYLN/MasterLens.git`） |
| 工程目录 | `/Users/orch/Documents/github/vst` |
| 桌面产物 | `~/Desktop/MasterLens-1.2.0-{macOS.dmg, Windows-x64.exe, Windows-ARM64.exe}` |

## 2. 工程结构

```
CMakeLists.txt              juce_add_plugin(FORMATS AU VST3 Standalone) + MasterLensDspTest + MasterLensUiShot
Source/
  PluginProcessor.*         APVTS 参数（targetLufs / tpCeiling / smoothing）、总线、状态存取
  PluginEditor.*            顶栏 + ModuleRail + DetailArea + TimelineStrip + About；30fps 定时器
  Version.h                 kVersion/kDeveloper(KNGTNPT)/kOrganisation(BerryStudio)
  Text.h                    ml::U8()（JUCE String(const char*) 按 ASCII 解析，中文必须走它）
  Delivery.h                平台静态表 + evaluate()（增益/预测TP/三级状态）
  dsp/
    AnalysisEngine.*        K加权 I/M/S/LRA、4×真峰值、PSR、TP事件环、三段分频相关/侧向、DC、爆音
    SpectrumAnalyzer.*      8192 FFT、对数插值取值、长期平均谱、4段能量占比、谐振检测、参考谱比对
    ReferenceSpectrum.*     后台线程 Welch 平均（1–4 条参考轨），SpinLock 交出结果
  ui/
    Localisation.h          ml::T(zh,en) / TF() 占位符；语言存 apvts.state["language"]
    Theme.*                 调色板 + uiFont/monoFont（字号系数 1.22，带 static 缓存）
    Widgets.h               Card/LoudnessTiles/LevelBars/HistoryGraph/SpectrumDisplay/BandBalance/StereoMeters/IssueList/AboutPanel
    Modules.h/.cpp          ModuleRail、TimelineStrip、DetailArea（7 个模块页 + 页内绘制）
    Advisor.*               规则引擎（三级严重度 + 迟滞）
    Panels.cpp / Displays.cpp
Tests/DspTest.cpp           17 项断言 + CPU 基准（当前：ALL CHECKS PASSED，CPU 0.42%）
Tools/UiShot.cpp            离屏渲染截图：参数 "" | en | about | expand | page0..page6
.github/workflows/build.yml macOS + Windows(x64/ARM64) 矩阵，自动探测 VS 版本 + NSIS
packaging/                  Install.command / Uninstall.command / README-DMG.txt / installer.nsi
scripts/make_dmg.sh         通用二进制构建 + 签名 + DMG（并复制到桌面）
```

## 3. 常用命令（可直接复制）

```bash
cd /Users/orch/Documents/github/vst
CM=.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake

# 开发构建（arm64，快）
$CM -S . -B build -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" -DCMAKE_OSX_ARCHITECTURES=arm64
$CM --build build --config Release -j 10

# 单元/自检
./build/MasterLensDspTest_artefacts/Release/MasterLensDspTest

# 界面离屏截图（会自动生成 ui_shot_*.png）
./build/MasterLensUiShot_artefacts/Release/MasterLensUiShot.app/Contents/MacOS/MasterLensUiShot page2

# 通用二进制 + DMG（输出 dist/ 并复制到桌面）
./scripts/make_dmg.sh

# 装到本机并验证 AU
cp -R build-universal/MasterLens_artefacts/Release/AU/MasterLens.component ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar; sleep 3; auval -v aufx Maln OrcA

# 发布
git add -A && git commit -m "..." && git push
./.tools/gh-bin/gh release create vX.Y.Z dist/MasterLens-X.Y.Z-macOS.dmg --title "MasterLens X.Y.Z" --notes "..."
./.tools/gh-bin/gh workflow run build.yml        # 触发三平台 CI
./.tools/gh-bin/gh run list --limit 3
./.tools/gh-bin/gh run download <run-id> -D /tmp/artifacts
```

## 4. 不可违反的工程约束（用户宪法）

1. 音频线程零分配、零锁、零 IO；缓冲全部在 `prepareToPlay` 预分配。
2. 插件不修改音频、零延迟；仅测量。
3. 所有判断必须可追溯到 BS.1770 公式、硬编码静态阈值表或用户参考轨；**禁止联网、禁止加载数据库/模型**。
4. 纯 C++（JUCE）；Python 只用于离线测试。
5. V1 仅立体声（代码结构保留声道数抽象）。
6. 输出三级状态：信息 / 警告 / 硬性超限（硬性仅限 TP 超限与用户响度目标越界）。
7. 每条检查尽量带时间戳/区间。
8. 改代码前先确认不违反以上任一条，冲突时先说明并给合规替代。

## 5. 已实现能力（1.2）

- 响度：I / M / S / LRA / PLR / **PSR（含最低点时间戳）**、门限直方图。
- 真峰值：4× 过采样，**超限事件环**（时间 + 超出量，定长 64 无分配）。
- 频谱：8192 FFT、对数插值绘制（已修“方波”）、4 段能量占比、**谐振检测**（±1/6 oct 局部均值 +6 dB）、**参考轨导入**（Welch 平均、路径持久化）。
- 立体声：全频段相关性/宽度 + **三段分频**（<150 / 150–2k / >2k）相关性与侧向能量。
- 平台交付：6 个平台静态表，增益/预测 TP/三级状态。
- 时间线：统一标记条 + TXT/JSON 报告导出。
- 界面：左侧模块导航（状态灯 + 关键值）→ 右侧详情主区 → 底部时间线；字号 1.22×；中英双语；关于面板（KNGTNPT / BerryStudio）。

## 6. 已知实现细节/坑

- 中文字面量**必须**用 `ml::U8()` / `T()`，否则乱码（JUCE 的 `String(const char*)` 走 ASCII）。
- `uiFont/monoFont` 内部有 static 缓存，且**故意不析构**（放在堆上），否则退出/卸载时崩溃。
- 频谱绘制用 `analyzer->getMagnitudeAt(hz)` 插值，不要再逐像素取最大值。
- 每个详情页的 `DetailArea::setState` 按页做了重绘节流，新增页面时保持同样策略。
- 版本号改 4 处：`Source/Version.h`、`CMakeLists.txt`（两处）、`scripts/make_dmg.sh`、`scripts/publish_with_gh.sh`。

## 7. 待办 / 建议下一步（按优先级）

1. **Windows 实机验证**：两个 `.exe` 由 CI 产出且为合法 NSIS 包，但未在 Windows 上做过“安装→DAW 扫描→出声”验证。
2. **参考轨导入端到端验证**：代码路径已编译通过，未用真实音频试过（可只加一个离线用例：自比应归零、+6 dB 窄带应报 ≥5 dB）。
3. **自动化测试补强**：PSR、TP 事件、谐振、平台预测目前只有逻辑，没有断言；建议在 `Tests/DspTest.cpp` 里加合成信号用例。
4. **界面打磨**：频谱页页脚（4 段占比 / 谐振 / 参考状态）在小窗口下略拥挤；频段条在 60 px 高时偏短。
5. **i18n 覆盖**：新增的 DetailArea 页内文案已双语，但 `delivery` 表头列名等个别字符串建议再走查一遍。
6. **可访问性/HiDPI**：目前字号随窗口固定，未跟随系统“更大文字”辅助功能。

## 8. 新会话开局提示（直接粘贴）

> 请先读 `/Users/orch/Documents/github/vst/HANDOFF.md`，它就是本项目全部上下文。
> 严格遵守其中的工程约束（音频线程零分配零锁、不联网、判断只可追溯到 BS.1770/静态阈值/用户参考轨）。
> 现在开始做：<在这里写你的新需求>。
> 改完请：跑 `MasterLensDspTest`、必要时用 `MasterLensUiShot` 截图自查、`scripts/make_dmg.sh` 出包，并按 §3 的方式提交与发布。
