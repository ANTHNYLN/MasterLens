# MasterLens

**母带响度检查 · 混音质量诊断 · 实时频谱**  
macOS 音频总线插件（VST3 / AU / 独立版），C++17 + JUCE 8。

![MasterLens 界面](docs/screenshot.png)

插件**不处理任何音频**（零延迟、零染色），只做测量与诊断，插在总线 / 母带链最后一级即可。

---

## 1. 功能

### 母带响度
| 指标 | 说明 |
| --- | --- |
| 综合响度 I | ITU-R BS.1770-4 K 加权 + 门限积分（400 ms 块、75 % 重叠、−70 LUFS 绝对门限、−10 LU 相对门限） |
| 瞬时 / 短时 M / S | 400 ms / 3 s 窗口，100 ms 更新 |
| 响度范围 LRA | EBU Tech 3342：3 s 短时、1 s 步进、−70 / −20 门限、10 / 95 百分位 |
| 峰响比 PLR | 真峰值 − 综合响度 |
| 真峰值 TP | 4× 过采样多相插值（96 tap、Blackman-Harris 窗），捕捉跨采样峰值 |
| 目标对齐 | 预设：流媒体 −14 / Apple Music −16 / YouTube −14 / 播客 −16 / 广播 EBU R128 −23 / 俱乐部 −8 LUFS |

### 混音质量诊断（参考常见混音质检标准）
- **失真**：采样级削波计数、跨采样过载区间计数、瞬态跳变（爆音 / 剪辑点）计数
- **声场**：声道相关性、立体声宽度（侧 / 中能量比）、120 Hz 以下侧向能量（低频单声道检查）
- **电平**：直流偏移（4 秒真均值）、动态过量压缩（LRA / PLR）
- **频段平衡**：10 段（超低 20–60 Hz … 极高频 16–20 kHz）相对参考的偏差，用于指出
  **浑浊（250–500 Hz 堆积）、刺耳（2–5 kHz 过硬）、齿音过重（5–9 kHz）、
  空气感不足（9–16 kHz）、超低频过重、低中频塌陷、整体偏亮 / 偏暗、频段空缺**
- 每条结论都带中文说明、数值与处理建议；事件类问题记录首次检出的时间戳
- 通过项同样列出（未检出削波、单声道兼容性良好、动态范围健康、频段平衡正常）

### 可视化
- 实时频谱：4096 点 FFT、**60 fps**、对数频率轴、峰值保持、逐像素最大值合成 + 轻度平滑
- 短时响度历史：最近 3 分钟，带目标区间与目标线
- M / S 电平条、相关性 / 宽度 / 低频单声道仪表、失真事件计数
- 一键导出文本分析报告（含全部测量值、频段偏差与诊断结论）

### 界面
明亮浅色卡片式布局，可自由缩放（1120×700 – 1800×1200），窗口尺寸随工程保存。

**中英双语**：右上角 `EN / 中文` 按钮一键切换，全部界面文案、诊断结论与导出报告同步切换；
语言选择保存在插件状态里，下次打开保持上次的选择。

| 中文 | English |
| --- | --- |
| ![中文界面](docs/screenshot.png) | ![English UI](docs/screenshot-en.png) |

---

## 2. 判定阈值

| 检查项 | 触发条件 | 严重度 |
| --- | --- | --- |
| 响度高于目标 | I > 目标 + 2 LU | 建议检查 |
| 响度低于目标 | I < 目标 − 2.5 LU | 可优化 |
| 真峰值过载 | TP > 上限 | 需要处理 |
| 贴近上限 | TP > 上限 − 0.1 dB | 可优化 |
| 动态过度压缩 | LRA < 3 LU | 建议检查 |
| 响度范围过大 | LRA > 16 LU | 可优化 |
| 瞬态被压扁 | PLR < 8 dB | 建议检查 |
| 相位冲突 | 相关性 < 0 | 需要处理 |
| 单声道兼容性偏低 | 相关性 < 0.15 | 建议检查 |
| 立体声过窄 | 宽度 < 15 % | 可优化 |
| 立体声过宽 | 宽度 > 150 % | 建议检查 |
| 低频非单声道 | 120 Hz 以下侧向 > 35 % | 建议检查 |
| 削波 / 跨采样过载 / 爆音 | 计数 > 0 | 需要处理 / 建议检查 / 可优化 |
| 直流偏移 | 4 s 均值 > −46 dBFS | 建议检查 |
| 浑浊 / 刺耳 / 齿音 | 对应频段偏差 > +4.5 dB | 建议检查 |
| 空气感不足 | 9–16 kHz 偏差 < −5 dB | 可优化 |
| 超低频过重 | 20–60 Hz 偏差 > +5.5 dB | 可优化 |
| 低中频塌陷 | 120–250 Hz 偏差 < −5 dB | 可优化 |
| 整体偏亮 / 偏暗 | 高频段均值 − 中低频段均值 > ±6 dB | 建议检查 |
| 频段空缺 | 任一频段偏差 < −8 dB | 可优化 |

