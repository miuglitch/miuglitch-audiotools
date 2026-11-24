#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto gainParamId = "gain";
}

ParticulateAudioProcessor::ParticulateAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

ParticulateAudioProcessor::~ParticulateAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout ParticulateAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        gainParamId,
        "Gain",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([](float value, int) { return juce::String (value, 1) + " dB"; })));

    return layout;
}

const juce::String ParticulateAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ParticulateAudioProcessor::acceptsMidi() const { return false; }
bool ParticulateAudioProcessor::producesMidi() const { return false; }
bool ParticulateAudioProcessor::isMidiEffect() const { return false; }
double ParticulateAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int ParticulateAudioProcessor::getNumPrograms() { return 1; }
int ParticulateAudioProcessor::getCurrentProgram() { return 0; }
void ParticulateAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String ParticulateAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}
void ParticulateAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void ParticulateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    gainSmoother.prepare (sampleRate, 20.0);

    const auto gainDb = apvts.getRawParameterValue (gainParamId)->load();
    gainSmoother.reset (juce::Decibels::decibelsToGain (gainDb));
}

void ParticulateAudioProcessor::releaseResources() {}

bool ParticulateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void ParticulateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto gainDb = apvts.getRawParameterValue (gainParamId)->load();
    gainSmoother.setTarget (juce::Decibels::decibelsToGain (gainDb));

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = gainSmoother.getNextValue();

        for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)
            buffer.getWritePointer (channel)[sample] *= gain;
    }
}

bool ParticulateAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ParticulateAudioProcessor::createEditor()
{
    return new ParticulateAudioProcessorEditor (*this);
}

void ParticulateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void ParticulateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
        tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParticulateAudioProcessor();
}
