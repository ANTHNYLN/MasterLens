#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto targetId = "targetLufs";
constexpr auto ceilingId = "tpCeiling";
constexpr auto smoothingId = "smoothing";
constexpr auto autoResetId = "autoResetOnStart";
constexpr auto autoScanId = "autoScanOnStop";
constexpr auto autoReportId = "autoReportOnStop";
constexpr auto commandId = "autoCommand";
} // namespace

//==============================================================================
MasterLensAudioProcessor::MasterLensAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    spectrumRing.prepare (32768);
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterLensAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { targetId, 1 }, "Target Loudness",
        juce::NormalisableRange<float> (-36.0f, 0.0f, 0.1f), -14.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ceilingId, 1 }, "True Peak Ceiling",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { smoothingId, 1 }, "Spectrum Smoothing",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.55f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoResetId, 1 }, "Auto reset on transport start", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoScanId, 1 }, "Auto capture album on stop", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { autoReportId, 1 }, "Auto export proof on stop", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { commandId, 1 }, "Automation command",
        juce::StringArray { "None", "Reset", "Capture track", "Export proof", "Export markers", "Clear album" },
        0));

    return { params.begin(), params.end() };
}

//==============================================================================
void MasterLensAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    engine.prepare (currentSampleRate, juce::jmax (1, getTotalNumInputChannels()));
    auditionEngine.prepare (currentSampleRate, 2);
    ml::resetStats (stats);
    spectrumRing.reset();

    resetRequested.store (false);
}

bool MasterLensAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo();
}

void MasterLensAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Host automation: reset at transport start, queue capture / proof on stop.
    bool playing = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();

    if (lastTransportPlaying < 0)
        lastTransportPlaying = playing ? 1 : 0;

    if (playing && lastTransportPlaying == 0 && getBool (autoResetId))
        resetRequested.store (true);

    if (! playing && lastTransportPlaying == 1)
    {
        if (getBool (autoScanId))
            pendingUiCommand.store (2, std::memory_order_release);

        if (getBool (autoReportId))
            pendingUiCommand.store (3, std::memory_order_release);
    }

    lastTransportPlaying = playing ? 1 : 0;

    if (auto* p = apvts.getRawParameterValue (commandId))
    {
        const int command = (int) p->load();

        if (command > 0 && command != lastCommandValue)
        {
            if (command == 1)
                resetRequested.store (true);
            else
                pendingUiCommand.store (command, std::memory_order_release);
        }

        lastCommandValue = command;
    }

    if (resetRequested.exchange (false))
    {
        engine.reset();
        ml::resetStats (stats);
        spectrumRing.reset();
        auditionEngine.reset();
    }

    // mono sum for the visualiser
    if (numChannels >= 2)
    {
        spectrumRing.writeStereoBlock (buffer.getReadPointer (0),
                                       buffer.getReadPointer (1), numSamples);
    }
    else
    {
        spectrumRing.writeBlock (buffer.getReadPointer (0), numSamples);
    }

    engine.process (buffer, stats, numSamples);
    auditionEngine.process (buffer);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================
float MasterLensAudioProcessor::getTargetLufs() const
{
    if (auto* p = apvts.getRawParameterValue (targetId))
        return p->load();

    return -14.0f;
}

float MasterLensAudioProcessor::getCeilingDb() const
{
    if (auto* p = apvts.getRawParameterValue (ceilingId))
        return p->load();

    return -1.0f;
}

float MasterLensAudioProcessor::getSmoothing() const
{
    if (auto* p = apvts.getRawParameterValue (smoothingId))
        return p->load();

    return 0.55f;
}

//==============================================================================
juce::AudioProcessorEditor* MasterLensAudioProcessor::createEditor()
{
    return new MasterLensAudioProcessorEditor (*this);
}

void MasterLensAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MasterLensAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterLensAudioProcessor();
}
