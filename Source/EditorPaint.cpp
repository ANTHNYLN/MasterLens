#include "PluginEditor.h"

using ml::T;
#include "Version.h"

//==============================================================================
void MasterLensAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ml::theme::background);

    // ---- header -------------------------------------------------------------
    {
        auto content = headerArea.reduced (16, 12);
        auto logo = content.removeFromLeft (32).toFloat();

        g.setGradientFill (juce::ColourGradient (ml::theme::accent, logo.getX(), logo.getY(),
                                                 ml::theme::violet, logo.getRight(), logo.getBottom(), false));
        g.fillRoundedRectangle (logo, 9.0f);

        const float barHeights[] { 0.30f, 0.62f, 0.90f, 0.48f };
        float barX = logo.getX() + 7.5f;

        for (float h : barHeights)
        {
            const float bh = logo.getHeight() * 0.46f * h + 3.0f;
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.fillRoundedRectangle (barX, logo.getBottom() - 7.0f - bh, 3.5f, bh, 1.75f);
            barX += 7.0f;
        }

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
                    juce::Rectangle<int> (textX + 206, content.getY(), 70, 22),
                    juce::Justification::centredLeft, false);

        g.setColour (ml::theme::textDim);
        g.setFont (ml::uiFont (ml::tokens::unit));
        g.drawText (T ("母带交付检查 · 不修改音频", "Mastering QC · never alters audio"),
                    textX, content.getY() + 24, 300, 14, juce::Justification::centredLeft);

        if (frozen)
        {
            const auto pill = juce::Rectangle<int> (textX + 244, content.getY() + 23, 56, 16);

            g.setColour (ml::theme::warn.withAlpha (0.16f));
            g.fillRoundedRectangle (pill.toFloat(), 8.0f);
            g.setColour (ml::theme::warn);
            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (T ("已保持", "Held"), pill, juce::Justification::centred);
        }

        g.setColour (ml::theme::border);
        g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) getWidth());
    }

    // ---- parameter bar ------------------------------------------------------
    {
        g.setColour (ml::theme::card);
        g.fillRect (paramArea);

        g.setColour (ml::theme::border);
        g.drawHorizontalLine (paramArea.getBottom() - 1, 0.0f, (float) getWidth());
    }

    // ---- footer -------------------------------------------------------------
    {
        g.setColour (ml::theme::border);
        g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) getWidth());

        auto f = footerArea.reduced (16, 6);

        g.setColour (ml::theme::textFaint);
        g.setFont (ml::monoFont (ml::tokens::unit));
        g.drawText (juce::String (processor.getSampleRateValue() / 1000.0, 1) + " kHz"
                    + ml::sep() + (stats.stereo ? T ("立体声", "stereo") : T ("单声道", "mono"))
                    + ml::sep() + juce::String ("4") + ml::times() + " TP",
                    f.getX(), f.getY(), 330, f.getHeight(), juce::Justification::centredLeft);

        if (footerStatusText.isNotEmpty())
        {
            auto pill = juce::Rectangle<int> (getWidth() / 2 - 46, f.getY(), 92, f.getHeight());

            g.setColour (footerStatusColour.withAlpha (0.12f));
            g.fillRoundedRectangle (pill.toFloat(), 9.0f);

            g.setColour (footerStatusColour);
            g.fillEllipse ((float) pill.getX() + 12.0f, (float) pill.getCentreY() - 3.0f, 6.0f, 6.0f);

            g.setFont (ml::uiFont (ml::tokens::unit));
            g.drawText (footerStatusText, pill.withTrimmedLeft (24),
                        juce::Justification::centredLeft);
        }
    }
}
