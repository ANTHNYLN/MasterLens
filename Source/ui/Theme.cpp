#include "Theme.h"

#include <map>

namespace ml
{
//==============================================================================
void theme::apply (bool useDark)
{
    theme::dark = useDark;

    if (useDark)
    {
        theme::background = juce::Colour (0xff0b0e13);
        theme::card       = juce::Colour (0xff141922);
        theme::cardAlt    = juce::Colour (0xff1b222d);
        theme::raised     = juce::Colour (0xff1b222d);
        theme::border     = juce::Colour (0xff232b38);
        theme::borderSoft = juce::Colour (0xff1e2632);
        theme::divider    = juce::Colour (0xff1e2632);
        theme::text       = juce::Colour (0xffe6eaf2);
        theme::textDim    = juce::Colour (0xff9aa6b8);
        theme::textFaint  = juce::Colour (0xff7e8a9c);
        theme::accent     = juce::Colour (0xff4c8dff);
        theme::accentSoft = juce::Colour (0xff1e2c46);
        theme::cyan       = juce::Colour (0xff46c8d8);
        theme::good       = juce::Colour (0xff35c48a);
        theme::info       = juce::Colour (0xff4c8dff);
        theme::warn       = juce::Colour (0xfff5a623);
        theme::bad        = juce::Colour (0xffff5c5c);
        theme::violet     = juce::Colour (0xff8b7bff);
    }
    else
    {
        theme::background = juce::Colour (0xfff1f5fb);
        theme::card       = juce::Colour (0xffffffff);
        theme::cardAlt    = juce::Colour (0xfff7faff);
        theme::raised     = juce::Colour (0xffffffff);
        theme::border     = juce::Colour (0xffe2e9f3);
        theme::borderSoft = juce::Colour (0xffedf2f9);
        theme::divider    = juce::Colour (0xffedf2f9);
        theme::text       = juce::Colour (0xff16232f);
        theme::textDim    = juce::Colour (0xff55657a);
        theme::textFaint  = juce::Colour (0xff63728a);
        theme::accent     = juce::Colour (0xff2f7bf6);
        theme::accentSoft = juce::Colour (0xffe6efff);
        theme::cyan       = juce::Colour (0xff16b6c9);
        theme::good       = juce::Colour (0xff14835a);
        theme::info       = juce::Colour (0xff2f7bf6);
        theme::warn       = juce::Colour (0xffb46a10);
        theme::bad        = juce::Colour (0xffc9302a);
        theme::violet     = juce::Colour (0xff5f4fe0);
    }

    ++theme::revisionCounter;
}

//==============================================================================
juce::String typefaceName()
{
    static const juce::String name = []
    {
        const juce::StringArray preferred { "PingFang SC", "Hiragino Sans GB", "Heiti SC",
                                            "Songti SC", "Helvetica Neue" };
        const auto all = juce::Font::findAllTypefaceNames();

        for (const auto& p : preferred)
            if (all.contains (p))
                return p;

        return juce::Font::getDefaultSansSerifFontName();
    }();

    return name;
}

static const juce::StringArray& allTypefaceNames()
{
    static const juce::StringArray names = juce::Font::findAllTypefaceNames();
    return names;
}

static std::map<int, juce::Font>& fontCache (bool bold)
{
    // Deliberately leaked: destroying juce::Font objects after JUCE's own
    // static font machinery has been torn down crashes on exit / plug-in unload.
    static auto* caches = new std::map<int, juce::Font>[2];
    return caches[bold ? 1 : 0];
}

juce::Font uiFont (float height, bool bold)
{
    auto& cache = fontCache (bold);

    const int key = juce::roundToInt (height * 10.0f);
    const auto it = cache.find (key);

    if (it != cache.end())
        return it->second;

    juce::Font font (juce::FontOptions (typefaceName(), height * 1.22f,
                                        bold ? juce::Font::bold : juce::Font::plain)
                         .withFallbacks ({ "PingFang SC", "Hiragino Sans GB", "Arial Unicode MS" }));

    cache.emplace (key, font);
    return font;
}

juce::Font monoFont (float height, bool bold)
{
    auto& cache = fontCache (bold);

    const int key = juce::roundToInt (height * 10.0f) + 100000;
    const auto it = cache.find (key);

    if (it != cache.end())
        return it->second;

    static const juce::String name = []
    {
        const auto& all = allTypefaceNames();

        for (const auto* candidate : { "SF Mono", "Menlo", "Monaco" })
            if (all.contains (candidate))
                return juce::String (candidate);

        return juce::String ("Menlo");
    }();

    juce::Font font (juce::FontOptions (name, height * 1.1f,
                                        bold ? juce::Font::bold : juce::Font::plain)
                         .withFallbacks ({ "PingFang SC", "Menlo", "Arial Unicode MS" }));

    cache.emplace (key, font);
    return font;
}

juce::Colour severityColour (Severity s)
{
    switch (s)
    {
        case Severity::critical: return theme::bad;
        case Severity::warn:     return theme::warn;
        case Severity::info:     return theme::info;
        default:                 return theme::good;
    }
}
} // namespace ml


