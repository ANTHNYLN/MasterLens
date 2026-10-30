#include "PluginEditor.h"

using ml::T;
#include "Presets.h"
#include "Version.h"

//==============================================================================
void MasterLensAudioProcessorEditor::updateTexts()
{
    const bool en = ml::Localisation::isEnglish();

    holdButton.setButtonText (frozen ? T ("恢复", "Resume") : T ("保持", "Hold"));
    clearButton.setButtonText (T ("清零统计", "Clear stats"));
    aboutButton.setButtonText (T ("关于", "About"));
    settingsButton.setButtonText (T ("设置", "Settings"));
    themeButton.setButtonText (ml::theme::dark ? T ("深色", "Dark") : T ("浅色", "Light"));
    languageButton.setButtonText (en ? juce::String::fromUTF8 ("中文") : juce::String ("EN"));
    exportButton.setButtonText (T ("导出报告", "Export"));

    targetCaption.setText (T ("目标响度 LUFS", "Target LUFS"), juce::dontSendNotification);
    ceilingCaption.setText (T ("真峰值上限 dBTP", "Ceiling dBTP"), juce::dontSendNotification);

    presetBox.setTooltip (T ("平台预设：同时设置目标响度与真峰值上限",
                             "Presets set both the loudness target and the true-peak ceiling"));

    holdButton.setTooltip (T ("保持 (Hold)：锁定当前读数，便于对比改动前后；再次点击恢复实时（按住 Space）",
                              "Hold: freezes the current readings for before / after comparison; "
                              "press again to resume (hold Space)"));
    clearButton.setTooltip (T ("清零统计：Integrated / LRA / 真峰值超限计数归零，不改变任何参数（R）",
                               "Clear stats: resets Integrated / LRA / true-peak overs. "
                               "Parameters are untouched (R)"));
    targetSlider.setTooltip (T ("可键入数值，方向键步进 0.1 LU，双击复位 −14.0",
                                "Type a value, use the arrow keys for 0.1 LU steps, "
                                "double-click to return to -14.0"));
    ceilingSlider.setTooltip (T ("可键入数值，方向键步进 0.1 dB，双击复位 −1.0",
                                 "Type a value, use the arrow keys for 0.1 dB steps, "
                                 "double-click to return to -1.0"));
    themeButton.setTooltip (T ("切换深色 / 浅色主题", "Switch dark / light theme"));
    languageButton.setTooltip (T ("切换界面语言", "Switch interface language"));
    aboutButton.setTooltip (T ("关于 MasterLens", "About MasterLens"));
    settingsButton.setTooltip (T ("设置：界面缩放 100–200%，字号与布局同步放大（⌘ + / ⌘ − 快速调整）",
                                  "Settings: interface scale 100-200% - type and layout zoom together "
                                  "(Cmd + / Cmd -)"));
    exportButton.setTooltip (T ("导出 TXT / JSON / CSV 检查报告（⌘E）",
                                "Export TXT / JSON / CSV QC report (Cmd+E)"));

    const int selected = juce::jlimit (0, ml::presets::customIndex, presetBox.getSelectedId() - 1);
    updatingPreset = true;
    presetBox.clear (juce::dontSendNotification);

    int lastKind = -1;

    for (int i = 0; i < ml::presets::count; ++i)
    {
        const auto& e = ml::presets::entries[i];

        if (e.kind != lastKind)
        {
            if (e.kind == 1)
                presetBox.addSectionHeading (T ("曲风参考", "Genre reference"));
            else if (e.kind == 2)
                presetBox.addSectionHeading (T ("手动设定", "Manual"));

            lastKind = e.kind;
        }

        presetBox.addItem (ml::U8 (en ? e.en : e.zh), i + 1);
    }

    presetBox.setSelectedId (selected + 1, juce::dontSendNotification);
    updatingPreset = false;

    // Guidance for the current choice: the LRA window this target normally implies.
    if (juce::isPositiveAndBelow (selected, ml::presets::customIndex))
    {
        const auto& e = ml::presets::entries[selected];

        juce::String tip (ml::U8 (en ? e.en : e.zh));
        tip << "  |  " << juce::String (e.target, 1) << " LUFS"
            << "  |  TP " << juce::String (e.ceiling, 1) << " dBTP";

        if (e.lraHi > 0.0f)
            tip << "  |  LRA " << juce::String (e.lraLo, 0) << "-" << juce::String (e.lraHi, 0) << " LU";

        presetBox.setTooltip (tip);
    }
    else
    {
        presetBox.setTooltip (T ("自定义目标：手动拖动目标响度与真峰值上限",
                                "Custom target: set loudness and true-peak ceiling by hand"));
    }
}

