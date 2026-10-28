#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Advisor.h"

namespace ml::theme
{
inline const juce::Colour background { 0xfff1f5fb };
inline const juce::Colour card       { 0xffffffff };
inline const juce::Colour cardAlt    { 0xfff7faff };
inline const juce::Colour border     { 0xffe2e9f3 };
inline const juce::Colour borderSoft { 0xffedf2f9 };
inline const juce::Colour text       { 0xff16232f };
inline const juce::Colour textDim    { 0xff62748a };
inline const juce::Colour textFaint  { 0xff9bacbf };
inline const juce::Colour accent     { 0xff2f7bf6 };
inline const juce::Colour accentSoft { 0xffe6efff };
inline const juce::Colour cyan       { 0xff16b6c9 };
inline const juce::Colour good       { 0xff1f9d63 };
inline const juce::Colour info       { 0xff2f7bf6 };
inline const juce::Colour warn       { 0xffef8b2c };
inline const juce::Colour bad        { 0xffe0483e };
inline const juce::Colour violet     { 0xff7d6bf0 };
} // namespace ml::theme

namespace ml
{
juce::String typefaceName();
juce::Font uiFont (float height, bool bold = false);
juce::Font monoFont (float height, bool bold = false);
juce::Colour severityColour (Severity s);

inline juce::String fmt1 (float v) { return juce::String ((double) v, 1); }
inline juce::String fmtDb (float v) { return v <= -99.0f ? juce::String ("--") : juce::String ((double) v, 1); }

inline juce::String fmtTime (double seconds)
{
    const int total = juce::jmax (0, (int) std::floor (seconds));
    const int mins = total / 60;
    const int secs = total % 60;
    return juce::String (mins).paddedLeft ('0', 2) + ":" + juce::String (secs).paddedLeft ('0', 2);
}

/** Bright, flat look for the whole editor. */
class LightLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LightLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};
} // namespace ml
