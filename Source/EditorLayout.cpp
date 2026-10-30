#include "PluginEditor.h"

using ml::T;

namespace
{
    juce::Rectangle<int> screenWorkArea (juce::Component& c)
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();

        if (auto* d = displays.getDisplayForRect (c.getScreenBounds()))
            return d->userBounds.toNearestInt();

        if (auto* d = displays.getPrimaryDisplay())
            return d->userBounds.toNearestInt();

        return { 0, 0, 1440, 900 };
    }
} // namespace

//==============================================================================
juce::Rectangle<int> MasterLensAudioProcessorEditor::baseBounds() const
{
    const float s = uiScale > 0.01f ? uiScale : 1.0f;

    return { 0, 0,
             juce::jmax (1, juce::roundToInt ((float) getWidth() / s)),
             juce::jmax (1, juce::roundToInt ((float) getHeight() / s)) };
}

//==============================================================================
float MasterLensAudioProcessorEditor::computeInitialUiScale() const
{
    const auto stored = processor.apvts.state.getProperty ("uiScale");

    if (! stored.isVoid())
        return juce::jlimit (ml::ui::minScale, ml::ui::maxScale, (float) stored);

    // First launch: pick the largest step whose minimum layout still fits on the
    // current display. A large monitor therefore opens at 200 %, a laptop at the
    // biggest size that its screen can actually show.
    const auto area = screenWorkArea (const_cast<MasterLensAudioProcessorEditor&> (*this));
    const float availW = (float) area.getWidth() * 0.96f;
    const float availH = (float) area.getHeight() * 0.96f;

    float chosen = ml::ui::minScale;

    for (float step : ml::ui::scaleSteps)
        if (900.0f * step <= availW && 620.0f * step <= availH)
            chosen = step;

    processor.apvts.state.setProperty ("uiScale", chosen, nullptr);
    return chosen;
}

//==============================================================================
void MasterLensAudioProcessorEditor::resized()
{
    auto base = baseBounds();
    auto b = base;

    headerArea = b.removeFromTop (56);
    paramArea = b.removeFromTop (72);
    footerArea = b.removeFromBottom (40);

    // ---- header widgets -----------------------------------------------------
    {
        auto h = headerArea.reduced (16, 12);

        languageButton.setBounds (h.removeFromRight (46).withSizeKeepingCentre (46, 28));
        h.removeFromRight (8);
        settingsButton.setBounds (h.removeFromRight (64).withSizeKeepingCentre (64, 28));
        h.removeFromRight (8);
        aboutButton.setBounds (h.removeFromRight (58).withSizeKeepingCentre (58, 28));
        h.removeFromRight (8);
        themeButton.setBounds (h.removeFromRight (72).withSizeKeepingCentre (72, 28));
        h.removeFromRight (12);

        const int presetW = juce::jlimit (150, 240, h.getWidth() - 360);
        presetBox.setBounds (h.removeFromRight (presetW).withSizeKeepingCentre (presetW, 28));
        h.removeFromRight (6);
        infoButton.setBounds (h.removeFromRight (28).withSizeKeepingCentre (28, 28));
    }

    // ---- parameter bar: 12 column grid --------------------------------------
    {
        auto bar = paramArea.reduced (16, 10);
        auto actions = bar.removeFromRight (juce::jlimit (170, 196, bar.getWidth() / 4));

        holdButton.setBounds (actions.removeFromRight (78).withSizeKeepingCentre (78, 32));
        actions.removeFromRight (8);
        clearButton.setBounds (actions.removeFromRight (96).withSizeKeepingCentre (96, 32));

        bar.removeFromRight (16);

        const int colW = juce::jmax (40, bar.getWidth() / 12);

        auto left = bar.withWidth (colW * 5);
        auto right = bar.withX (bar.getX() + colW * 6).withWidth (colW * 5);

        targetCaption.setBounds (left.removeFromTop (16));
        targetSlider.setBounds (left.removeFromTop (32));
        ceilingCaption.setBounds (right.removeFromTop (16));
        ceilingSlider.setBounds (right.removeFromTop (32));
    }

    // ---- body ---------------------------------------------------------------
    auto body = b.reduced (16, 12);

    rail.setBounds (body.removeFromLeft (200));
    body.removeFromLeft (12);

    timeline.setBounds (body.removeFromBottom (44));
    body.removeFromBottom (8);
    detail.setBounds (body);

    // ---- footer widgets -----------------------------------------------------
    {
        auto f = footerArea.reduced (16, 6);

        exportButton.setBounds (f.removeFromRight (120).withSizeKeepingCentre (120, 28));
        f.removeFromRight (8);
        snapshotB.setBounds (f.removeFromRight (34).withSizeKeepingCentre (34, 28));
        f.removeFromRight (4);
        snapshotA.setBounds (f.removeFromRight (34).withSizeKeepingCentre (34, 28));
    }

    aboutPanel.setBounds (base);
    genrePanel.setBounds (base);
    settingsPanel.setBounds (base);

    // One transform for the whole surface: layout stays in base points, so a
    // 200 % zoom cannot clip a label.
    for (auto* child : getChildren())
        if (child != nullptr)
            child->setTransform (juce::AffineTransform::scale (uiScale));

    processor.apvts.state.setProperty ("editorWidth", base.getWidth(), nullptr);
    processor.apvts.state.setProperty ("editorHeight", base.getHeight(), nullptr);
}

//==============================================================================
void MasterLensAudioProcessorEditor::setUiScale (float newScale, bool store)
{
    newScale = juce::jlimit (ml::ui::minScale, ml::ui::maxScale, newScale);

    if (std::abs (newScale - uiScale) < 0.001f)
        return;

    const int baseW = juce::jlimit (900, 1900, juce::roundToInt ((float) getWidth() / uiScale));
    const int baseH = juce::jlimit (620, 1400, juce::roundToInt ((float) getHeight() / uiScale));

    uiScale = newScale;
    ml::ui::setScale (uiScale);

    if (store)
        processor.apvts.state.setProperty ("uiScale", uiScale, nullptr);

    setResizeLimits (ml::ui::scaled (900), ml::ui::scaled (620),
                     ml::ui::scaled (1900), ml::ui::scaled (1400));
    setSize (ml::ui::scaled (baseW), ml::ui::scaled (baseH));

    settingsPanel.setScale (uiScale);
    detail.invalidateDisplays();
    rail.repaint();
    timeline.repaint();
    repaint();
}

//==============================================================================
void MasterLensAudioProcessorEditor::toggleTheme()
{
    ml::theme::apply (! ml::theme::dark);
    processor.apvts.state.setProperty ("appearance", ml::theme::dark ? 1 : 0, nullptr);
    applyTheme();
}

void MasterLensAudioProcessorEditor::applyTheme()
{
    lnf.refreshColours();

    targetCaption.setColour (juce::Label::textColourId, ml::theme::textDim);
    ceilingCaption.setColour (juce::Label::textColourId, ml::theme::textDim);

    detail.invalidateDisplays();
    rail.repaint();
    timeline.repaint();
    settingsPanel.repaint();

    updateTexts();
    repaint();
}

void MasterLensAudioProcessorEditor::setThemeForSnapshot (bool useDark)
{
    if (useDark == ml::theme::dark)
        return;

    toggleTheme();
}
