#include "PluginEditor.h"

using ml::T;
#include "Version.h"
#include "ui/Logo.h"

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);
    g.addTransform (juce::AffineTransform::scale (uiScale));

    // ---- header -------------------------------------------------------------
    {
        auto content = headerArea.reduced (16, 12);
        auto logo = content.removeFromLeft (32).toFloat();

        ml::paintLogoMark (g, logo);

        content.removeFromLeft (12);

        const int textX = content.getX();
        const auto nameFont = ml::uiFont (15.0f, true);

        g.setColour (ml::theme::text);
        g.setFont (nameFont);
        g.drawText ("MasterLens", juce::Rectangle<int> (textX, content.getY(), 200, 22),
                    juce::Justification::centredLeft, false);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::uiFont (ml::tokens::aux));
        g.drawText ("v" + juce::String (ml::kVersion),
                    juce::Rectangle<int> (textX + 206, content.getY(), 56, 22),
                    juce::Justification::centredLeft, false);

        g.setColour (ml::theme::cyan);
        g.setFont (ml::uiFont (ml::tokens::unit, true));
        g.drawText ("by BerryStudio",
                    juce::Rectangle<int> (textX + 258, content.getY() + 1, 130, 20),
                    juce::Justification::centredLeft, false);

        g.setColour (ml::theme::textDim);
        g.setFont (ml::uiFont (ml::tokens::unit));
        g.drawText (T ("母带交付检查 · 不修改音频", "Mastering QC · never alters audio"),
                    textX, content.getY() + 24, 320, 14, juce::Justification::centredLeft);

        if (frozen)
        {
            const auto pill = juce::Rectangle<int> (textX + 330, content.getY() + 23, 56, 16);

            g.setColour (ml::theme::warn.withAlpha (0.16f));
            g.fillRoundedRectangle (pill.toFloat(), 8.0f);
            g.setColour (ml::theme::warn);
            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (T ("已保持", "Held"), pill, juce::Justification::centred);
        }

        g.setColour (ml::theme::border);
        g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) baseBounds().getWidth());
    }

    // ---- parameter bar ------------------------------------------------------
    {
        g.setColour (ml::theme::card);
        g.fillRect (paramArea);

        g.setColour (ml::theme::border);
        g.drawHorizontalLine (paramArea.getBottom() - 1, 0.0f, (float) baseBounds().getWidth());
    }

    // ---- footer -------------------------------------------------------------
    {
        g.setColour (ml::theme::border);
        g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) baseBounds().getWidth());

        auto f = footerArea.reduced (16, 6);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::monoFont (ml::tokens::unit));
        g.drawText (juce::String (processor.getSampleRateValue() / 1000.0, 1) + " kHz"
                    + ml::sep() + (stats.stereo ? T ("立体声", "stereo") : T ("单声道", "mono"))
                    + ml::sep() + juce::String ("4") + ml::times() + " TP",
                    f.getX(), f.getY(), 330, f.getHeight(), juce::Justification::centredLeft);

        if (footerStatusText.isNotEmpty())
        {
            auto pill = juce::Rectangle<int> (f.getCentreX() - 46, f.getY(), 92, f.getHeight());

            g.setColour (footerStatusColour.withAlpha (0.12f));
            g.fillRoundedRectangle (pill.toFloat(), 9.0f);

            g.setColour (footerStatusColour);
            g.fillEllipse ((float) pill.getX() + 12.0f, (float) pill.getCentreY() - 3.0f, 6.0f, 6.0f);

            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (footerStatusText, pill.withTrimmedLeft (24),
                        juce::Justification::centredLeft);
        }

        // Developer lockup, right-aligned before the A / B chips.
        if (snapshotA.getX() > f.getX() + 400)
        {
            auto brand = juce::Rectangle<int> (f.getX() + 340, f.getY(),
                                               snapshotA.getX() - f.getX() - 352, f.getHeight());

            ml::paintBerryWordmark (g, brand, ml::theme::textFaint, 9.0f,
                                    juce::Justification::right);
        }
    }
}