namespace ml
{
//==============================================================================
MasterLensLookAndFeel::MasterLensLookAndFeel()
{
    refreshColours();
}

void MasterLensLookAndFeel::refreshColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::DocumentWindow::textColourId, theme::text);

    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextEditor::backgroundColourId, theme::cardAlt);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::border);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.25f));

    setColour (juce::TextButton::buttonColourId, theme::cardAlt);
    setColour (juce::TextButton::buttonOnColourId, theme::accentSoft);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::accent);

    setColour (juce::ComboBox::backgroundColourId, theme::cardAlt);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);

    setColour (juce::PopupMenu::backgroundColourId, theme::card);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accentSoft);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::accent);

    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxBackgroundColourId, theme::cardAlt);
    setColour (juce::Slider::textBoxOutlineColourId, theme::border);
    setColour (juce::Slider::thumbColourId, theme::accent);

    setColour (juce::ScrollBar::thumbColourId, theme::border);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId, theme::cardAlt);
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::border);
}

void MasterLensLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float minSliderPos, float maxSliderPos,
                                              juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                          maxSliderPos, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    constexpr float trackH = 6.0f;

    auto track = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackH * 0.5f,
                                         bounds.getWidth(), trackH);

    g.setColour (theme::borderSoft);
    g.fillRoundedRectangle (track, 3.0f);

    const float pos = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    g.setColour (theme::accent);
    g.fillRoundedRectangle (track.withRight (pos), 3.0f);

    const float r = 7.0f;
    auto thumb = juce::Rectangle<float> (pos - r, bounds.getCentreY() - r, r * 2.0f, r * 2.0f);

    g.setColour (theme::cardAlt);
    g.fillEllipse (thumb);
    g.setColour (slider.hasKeyboardFocus (true) ? theme::text : theme::accent);
    g.drawEllipse (thumb.reduced (1.5f), 2.0f);
}

void MasterLensLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    constexpr float corner = tokens::radiusControl;

    auto base = button.getToggleState() ? theme::accentSoft : backgroundColour;

    if (isButtonDown)
        base = base.brighter (0.10f);
    else if (isMouseOver)
        base = base.brighter (0.06f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (button.getToggleState() ? theme::accent.withAlpha (0.70f) : theme::border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (2.0f), corner - 1.0f, 2.0f);
    }
}


void MasterLensLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                          int buttonX, int buttonY, int buttonW, int buttonH,
                                          juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, tokens::radiusControl);

    g.setColour (box.hasKeyboardFocus (true) ? theme::accent
                                             : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, tokens::radiusControl, 1.0f);

    const float cx = bounds.getRight() - 16.0f;
    const float cy = bounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));

    if (box.hasKeyboardFocus (true))
    {
        g.setColour (theme::accent);
        g.drawRoundedRectangle (bounds.reduced (2.0f), tokens::radiusControl - 1.0f, 2.0f);
    }
}

void MasterLensLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Sliders use their text box label as the numeric input: draw it as a real
    // 32 px control with a monospaced, right aligned reading.
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
    {
        auto r = label.getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (r, tokens::radiusControl);

        g.setColour (label.hasKeyboardFocus (true) ? theme::accent : theme::border);
        g.drawRoundedRectangle (r, tokens::radiusControl, 1.0f);

        if (label.isBeingEdited())
            return; // the TextEditor paints itself

        g.setColour (theme::text);
        g.setFont (monoFont (13.0f));
        g.drawText (label.getText(), label.getLocalBounds().reduced (8, 0),
                    juce::Justification::centredRight, false);
        return;
    }

    LookAndFeel_V4::drawLabel (g, label);
}

void MasterLensLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                         int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (theme::cardAlt);
    g.fillRoundedRectangle (r, tokens::radiusControl);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, tokens::radiusControl, 1.0f);

    g.setColour (theme::text);
    g.setFont (uiFont (tokens::aux * ui::scale));
    g.drawFittedText (text, 8, 0, width - 16, height, juce::Justification::centred,
                      juce::jmax (2, juce::roundToInt (3.0f * ui::scale)), 1.0f);
}

juce::Rectangle<int> MasterLensLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                              juce::Point<int> screenPos,
                                                              juce::Rectangle<int> parentArea)
{
    const auto font = uiFont (tokens::aux * ui::scale);
    const int maxW = juce::roundToInt (360.0f * ui::scale);
    const int pad = juce::roundToInt (16.0f * ui::scale);

    juce::AttributedString s;
    s.setJustification (juce::Justification::centred);
    s.append (tipText, font, theme::text);

    juce::TextLayout layout;
    layout.createLayout (s, (float) maxW);

    int w = juce::jlimit (80, maxW, juce::roundToInt (layout.getWidth()) + pad);
    int h = juce::jmax (juce::roundToInt (22.0f * ui::scale),
                        juce::roundToInt (layout.getHeight()) + juce::roundToInt (8.0f * ui::scale));

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 12
                                                                      : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - h - 6
                                                                      : screenPos.y + 18,
                                 w, h)
             .constrainedWithin (parentArea);
}

juce::Font MasterLensLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (tokens::label); }

juce::Font MasterLensLookAndFeel::getLabelFont (juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return monoFont (13.0f);

    return uiFont (tokens::aux);
}

/** Popup menus live in their own desktop window and are not affected by the
    editor transform, so their type is scaled here instead. */
juce::Font MasterLensLookAndFeel::getPopupMenuFont() { return uiFont (tokens::label * ui::scale); }
juce::Font MasterLensLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return uiFont (tokens::label); }

void MasterLensLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colours::transparentBlack);
    g.setColour (theme::card);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, tokens::radiusCard);
    g.setColour (theme::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f,
                            tokens::radiusCard, 1.0f);
}
} // namespace ml
