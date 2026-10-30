#include "Widgets.h"
#include "../Version.h"

namespace ml
{
//==============================================================================
SnapshotChip::SnapshotChip (juce::String letterText)
    : letter (std::move (letterText))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (true);
}

void SnapshotChip::setState (bool hasData, bool isActive)
{
    if (filled == hasData && active == isActive)
        return;

    filled = hasData;
    active = isActive;
    repaint();
}

void SnapshotChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);

    const auto background = active ? theme::accentSoft
                                   : (isMouseOver() ? theme::cardAlt.brighter (0.06f) : theme::cardAlt);

    g.setColour (background);
    g.fillRoundedRectangle (r, tokens::radiusControl);

    g.setColour (active ? theme::accent.withAlpha (0.75f) : theme::border);
    g.drawRoundedRectangle (r, tokens::radiusControl, 1.0f);

    g.setColour (active ? theme::accent : (filled ? theme::text : theme::textDim));
    g.setFont (monoFont (12.0f, true));
    g.drawText (letter, getLocalBounds().withTrimmedRight (10), juce::Justification::centred);

    if (filled)
    {
        g.setColour (active ? theme::accent : theme::good);
        g.fillEllipse (r.getRight() - 10.0f, r.getY() + 6.0f, 4.0f, 4.0f);
    }

    if (hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (r.reduced (2.0f), tokens::radiusControl - 1.0f, 2.0f);
    }
}

void SnapshotChip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        if (onClear != nullptr && filled)
            onClear();

        return;
    }

    if (onClick != nullptr)
        onClick();
}

juce::String SnapshotChip::getTooltip()
{
    if (filled)
        return T ("点击切回实时读数；右键清除该快照",
                  "Click to return to live readings; right-click to clear this snapshot");

    return T ("点击把当前读数保存到该快照，用于改动前后对比",
              "Click to store the current readings for before / after comparison");
}

//==============================================================================
static juce::Rectangle<int> aboutCardBounds (juce::Rectangle<int> area)
{
    const int w = juce::jmin (520, area.getWidth() - 64);
    const int h = juce::jmin (430, area.getHeight() - 64);

    return area.withSizeKeepingCentre (w, h);
}

AboutPanel::AboutPanel()
{
    setOpaque (false);

    closeButton.setButtonText (T ("关闭", "Close"));
    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
    addAndMakeVisible (closeButton);
}

void AboutPanel::setVisibleAndFocus (bool shouldBeVisible)
{
    setVisible (shouldBeVisible);

    if (shouldBeVisible)
    {
        toFront (true);
        closeButton.setButtonText (T ("关闭", "Close"));
        closeButton.grabKeyboardFocus();
    }
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (theme::dark ? 0.55f : 0.28f));

    const auto card = aboutCardBounds (getLocalBounds()).toFloat();

    g.setColour (theme::card);
    g.fillRoundedRectangle (card, tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (card.reduced (0.5f), tokens::radiusCard, 1.0f);

    auto inner = card.reduced (28.0f, 24.0f).toNearestInt();

    auto logo = inner.removeFromTop (48).removeFromLeft (48).toFloat();

    g.setGradientFill (juce::ColourGradient (theme::accent, logo.getX(), logo.getY(),
                                             theme::violet, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 12.0f);

    const float bars[] { 0.34f, 0.66f, 0.92f, 0.5f };
    float bx = logo.getX() + 9.0f;

    for (float bh : bars)
    {
        const float hgt = logo.getHeight() * 0.44f * bh + 4.0f;
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillRoundedRectangle (bx, logo.getBottom() - 10.0f - hgt, 4.5f, hgt, 2.0f);
        bx += 8.5f;
    }

    g.setColour (theme::text);
    g.setFont (uiFont (20.0f, true));
    g.drawText (kProductName, (int) logo.getRight() + 14, (int) logo.getY() - 4, 300, 28,
                juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (uiFont (tokens::aux));
    g.drawText (T ("母带响度与混音质检", "Mastering loudness & mix inspector"),
                (int) logo.getRight() + 14, (int) logo.getY() + 22, 320, 18,
                juce::Justification::centredLeft);

    inner.removeFromTop (16);

    g.setColour (theme::divider);
    g.drawHorizontalLine (inner.getY(), (float) inner.getX(), (float) inner.getRight());
    inner.removeFromTop (10);

    const auto drawRow = [&g, &inner] (const juce::String& label, const juce::String& value,
                                       juce::Colour colour)
    {
        auto r = inner.removeFromTop (26);
        g.setColour (theme::textFaint);
        g.setFont (uiFont (tokens::aux));
        g.drawText (label, r.removeFromLeft (128), juce::Justification::centredLeft);
        g.setColour (colour);
        g.setFont (uiFont (tokens::label, true));
        g.drawText (value, r, juce::Justification::centredLeft);
    };

    drawRow (T ("版本", "Version"), kVersion, theme::text);
    drawRow (T ("开发者", "Developer"), kDeveloper, theme::accent);
    drawRow (T ("机构", "Organisation"), kOrganisation, theme::violet);

    inner.removeFromTop (8);

    g.setColour (theme::textFaint);
    g.setFont (uiFont (tokens::aux));
    g.drawFittedText (T ("测量标准：ITU-R BS.1770-4 · EBU R128 · EBU Tech 3342\n"
                         "真峰值 4× 过采样插值 · 零延迟 · 不修改音频\n"
                         "Built with JUCE 8",
                         "Standards: ITU-R BS.1770-4 · EBU R128 · EBU Tech 3342\n"
                         "4x oversampled true peak · zero latency · never alters audio\n"
                         "Built with JUCE 8"),
                      inner, juce::Justification::topLeft, 4);
}

void AboutPanel::resized()
{
    const auto card = aboutCardBounds (getLocalBounds());
    closeButton.setBounds (card.getRight() - 124, card.getBottom() - 46, 96, 30);
}

void AboutPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! aboutCardBounds (getLocalBounds()).contains (e.getPosition()))
        if (onClose != nullptr)
            onClose();
}

} // namespace ml