频段偏差的计算：对长期平均功率谱按倍频程宽度归一化（粉噪参考），
再与 20 Hz–16 kHz 各段密度的均值比较，因此衡量的是**相对平衡**，
绝对值请结合素材类型判断。

---

## 3. 构建

依赖：macOS 11+、Xcode Command Line Tools、CMake ≥ 3.22。
JUCE 8.0.15 已放在 `.tools/JUCE`（不参与版本库提交时可自行 `git clone --depth 1 --branch 8.0.15`）。

```bash
cd /Users/orch/Documents/github/vst
.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake \
    -S . -B build -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" \
    -DCMAKE_OSX_ARCHITECTURES=arm64
.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake \
    --build build --config Release --target MasterLens -j 10
```

产物：
```
build/MasterLens_artefacts/Release/VST3/MasterLens.vst3
build/MasterLens_artefacts/Release/AU/MasterLens.component
build/MasterLens_artefacts/Release/Standalone/MasterLens.app
```

### 打包 DMG（通用二进制）

```bash
./scripts/make_dmg.sh
```

输出 `dist/MasterLens-1.0.0-macOS.dmg`，并同时复制一份到 **桌面**，内含 VST3、AU、独立版、安装 / 卸载脚本与说明。

---

## 4. 安装（DMG）

1. 打开 DMG，双击「安装 Install.command」（右键 → 打开，可绕过 Gatekeeper 提示）。
2. 脚本复制到：
   - `~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3`
   - `~/Library/Audio/Plug-Ins/Components/MasterLens.component`
   - `~/Applications/MasterLens.app`（独立版，便于不打开 DAW 时试听）
3. 重启 DAW → 音频效果 / Analyzer 分类 → MasterLens。

卸载：双击 DMG 中的「卸载 Uninstall.command」。

---

## 5. 验证

### DSP 自检

```bash
./build/MasterLensDspTest_artefacts/Release/MasterLensDspTest
```

覆盖：
- 997 Hz 单声道 −20 dBFS 正弦 → **−23.00 LUFS**（R128 标定点）
- 立体声双声道相同信号 → −20.00 LUFS（BS.1770 声道能量求和，+3 LU 属标准行为）
- 真峰值 / 采样峰值标定、稳态 LRA=0、相关性 ±1、反相宽度
- 静音门限：20 s 静音后综合响度不变、短时归零
- 直流偏移：+0.01 偏移 → −40.00 dBFS；55 Hz 低音 → −81 dBFS（无误报）
- 频谱标定：满幅 1 kHz 正弦 → 峰值 0 dBFS ±1.5 dB、频点误差 < 15 Hz
- CPU 基准：60 s 立体声 48 kHz 的处理耗时（见运行输出）

### 插件格式验证

```bash
auval -v aufx Maln OrcA
```

---

## 6. 工程结构

```
CMakeLists.txt              JUCE 插件 / 自检 / 截图工具三个目标
Source/
  PluginProcessor.*         参数（目标响度、真峰值上限、频谱平滑）、总线、状态
  PluginEditor.*            界面布局、60 fps 定时器、预设、报告导出
  Text.h                    UTF-8 字面量辅助（JUCE 的 const char* 构造按 ASCII 解析）
  dsp/
    AnalysisEngine.*        K 加权、门限直方图、LRA、真峰值、相关性、宽度、DC、爆音
    SpectrumAnalyzer.*      4096 点 FFT、平滑、峰值保持、长期平均谱与频段偏差
  ui/
    Theme.*                 调色板、字体、LookAndFeel
    Widgets.h               组件声明
    Panels.cpp              卡片、响度磁贴、M/S 条、声场仪表
    Displays.cpp            频谱、频段平衡、历史曲线、诊断列表
    Advisor.*               规则引擎（阈值 + 迟滞，避免闪烁）
Tests/DspTest.cpp           离线自检（16 项断言 + CPU 基准）
Tools/UiShot.cpp            离屏渲染截图工具（无屏幕录制权限时验证界面）
packaging/                  DMG 内的安装 / 卸载脚本与说明
scripts/make_dmg.sh         通用二进制构建 + 签名 + DMG 打包
```

---

## 7. 已知限制

- ad-hoc 签名，未做 Apple 公证：首次安装可能需要「右键 → 打开」或 `xattr -dr com.apple.quarantine`。
- 仅支持单声道 / 立体声总线（不做 5.1 / 7.1 声道权重）。
- 频谱为 4096 点 FFT，48 kHz 下分辨率 11.7 Hz：20–60 Hz 频段仅约 3 个 bin，超低频细微差异请结合参考曲目判断。
- 频段平衡基于长期平均谱，需要数秒素材才会稳定；静音时会保持上一状态。
- 通用二进制已构建（arm64 + x86_64），但只在 Apple Silicon 实机验证过。
- 显示器截图受 macOS 屏幕录制权限限制时，可用 `Tools/UiShot.cpp` 离屏渲染（`MasterLensUiShot` 目标）。

---

## 8. 许可

JUCE 8.0.15 采用 AGPLv3 / 商业双许可；本项目源码按 AGPLv3 使用 JUCE。


