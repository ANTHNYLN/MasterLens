MasterLens 1.4.2 —— 母带响度 · 平台归一化 · 信号健康（macOS · VST3 / AU / 独立版）
by BerryStudio · 开发者 KNGTNPT
=====================================================================

【安装】
  1. 双击「安装 Install.command」。
     如系统提示“无法打开，因为来自身份不明的开发者”，
     请右键该文件 → 打开 → 仍要打开。
  2. 脚本会把插件复制到：
        ~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3
        ~/Library/Audio/Plug-Ins/Components/MasterLens.component
     并把独立版复制到 ~/Applications/MasterLens.app
  3. 重启 DAW，在「音频效果 → Analyzer / 分析」分类中找到 MasterLens。

【在哪用】
  插在总线（Master Bus）或母带链的最后一级。插件不做任何音频处理，
  零延迟、零染色，只做测量。

【看什么】
  · 实时频谱：8192 点 FFT、分数倍频程平滑（低频 1/3 倍频程）、
    对数频率、平滑曲线与峰值保持。
  · 母带响度：综合响度 I（ITU-R BS.1770-4 门限积分）、M / S 瞬时值、
    LRA、PLR、真峰值（4 倍过采样插值）。
  · 平台归一化：8 个平台归一化后的实际后果——服务增益、预测真峰值、
    服务端必须做的限幅量、被丢弃的响度，并与导入的参考作品逐项对比。
  · 信号健康：谐波 / 互调失真、混叠、超低频堆积、瞬态完整性、
    单声道折叠损失、中频掩蔽，以及把同时越线的多层异常合并归因的联动层。
  · 混音质量诊断：削波 / 跨采样过载 / 爆音、相位与单声道兼容性、
    立体声宽度、低频单声道、直流偏移、10 段频段平衡，并给出处理建议。
  · 短时响度历史：最近 3 分钟，带目标区间。
  · 界面缩放：设置里可选 100% / 125% / 150% / 175% / 200%，字号与布局
    等比放大，不截断文字；⌘ + / ⌘ − 快速调整。
  · 导出报告：header 右上角「导出报告 / Export」，输出 TXT / JSON / CSV。
  · 关于：右上角「关于 / About」查看版本、开发者 KNGTNPT、BerryStudio。
  · 语言：右上角「EN / 中文」一键切换中英文界面与报告。

【常用目标】
  流媒体：-14 LUFS / -1 dBTP
  Apple Music：-16 LUFS / -1 dBTP
  播客：-16 LUFS / -1 dBTP
  广播 EBU R128：-23 LUFS / -1 dBTP
  俱乐部 / 舞曲：-8 LUFS / -0.8 dBTP（不做归一化，只检查峰值）

【卸载】
  双击「卸载 Uninstall.command」。

【说明】
  · 本构建为通用二进制（Apple Silicon + Intel），最低 macOS 11。
  · 1.4.2：界面缩放 100–200%（默认按屏幕自动选择最大可用档位）、
    全新 BerryStudio 品牌标识与图标、低频频谱平滑、8 平台归一化后果模拟、
    参考作品量化对比、6 层信号缺陷检测与联动归因。
  · 采用 ad-hoc 签名（未做 Apple 公证）。若 DAW 拒绝加载，可执行：
        xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3
