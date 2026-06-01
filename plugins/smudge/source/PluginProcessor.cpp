#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamID
{
    static const juce::String Amount   = "amount";
    static const juce::String Warble   = "warble";
    static const juce::String Rate     = "rate";
    static const juce::String Stereo   = "stereo";
    static const juce::String Flutter  = "stereo_flutter";
    static const juce::String HP       = "highpass";
    static const juce::String LP       = "lowpass";
    static const juce::String Stuffing = "stuffing";
}

juce::AudioProcessorValueTreeState::ParameterLayout
SmudgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [] (const juce::String& id, const juce::String& name,
                   float def, float stepSize)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            id, name,
            juce::NormalisableRange<float> (0.0f, 100.0f, stepSize),
            def,
            juce::AudioParameterFloatAttributes().withLabel ("%"));
    };

    params.push_back (pct (ParamID::Amount,   "Amount",         0.0f,   1.0f));
    params.push_back (pct (ParamID::Warble,   "Warble",         30.0f,  0.1f));
    params.push_back (pct (ParamID::Rate,     "Rate",           30.0f,  0.1f));
    params.push_back (pct (ParamID::Stereo,   "Stereo",         0.0f,   0.1f));
    params.push_back (pct (ParamID::Flutter,  "Stereo Flutter", 0.0f,   0.1f));
    params.push_back (pct (ParamID::HP,       "Highpass",       0.0f,   0.1f));
    params.push_back (pct (ParamID::LP,       "Lowpass",        100.0f, 0.1f));
    params.push_back (pct (ParamID::Stuffing, "Stuffing",       0.0f,   0.1f));

    return { params.begin(), params.end() };
}

SmudgeAudioProcessor::SmudgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SmudgeState", createParameterLayout())
{
}

void SmudgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    smudge_.prepare (sampleRate, samplesPerBlock);

    pAmount   = apvts.getRawParameterValue (ParamID::Amount);
    pWarble   = apvts.getRawParameterValue (ParamID::Warble);
    pRate     = apvts.getRawParameterValue (ParamID::Rate);
    pStereo   = apvts.getRawParameterValue (ParamID::Stereo);
    pFlutter  = apvts.getRawParameterValue (ParamID::Flutter);
    pHP       = apvts.getRawParameterValue (ParamID::HP);
    pLP       = apvts.getRawParameterValue (ParamID::LP);
    pStuffing = apvts.getRawParameterValue (ParamID::Stuffing);
}

void SmudgeAudioProcessor::releaseResources() { smudge_.reset(); }
void SmudgeAudioProcessor::reset()             { smudge_.reset(); }

bool SmudgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::create5point1()
        || in == juce::AudioChannelSet::create7point1()
        || in == juce::AudioChannelSet::ambisonic();
}

GlitchCore::Smudge::Params SmudgeAudioProcessor::buildParams() const
{
    GlitchCore::Smudge::Params p;
    // Null guard: if processBlock is called before prepareToPlay (rare but
    // possible during state restore), pointers are null. Return defaults.
    if (pAmount == nullptr) return p;

    p.amount        = pAmount->load()   / 100.0f;
    p.warble        = pWarble->load()   / 100.0f;
    p.rate          = pRate->load()     / 100.0f;
    p.stereo        = pStereo->load()   / 100.0f;
    p.stereoFlutter = pFlutter->load()  / 100.0f;
    p.highpass      = pHP->load()       / 100.0f;
    p.lowpass       = pLP->load()       / 100.0f;
    p.stuffing      = pStuffing->load() / 100.0f;
    return p;
}

void SmudgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    const float* inputs [32] = {};
    float*       outputs[32] = {};
    for (int ch = 0; ch < numChannels && ch < 32; ++ch)
    {
        inputs [ch] = buffer.getReadPointer  (ch);
        outputs[ch] = buffer.getWritePointer (ch);
    }
    smudge_.process (inputs, outputs, numChannels, numSamples, buildParams());
}

void SmudgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SmudgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SmudgeAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmudgeAudioProcessor();
}
