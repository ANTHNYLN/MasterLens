#include "PluginEditor.h"

using ml::T;

//==============================================================================
void MasterLensAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();

    headerArea = b.removeFromTop (56);
    paramArea = b.removeFromTop (72);
    footerArea = b.removeFromBottom (40);

    // ---- header widgets -----------------------------------------------------
    {
        auto h = headerArea.reduced (16, 12);

        languageButton.setBounds (h.removeFromRight (46).withSizeKeepingCentre (46, 28));
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

    aboutPanel.setBounds (getLocalBounds());
    genrePanel.setBounds (getLocalBounds());

    processor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    processor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
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

    updateTexts();
    repaint();
}

void MasterLensAudioProcessorEditor::setThemeForSnapshot (bool useDark)
{
    if (useDark == ml::theme::dark)
        return;

    toggleTheme();
}
