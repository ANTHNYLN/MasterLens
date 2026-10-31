MasterLens 1.5.0 —— 双基准检查 · 专辑统一 · 交付门禁（macOS · VST3 / AU / 独立版）
by BerryStudio · 开发者 KNGTNPT
=====================================================================

【安装】
  1. 双击「安装 Install.command」。如系统提示“无法打开，因为来自身份不明的开发者”，
     请右键该文件 → 打开 → 仍要打开。
  2. 脚本会复制：
        ~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3
        ~/Library/Audio/Plug-Ins/Components/MasterLens.component
     独立版复制到 ~/Applications/MasterLens.app
  3. 重启 DAW，在厂商 BerryStudio 下搜索 MasterLens（AU 名称亦为 BerryStudio: MasterLens）。

【工作方式】
  默认零延迟、默认旁路，只测量不改变音频；只有点击“试听”风险时，输出才会
  临时替换为所选时间片段 / 频段的循环，并自动做响度匹配。按 Esc 或再次点击停止。

【1.5 的核心变化：不再用固定曲风响度当合格线】
  · 顶部下拉选择主基准：
      平台归一化后果 / 参考轨 / 专辑中位 / 手动参考
  · 平台模式直接显示归一化后的衰减、服务端限幅和被丢弃的响度；
    -4 LUFS 这类超高响度会明确显示“归一化后感知响度不变，动态余量已损失多少”。
  · 参考轨 / 专辑中位模式显示 LUFS、LRA、PLR 与基线的差；偏差只作为信息，
    不自动判不合格。频谱平衡也只对照参考轨或专辑中位，不再内置绝对目标曲线。
  · 真峰值上限仍用于编码风险与专辑门禁；它不再与曲风响度绑定。

【频谱 60 fps】
  频谱页分析路径按 60 Hz 更新（8192 FFT、分数倍频程平滑、峰值保持）；
  其他页面保持低负载刷新（约 30 Hz），HOLD 时停止 FFT。

【专辑统一 / 交付门禁】
  1. 在 DAW 中逐首播放，输入曲目名，点「采集本曲」。采集写入共享专辑文件，
     同一台机器的其他 MasterLens 实例会看到同一份专辑。
  2. 至少采集 2 首后生成专辑中位；表格并列 LUFS / LRA / PLR / TP / Δ中位 / 状态。
  3. 右侧按严重度排序整张专辑的风险：平台归一化、编码 / TP、信号缺陷、
     单声道折叠、与专辑中位的偏离；并显示新增 / 恶化 / 改善的版本 diff。
  4. 底部五个规则开关控制门禁：平台限幅、真峰值、信号缺陷、单声道、中位偏离。
     阈值与规则可通过导入/导出的专辑 JSON 自定义并团队共享。
  5. 「导出证明」输出 JSON + TXT：包含 schema/版本/修订号、每首快照、中位、
     规则、阻断/放行结论、风险排序、版本 diff 与 SHA-256 快照/证据哈希。

【问题联动试听】
  · 信号健康页六张卡片可直接点击试听：谐波 2–12k、混叠 12–20k、超低 20–60、
    全频瞬态、单声道 60–250（M 折叠）、掩蔽 200–800。
  · 专辑页右侧风险行可直接点击试听对应频段；试听窗口取最近 1.5 s，
    若风险时间仍在 12 s 历史内则使用该时间点。
  · 试听采用 5 ms 淡入淡出并做峰值保护；默认旁路时输出逐位不变。

【DAW 标记与自动化】
  · 时间线页可「导出标记 CSV / 导入标记 CSV」，与 DAW 的标记表互相转换
    （timecode / seconds / severity / text）。VST3/AU 没有跨宿主标记写回 API，
    因此本版提供 CSV 交换，而不是声称直接写回每家 DAW 的标记轨。
  · 宿主自动化参数：Auto reset on transport start、Auto capture album on stop、
    Auto export proof on stop、Automation command
    （None / Reset / Capture track / Export proof / Export markers / Clear album）。
    重置在音频线程直接生效；采集 / 证明 / 标记需要编辑器打开以访问频谱与文件。

【平台归一化（仅作后果模拟，不是曲风目标）】
  Spotify / Apple Music / YouTube / Amazon / Tidal / SoundCloud / Podcast / EBU R128
  的响度归一化、预测真峰值、服务端限幅与被丢弃响度；不同平台可能不同。

【导出报告】
  右上角「导出报告」输出 TXT / JSON / CSV；报告包含基准模式、平台后果、
  专辑门禁、风险与版本 diff。专辑证明在专辑页单独导出。

【性能参考】
  频谱页约 3.7% 单核（60 fps；1.4.3 为 20 fps / 1.7%）；
  响度页约 2.7%，专辑页约 1.4%，信号页约 2.2%。DSP 约 0.31%。
  数值随机器与系统调度波动；用 MasterLensUiShot bench pageN 复测。

【卸载】
  双击「卸载 Uninstall.command」。

【说明】
  · 通用二进制（Apple Silicon + Intel），最低 macOS 11。
  · 采用 ad-hoc 签名（未做 Apple 公证）。若 DAW 拒绝加载：
        xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3
  · 专辑文件：~/Library/Application Support/BerryStudio/MasterLens/album.json
    （可用 MASTERLENS_ALBUM_FILE 环境变量覆盖，便于测试 / 团队共享快照）。