//==============================================================================
bool MasterLensAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*> (focused) != nullptr)
            return false;

    if (key == juce::KeyPress::spaceKey)
    {
        holdFromKeyboard = true;
        setHold (true, true);
        return true;
    }

    if (key.getModifiers().isCommandDown())
    {
        const auto scaleChar = key.getTextCharacter();

        if (scaleChar == '=' || scaleChar == '+' || scaleChar == '-')
        {
            const int step = juce::jlimit (0, (int) ml::ui::scaleSteps.size() - 1,
                                           ml::ui::stepIndexFor (ml::ui::scale)
                                               + (scaleChar == '-' ? -1 : 1));
            setUiScale (ml::ui::scaleSteps[(size_t) step], true);
            return true;
        }
    }

    if (key.getModifiers().isCommandDown() && (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E'))
    {
        exportReport();
        return true;
    }

    const auto c = key.getTextCharacter();

    if (c == 'r' || c == 'R')
    {
        resetAnalysis();
        return true;
    }

    if (c >= '1' && c <= (char) ('0' + ml::numPages))
    {
        setPage ((int) (c - '1'));
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (settingsPanel.isVisible())
            settingsPanel.setVisibleAndFocus (false);
        else if (aboutPanel.isVisible())
            aboutPanel.setVisibleAndFocus (false);
        else if (genrePanel.isVisible())
            genrePanel.setVisibleAndFocus (false);
        else
            setHold (false, false);

        return true;
    }

    return false;
}

bool MasterLensAudioProcessorEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return keyPressed (key);
}

bool MasterLensAudioProcessorEditor::keyStateChanged (bool)
{
    if (holdFromKeyboard && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        holdFromKeyboard = false;
        setHold (false, false);
        return true;
    }

    return false;
}

void MasterLensAudioProcessorEditor::setHold (bool shouldHold, bool fromKeyboard)
{
    frozen = shouldHold;
    holdFromKeyboard = fromKeyboard && shouldHold;

    if (holdButton.getToggleState() != frozen)
        holdButton.setToggleState (frozen, juce::dontSendNotification);

    updateTexts();
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::updateSnapshotChips()
{
    snapshotA.setState (snapshots[0].valid, viewingSnapshot == 0);
    snapshotB.setState (snapshots[1].valid, viewingSnapshot == 1);
}

void MasterLensAudioProcessorEditor::captureSnapshot (int slot)
{
    if (slot < 0 || slot > 1 || ! stats.hasSignal)
        return;

    snapshots[slot].valid = true;
    snapshots[slot].stats = stats;
    snapshots[slot].capturedAt = stats.elapsed;
    viewingSnapshot = slot;

    updateSnapshotChips();
    repaint();
}

void MasterLensAudioProcessorEditor::recallSnapshot (int slot)
{
    if (slot < 0 || slot > 1)
        return;

    if (! snapshots[slot].valid)
    {
        captureSnapshot (slot);
        return;
    }

    viewingSnapshot = viewingSnapshot == slot ? -1 : slot;
    updateSnapshotChips();
    repaint();
}

void MasterLensAudioProcessorEditor::clearSnapshot (int slot)
{
    if (slot < 0 || slot > 1)
        return;

    snapshots[slot] = ml::Snapshot {};

    if (viewingSnapshot == slot)
        viewingSnapshot = -1;

    updateSnapshotChips();
    repaint();
}

void MasterLensAudioProcessorEditor::captureSnapshotForSnapshot (int slot)
{
    captureSnapshot (slot);
}
