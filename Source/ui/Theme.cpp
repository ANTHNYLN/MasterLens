#include "Theme.h"

#include <map>

namespace ml
{
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

//==============================================================================
LightLookAndFeel::LightLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::DocumentWindow::textColourId, theme::text);

    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextEditor::backgroundColourId, theme::card);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::border);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.25f));

    setColour (juce::TextButton::buttonColourId, theme::card);
    setColour (juce::TextButton::buttonOnColourId, theme::accentSoft);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::accent);

    setColour (juce::ComboBox::backgroundColourId, theme::card);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);

    setColour (juce::PopupMenu::backgroundColourId, theme::card);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::headerTextColourId, theme::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accentSoft);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::accent);

    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::thumbColourId, theme::accent);

    setColour (juce::ScrollBar::thumbColourId, theme::border);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
}

void LightLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
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
    const float trackH = 5.0f;

    auto track = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackH * 0.5f,
                                         bounds.getWidth(), trackH);

    g.setColour (theme::border);
    g.fillRoundedRectangle (track, trackH * 0.5f);

    const float pos = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    g.setColour (theme::accent);
    g.fillRoundedRectangle (track.withRight (pos), trackH * 0.5f);

    const float r = 7.0f;
    auto thumb = juce::Rectangle<float> (pos - r, bounds.getCentreY() - r, r * 2.0f, r * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillEllipse (thumb.translated (0.0f, 1.5f));
    g.setColour (theme::card);
    g.fillEllipse (thumb);
    g.setColour (theme::accent);
    g.drawEllipse (thumb.reduced (0.8f), 1.8f);
}

void LightLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 9.0f;

    auto base = button.getToggleState() ? theme::accentSoft : backgroundColour;

    if (isButtonDown)
        base = base.darker (0.07f);
    else if (isMouseOver)
        base = base.brighter (0.03f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (button.getToggleState() ? theme::accent.withAlpha (0.65f) : theme::border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void LightLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                     int buttonX, int buttonY, int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 9.0f);

    g.setColour (isButtonDown ? theme::accent.withAlpha (0.7f)
                              : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 9.0f, 1.0f);

    const float cx = bounds.getRight() - 16.0f;
    const float cy = bounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));
}

juce::Font LightLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return uiFont (13.0f); }
juce::Font LightLookAndFeel::getLabelFont (juce::Label&)        { return uiFont (12.0f); }
juce::Font LightLookAndFeel::getPopupMenuFont()                 { return uiFont (13.0f); }

void LightLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colours::transparentBlack);
    g.setColour (theme::card);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 10.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 10.0f, 1.0f);
}
} // namespace ml

