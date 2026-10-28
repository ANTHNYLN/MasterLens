#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto targetId = "targetLufs";
constexpr auto ceilingId = "tpCeiling";
constexpr auto smoothingId = "smoothing";
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

    return { params.begin(), params.end() };
}

//==============================================================================
void MasterLensAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    engine.prepare (currentSampleRate, juce::jmax (1, getTotalNumInputChannels()));
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

    if (resetRequested.exchange (false))
    {
        engine.reset();
        ml::resetStats (stats);
        spectrumRing.reset();
    }

    // mono sum for the visualiser
    if (numChannels >= 2)
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (1);

        for (int n = 0; n < numSamples; ++n)
            spectrumRing.write (0.5f * (l[n] + r[n]));
    }
    else
    {
        const float* l = buffer.getReadPointer (0);

        for (int n = 0; n < numSamples; ++n)
            spectrumRing.write (l[n]);
    }

    engine.process (buffer, stats, numSamples);

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
