# MasterLens — 任务交接（Handoff）

> 新会话请先读本文件，再开始工作。本文件替代全部历史对话上下文。

## 0. 一句话现状

macOS 母带交付检查插件 **MasterLens 1.3.0**：在 1.2 的测量内核之上完成 UI 重构（深色专业风 + 浅色主题、令牌化设计系统、12 列栅格、A/B 快照、检查清单、导出 CSV），测量/DSP 逻辑未改动。1.2 的测量结论仍全部有效（DSP 自检 17 项 + 新增 8 项交付预测断言全部通过，CPU 0.36 %）。

## 1. 环境事实（本机）

| 项 | 值 |
|---|---|
| 平台 | macOS 27 / arm64（M3 MacBook Air），无完整 Xcode，仅 Command Line Tools |
| CMake | `/Users/orch/Documents/github/vst/.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake` |
| JUCE | `/Users/orch/Documents/github/vst/.tools/JUCE`（8.0.15，已被 .gitignore 排除） |
| GitHub CLI | `/Users/orch/Documents/github/vst/.tools/gh-bin/gh`（已登录 ANTHNYLN，凭据在钥匙串） |
| 仓库 | https://github.com/ANTHNYLN/MasterLens （remote 为 SSH：`git@github.com:ANTHNYLN/MasterLens.git`） |
| 工程目录 | `/Users/orch/Documents/github/vst` |
| 桌面产物 | `~/Desktop/MasterLens-1.3.0-{macOS.dmg, Windows-x64.exe, Windows-ARM64.exe}` |

## 2. 工程结构

