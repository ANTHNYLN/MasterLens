#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

/** Offscreen renderer used to verify the editor layout without a screen recorder.

    Feeds a synthetic music-like signal through the analysis engine, lets the
    60 fps editor timer run for a couple of seconds, then writes a PNG snapshot.
*/
class UiShotApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MasterLensUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        english = commandLine.containsIgnoreCase ("en");
        aboutShot = commandLine.containsIgnoreCase ("about");
        expandedShot = commandLine.containsIgnoreCase ("expand");

        processor = std::make_unique<MasterLensAudioProcessor>();
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processor->prepareToPlay (48000.0, 512);

        feedTestAudio (200.0);

        processor->apvts.state.setProperty ("language", english ? 1 : 0, nullptr);

        editor = std::make_unique<MasterLensAudioProcessorEditor> (*processor);
        editor->setSize (1180, 760);

        if (aboutShot)
            editor->setAboutVisible (true);

        juce::Timer::callAfterDelay (2500, [this] { saveAndQuit(); });
    }

    void shutdown() override
    {
        editor.reset();
        processor.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    void feedTestAudio (double seconds)
    {
        constexpr int blockSize = 512;
        constexpr double sr = 48000.0;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        juce::Random rng (20240607);

        const int totalBlocks = (int) (seconds * sr / blockSize);

        float pl0 = 0.0f, pl1 = 0.0f, pl2 = 0.0f;
        float lpL = 0.0f, lpR = 0.0f;
        double sampleIndex = 0.0;

        for (int b = 0; b < totalBlocks; ++b)
        {
            for (int n = 0; n < blockSize; ++n)
            {
                const double t = (sampleIndex + n) / sr;

                // kick every 0.5 s
                const double beat = std::fmod (t, 0.5);
                const float kickEnv = (float) std::exp (-beat * 18.0);
                const float kick = 0.34f * kickEnv * (float) std::sin (twoPi * 55.0 * beat);

                // bass + chord bed
                const float bass = 0.12f * (float) std::sin (twoPi * 110.0 * t);
                const float chord = 0.05f * (float) (std::sin (twoPi * 440.0 * t)
                                                    + std::sin (twoPi * 554.37 * t)
                                                    + std::sin (twoPi * 659.25 * t));

                // hats
                const double hatBeat = std::fmod (t, 0.25);
                const float hatEnv = (float) std::exp (-hatBeat * 45.0);
                const float hat = 0.05f * hatEnv * (rng.nextFloat() * 2.0f - 1.0f);

                // shared pink noise bed (mono low end) + high-passed side signal
                const float n0 = rng.nextFloat() * 2.0f - 1.0f;
                pl0 = 0.99765f * pl0 + n0 * 0.0990460f;
                pl1 = 0.96300f * pl1 + n0 * 0.2965164f;
                pl2 = 0.57000f * pl2 + n0 * 1.0526913f;
                const float pinkCommon = (pl0 + pl1 + pl2 + n0 * 0.1848f) * 0.050f;

                const float nl = rng.nextFloat() * 2.0f - 1.0f;
                const float nr = rng.nextFloat() * 2.0f - 1.0f;
                lpL += 0.020f * (nl - lpL);
                lpR += 0.020f * (nr - lpR);
                const float sideL = (nl - lpL) * 0.105f;
                const float sideR = (nr - lpR) * 0.105f;

                // 24 s musical form so the history graph shows real dynamics
                const double section = std::fmod (t, 24.0);
                const float sectionGain = section < 12.0 ? 0.62f : 1.0f;
                const float swell = 0.88f + 0.12f * (float) std::sin (twoPi * t / 7.0);

                const float centre = (kick + bass + chord * 0.85f + hat * 0.7f) * sectionGain * swell;

                buffer.setSample (0, n, juce::jlimit (-1.0f, 1.0f, centre + (pinkCommon + sideL) * sectionGain));
                buffer.setSample (1, n, juce::jlimit (-1.0f, 1.0f, centre + (pinkCommon + sideR) * sectionGain));
            }

            processor->processBlock (buffer, midi);
            sampleIndex += blockSize;
        }
    }

    void saveAndQuit()
    {
        if (expandedShot)
            editor->expandFirstIssueForSnapshot();

        std::vector<float> curve;
        processor->copyShortTermCurve (curve, 1800);

        int valid = 0;
        float minV = 0.0f, maxV = -200.0f;

        for (auto v : curve)
        {
            if (v > -99.0f)
            {
                ++valid;
                minV = juce::jmin (minV == 0.0f ? v : minV, v);
                maxV = juce::jmax (maxV, v);
            }
        }

        std::printf ("curve points=%d valid=%d min=%.2f max=%.2f last=%.2f\n",
                     (int) curve.size(), valid, minV, maxV,
                     curve.empty() ? 0.0f : curve.back());

        const auto s = processor->getStatsSnapshot();
        std::printf ("integrated=%.2f  tp=%.2f  lra=%.2f  corr=%.2f  width=%.1f  lowside=%.1f\n",
                     s.integrated, s.truePeakDb, s.lra, s.correlation, s.widthPct, s.lowSidePct);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

        const auto output = juce::File::getCurrentWorkingDirectory()
                                .getChildFile (aboutShot ? "ui_shot_about.png"
                                              : expandedShot ? "ui_shot_expanded.png"
                                              : (english ? "ui_shot_en.png" : "ui_shot.png"));

        juce::PNGImageFormat png;

        if (auto stream = std::unique_ptr<juce::FileOutputStream> (output.createOutputStream()))
            png.writeImageToStream (image, *stream);

        quit();
    }

    std::unique_ptr<MasterLensAudioProcessor> processor;
    std::unique_ptr<MasterLensAudioProcessorEditor> editor;
    bool english = false;
    bool aboutShot = false;
    bool expandedShot = false;
};

START_JUCE_APPLICATION (UiShotApplication)
