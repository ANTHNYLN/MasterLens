#include "Logo.h"

#include "Theme.h"

namespace ml
{
namespace
{
    float glyphWidth (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }

    void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                      juce::Rectangle<int> area, float tracking, juce::Colour colour,
                      juce::Justification justification)
    {
        float total = 0.0f;

        for (int i = 0; i < text.length(); ++i)
            total += glyphWidth (font, juce::String::charToString (text[i])) + tracking;

        total = juce::jmax (0.0f, total - tracking);

        const float start = justification.testFlags (juce::Justification::horizontallyCentred)
                                ? (float) area.getX() + ((float) area.getWidth() - total) * 0.5f
                                : (justification.testFlags (juce::Justification::right)
                                       ? (float) area.getRight() - total
                                       : (float) area.getX());

        float x = start;

        g.setColour (colour);
        g.setFont (font);

        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString (text[i]);
            const float w = glyphWidth (font, ch);

            g.drawText (ch, juce::Rectangle<float> (x, (float) area.getY(), w + 2.0f,
                                                    (float) area.getHeight()),
                        juce::Justification::centredLeft, false);

            x += w + tracking;
        }
    }
} // namespace

//==============================================================================
void paintLogoMark (juce::Graphics& g, juce::Rectangle<float> area, bool monochrome)
{
    if (area.getWidth() < 4.0f || area.getHeight() < 4.0f)
        return;

    const float s = juce::jmin (area.getWidth(), area.getHeight());
    area = area.withSizeKeepingCentre (s, s);

    const float corner = s * 0.255f;
    const auto centre = area.getCentre();

    if (monochrome)
    {
        g.setColour (theme::text);
        g.fillRoundedRectangle (area, corner);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (theme::accent.brighter (0.10f),
                                                 area.getX(), area.getY(),
                                                 theme::violet.darker (0.08f),
                                                 area.getRight(), area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);

        // Top-edge sheen keeps the mark from reading as a flat blue square.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f),
                                                 area.getX(), area.getY(),
                                                 juce::Colours::white.withAlpha (0.0f),
                                                 area.getX(), area.getCentreY(), false));
        g.fillRoundedRectangle (area.reduced (s * 0.02f), corner * 0.92f);
    }

    const auto ink = monochrome ? theme::background : juce::Colours::white;
    const float ringR = s * 0.305f;

    // Aperture ring: an arc with a deliberate opening at the lower right.
    {
        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f,
                            0.42f, juce::MathConstants<float>::twoPi - 0.68f, true);

        g.setColour (ink.withAlpha (0.94f));
        g.strokePath (ring, juce::PathStrokeType (s * 0.052f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Six iris blades, drawn as short radial strokes between ring and centre.
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::twoPi / 6.0f + 0.36f;
        const float r0 = ringR * 0.62f;
        const float r1 = ringR * 0.96f;

        juce::Path blade;
        blade.startNewSubPath (centre.x + std::cos (a) * r0, centre.y + std::sin (a) * r0);
        blade.lineTo (centre.x + std::cos (a) * r1, centre.y + std::sin (a) * r1);

        g.setColour (ink.withAlpha (i % 2 == 0 ? 0.34f : 0.22f));
        g.strokePath (blade, juce::PathStrokeType (s * 0.026f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // BerryStudio cluster: three overlapping berries with a stem and leaf, so the
    // mark reads as a berry bunch rather than a face at small sizes.
    const float br = s * 0.078f;
    const auto berry = [&] (float dx, float dy, float alpha)
    {
        g.setColour (ink.withAlpha (alpha));
        g.fillEllipse (centre.x + dx * s - br, centre.y + dy * s - br, br * 2.0f, br * 2.0f);
    };

    berry (0.0f, -0.062f, 0.98f);
    berry (-0.060f, 0.054f, 0.98f);
    berry (0.060f, 0.054f, 0.98f);

    if (! monochrome && s >= 64.0f)
    {
        auto stem = juce::Rectangle<float> (centre.x - s * 0.010f, centre.y - s * 0.196f,
                                            s * 0.020f, s * 0.070f);
        g.setColour (ink.withAlpha (0.92f));
        g.fillRoundedRectangle (stem, s * 0.010f);

        juce::Path leaf;
        leaf.addEllipse (centre.x + s * 0.020f, centre.y - s * 0.212f,
                         s * 0.105f, s * 0.042f);
        leaf.applyTransform (juce::AffineTransform::rotation (-0.58f,
                                                              centre.x + s * 0.072f,
                                                              centre.y - s * 0.191f));
        g.fillPath (leaf);
    }

    // Accent segment on the aperture: a small "measuring" highlight instead of
    // colouring a berry, which would read as a face at small sizes.
    if (! monochrome && s >= 40.0f)
    {
        juce::Path accent;
        accent.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, -0.92f, -0.18f, true);
        g.setColour (theme::cyan.withAlpha (0.95f));
        g.strokePath (accent, juce::PathStrokeType (s * 0.052f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }
}

void paintBerryWordmark (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                         float height, juce::Justification justification)
{
    const auto font = uiFont (height, true);
    drawTracked (g, "BERRYSTUDIO", font, area, height * 0.22f, colour, justification);
}

void paintBerryLockup (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                       float markSize, float textHeight)
{
    auto mark = area.removeFromLeft (juce::roundToInt (markSize)).toFloat()
                    .withSizeKeepingCentre (markSize, markSize);

    paintLogoMark (g, mark);

    area.removeFromLeft (juce::roundToInt (markSize * 0.28f));

    auto textArea = area;

    paintBerryWordmark (g, textArea.removeFromTop (juce::roundToInt (textHeight * 1.7f)),
                        colour, textHeight, juce::Justification::centredLeft);

    g.setColour (colour.withAlpha (0.62f));
    g.setFont (uiFont (textHeight * 0.66f));
    g.drawText ("MASTERING  INSTRUMENTS",
                textArea.removeFromTop (juce::roundToInt (textHeight * 1.1f)),
                juce::Justification::centredLeft, false);
}
} // namespace ml