```
CMakeLists.txt              juce_add_plugin(FORMATS AU VST3 Standalone) + MasterLensDspTest + MasterLensUiShot
Source/
  Version.h                 kVersion/kDeveloper(KNGTNPT)/kOrganisation(BerryStudio)
  Text.h                    ml::U8()（JUCE String(const char*) 按 ASCII 解析，中文必须走它）
  Presets.h                 8 个静态预设（名称 / 目标 / 上限）
  Delivery.h                平台静态表 + evaluate()（增益/预测TP/三级状态）
  PluginProcessor.*         APVTS 参数（targetLufs / tpCeiling / smoothing）、总线、状态存取
  PluginEditor.*            编辑器骨架（控件创建、窗口尺寸、快捷键挂载）
  EditorControls.cpp        控件搭建 + 信号回调
  EditorPaint.cpp           Header / 参数条 / 底栏绘制
  EditorLayout.cpp          resized()、主题切换
  EditorTimer.cpp           30 fps 轮询、标记合并、UiState 组装
  EditorRail.cpp            左导航状态与整体结论
  EditorTexts.cpp           文案 / tooltip / 快捷键 / 保持 / A-B 快照
  EditorPresets.cpp         预设、清零、参考轨、导出入口
  EditorReportTxt/Json/Csv  三种报告格式
  dsp/
    AnalysisEngine.*        K加权 I/M/S/LRA、4×真峰值、PSR、TP事件环、三段分频、DC、爆音
    SpectrumAnalyzer.*      8192 FFT、对数插值取值、长期平均谱、4段能量占比、谐振检测、参考谱比对
    ReferenceSpectrum.*     后台线程 Welch 平均（1–4 条参考轨），SpinLock 交出结果
  ui/
    Theme.h/.cpp            设计令牌 + 深/浅调色板 + 格式化（U+2212 / en dash）+ LookAndFeel
    Localisation.h          ml::T(zh,en) / TF() 占位符；语言存 apvts.state["language"]
    StatusText.h            四级状态文案与颜色
    Widgets.h               组件声明（MetricGrid / LevelMeters / HistoryGraph / SnapshotChip …）
    Panels.cpp              指标卡 + 空态
    Meters.cpp              M/S 横向电平
    Stereo.cpp              相关性 / 宽度 / 低频侧向 + 三段表 + 事件 chip
    Spectra.cpp             频谱与 10 段平衡绘制
    History.cpp             历史曲线（窗口切换 + hover）
    IssueList.cpp           可展开问题列表
    About.cpp               关于面板 + A/B 快照 chip
    Checklist.cpp           9 项固定阈值检查清单
    Rail.cpp                左导航
    Timeline.cpp            底部整曲时间线
    Modules.h               UiState / PageId / 组件声明
    Detail.cpp              页面骨架、空态切换、布局
    DetailPages.cpp         页面分发 + Header/Meta
    DetailPeak.cpp          真峰值页
    DetailMix.cpp           频谱 chip / 交付表
    DetailDiag.cpp          诊断页
    DetailTime.cpp          时间线页
    Advisor.*               规则引擎（三级严重度 + 迟滞 + 交付预测规则）
Tests/DspTest.cpp           25 项断言 + CPU 基准
Tools/UiShot.cpp            离屏渲染截图："" | en | about | expand | page0..page6 | small | light | empty | snap
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

# 界面离屏截图（会自动生成 ui_shot_*.png，并额外写一份到 /tmp/uishot_*.png）
./build/MasterLensUiShot_artefacts/Release/MasterLensUiShot.app/Contents/MacOS/MasterLensUiShot page2
# 可用参数：en | about | expand | page0..page6 | small | light | empty | snap

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

- 中文字面量**必须**用 `ml::U8()` / `T()`；`juce::String(const char*)` 按 Latin-1 解释。
- **非 ASCII 字面量不得出现在 `const char* + juce::String` 的左侧**：JUCE 的 `operator+ (const char*, const String&)` 会把右侧字符串的 UTF-8 字节当成单字符，得到 `Â·`。需要拼接时用 `juce::String("...") + ml::sep() + ...`。
- `uiFont/monoFont` 内部有 static 缓存，且**故意不析构**（放在堆上），否则退出/卸载时崩溃。
- `theme` 调色板是可变 inline 变量：切换主题只需 `theme::apply()` + `repaint()`，但**离屏缓存的组件**必须比较 `theme::revision()`（频谱/分段/历史已实现）。
- 每个详情页的 `DetailArea::setState` 按页做了重绘节流，新增页面时保持同样策略。
- 版本号改 5 处：`Source/Version.h`、`CMakeLists.txt`（两处）、`scripts/make_dmg.sh`、`scripts/publish_with_gh.sh`、`.github/workflows/build.yml`。
- **仓库位于 iCloud 同步的 `~/Documents`**：UiShot 写出的 PNG 可能被同步服务回滚成旧版本，造成“代码改了但截图没变”的假象。`Tools/UiShot.cpp` 因此同时写一份到 `/tmp/uishot_<name>.png`，自查请看 `/tmp` 副本。

## 7. 待办 / 建议下一步（按优先级）

1. **Windows 实机验证**：CI 产出的 `.exe` 尚未在 Windows 上做过“安装→DAW 扫描→出声”验证。
2. **参考轨导入端到端验证**：可加离线用例（自比归零、+6 dB 窄带应报 ≥5 dB）。
3. **测试补强**：PSR / TP 事件 / 谐振目前仍只有逻辑，没有断言；交付预测已有 8 项断言。
4. **界面打磨**：立体声页中间仍偏空，可加矢量示波器；频谱页小窗口下页脚略挤。
5. **i18n 走查**：新增页面文案已双语，个别导出/表格列名建议再核对。
6. **可访问性**：字号未跟随系统“更大文字”，只随窗口缩放。

## 8. 新会话开局提示（直接粘贴）

> 请先读 `/Users/orch/Documents/github/vst/HANDOFF.md`，它就是本项目全部上下文。
> 严格遵守其中的工程约束（音频线程零分配零锁、不联网、判断只可追溯到 BS.1770/静态阈值/用户参考轨）。
> 现在开始做：<在这里写你的新需求>。
> 改完请：跑 `MasterLensDspTest`、必要时用 `MasterLensUiShot` 截图自查、`scripts/make_dmg.sh` 出包，并按 §3 的方式提交与发布。
