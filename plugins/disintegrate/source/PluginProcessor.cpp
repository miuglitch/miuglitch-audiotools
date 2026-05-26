#include "PluginProcessor.h"
#include "PluginEditor.h"

DisintegrateProcessor::DisintegrateProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",   juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

DisintegrateProcessor::~DisintegrateProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
DisintegrateProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Amount: 0.0 to 1.0 (displayed as 0-100%)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    // Min Drop Time: 1ms to 500ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "minDropMs", "Min Drop Time",
        juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f),
        1.0f));

    // Stereo Link: on/off
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "stereoLink", "Stereo Link",
        true));

    return { params.begin(), params.end() };
}

void DisintegrateProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    disintegrate.prepare(sampleRate, getTotalNumInputChannels());
}

void DisintegrateProcessor::releaseResources() {}

void DisintegrateProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    // Read parameters
    float amount     = apvts.getRawParameterValue("amount")->load();
    float minDropMs  = apvts.getRawParameterValue("minDropMs")->load();
    bool  stereoLink = apvts.getRawParameterValue("stereoLink")->load() > 0.5f;

    disintegrate.setAmount(amount);
    disintegrate.setMinDropMs(minDropMs);
    disintegrate.setStereoLink(stereoLink);

    // Get raw channel pointers and process
    float* channels[32];
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        channels[ch] = buffer.getWritePointer(ch);

    disintegrate.process(channels, buffer.getNumChannels(), buffer.getNumSamples());
}

void DisintegrateProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DisintegrateProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* DisintegrateProcessor::createEditor()
{
    return new DisintegrateEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DisintegrateProcessor();
}