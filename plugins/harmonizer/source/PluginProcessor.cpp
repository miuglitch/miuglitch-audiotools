#include "PluginProcessor.h"
#include "PluginEditor.h"

using Harmonizer = GlitchCore::SpectralHarmonizer;

static const juce::Identifier kCustomMaskProp { "customMask" };

juce::AudioProcessorValueTreeState::ParameterLayout
SpectralHarmonizerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [] (const juce::String& id, const juce::String& name, float def)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
            juce::AudioParameterFloatAttributes().withLabel ("%"));
    };
    auto choice = [] (const juce::String& id, const juce::String& name,
                      const juce::StringArray& items, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
    };
    auto integer = [] (const juce::String& id, const juce::String& name, int lo, int hi, int def,
                       const juce::String& label)
    {
        return std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, lo, hi, def,
            juce::AudioParameterIntAttributes().withLabel (label));
    };
    auto toggle = [] (const juce::String& id, const juce::String& name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
    };

    juce::StringArray scaleNames;
    for (const auto& s : GlitchCore::kScales) scaleNames.add (s.name);
    scaleNames.add ("Custom");

    params.push_back (pct    (ParamID::Amount,  "Amount",  0.0f));
    params.push_back (pct    (ParamID::Sustain, "Sustain", 20.0f));
    params.push_back (choice (ParamID::Root,  "Root",
                              { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back (choice (ParamID::Scale, "Scale", scaleNames, 0));
    params.push_back (choice (ParamID::Source, "Target Source",
                              { "Auto (MIDI, else Scale)", "Scale", "MIDI" }, 0));
    params.push_back (choice (ParamID::MidiTargets, "MIDI Targets",
                              { "Harmonic Series", "All Octaves" }, 0));
    params.push_back (integer (ParamID::Harmonics, "Harmonics", 1,
                               Harmonizer::kMaxPartialsPerVoice, 16, ""));
    params.push_back (toggle  (ParamID::Latch, "Latch", false));
    params.push_back (toggle  (ParamID::Mpe,   "MPE",   false));
    params.push_back (integer (ParamID::BendRange,    "Bend Range",     0, 48, 2,  "st"));
    params.push_back (integer (ParamID::MpeBendRange, "MPE Bend Range", 0, 96, 48, "st"));
    params.push_back (choice  (ParamID::FftSize, "FFT Size", { "2048", "4096" }, 1));

    return { params.begin(), params.end() };
}

SpectralHarmonizerAudioProcessor::SpectralHarmonizerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SpectralHarmonizerState", createParameterLayout())
{
    pAmount       = apvts.getRawParameterValue (ParamID::Amount);
    pSustain      = apvts.getRawParameterValue (ParamID::Sustain);
    pRoot         = apvts.getRawParameterValue (ParamID::Root);
    pScale        = apvts.getRawParameterValue (ParamID::Scale);
    pSource       = apvts.getRawParameterValue (ParamID::Source);
    pMidiTargets  = apvts.getRawParameterValue (ParamID::MidiTargets);
    pHarmonics    = apvts.getRawParameterValue (ParamID::Harmonics);
    pLatch        = apvts.getRawParameterValue (ParamID::Latch);
    pMpe          = apvts.getRawParameterValue (ParamID::Mpe);
    pBendRange    = apvts.getRawParameterValue (ParamID::BendRange);
    pMpeBendRange = apvts.getRawParameterValue (ParamID::MpeBendRange);
    pFftSize      = apvts.getRawParameterValue (ParamID::FftSize);

    apvts.state.setProperty (kCustomMaskProp, (int) customMask_.load(), nullptr);
    setLatencySamples (harmonizer_.getLatencySamples());
}

Harmonizer::FftSize SpectralHarmonizerAudioProcessor::requestedFftSize() const
{
    return pFftSize->load() < 0.5f ? Harmonizer::FftSize::k2048 : Harmonizer::FftSize::k4096;
}

void SpectralHarmonizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    harmonizer_.prepare (sampleRate, samplesPerBlock, requestedFftSize());
    setLatencySamples (harmonizer_.getLatencySamples());
}

void SpectralHarmonizerAudioProcessor::releaseResources() { harmonizer_.reset(); }
void SpectralHarmonizerAudioProcessor::reset()             { harmonizer_.reset(); }

double SpectralHarmonizerAudioProcessor::getTailLengthSeconds() const
{
    return harmonizer_.getTailLengthSeconds (pSustain->load() / 100.0f);
}

bool SpectralHarmonizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SpectralHarmonizerAudioProcessor::setCustomMask (std::uint32_t mask)
{
    mask &= 0xffffffu;
    customMask_.store (mask);
    apvts.state.setProperty (kCustomMaskProp, (int) mask, nullptr);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

Harmonizer::Params SpectralHarmonizerAudioProcessor::buildParams() const
{
    Harmonizer::Params p;
    p.amount       = pAmount->load()  / 100.0f;
    p.sustain      = pSustain->load() / 100.0f;
    p.root         = (int) pRoot->load();
    p.scale        = (int) pScale->load();
    p.customMask   = customMask_.load();
    p.source       = (Harmonizer::TargetSource) (int) pSource->load();
    p.midiTargets  = (Harmonizer::MidiTargets)  (int) pMidiTargets->load();
    p.numHarmonics = (int) pHarmonics->load();
    p.latch        = pLatch->load() > 0.5f;
    p.mpe          = pMpe->load()   > 0.5f;
    p.bendRange    = pBendRange->load();
    p.mpeBendRange = pMpeBendRange->load();
    return p;
}

void SpectralHarmonizerAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    const int ch = m.getChannel();
    if      (m.isNoteOn())             harmonizer_.noteOn  (ch, m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())            harmonizer_.noteOff (ch, m.getNoteNumber());
    else if (m.isPitchWheel())         harmonizer_.pitchBend (ch, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())    harmonizer_.channelPressure (ch, (float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())         harmonizer_.polyPressure (ch, m.getNoteNumber(), (float) m.getAfterTouchValue() / 127.0f);
    else if (m.isController())         harmonizer_.controller (ch, m.getControllerNumber(), (float) m.getControllerValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) harmonizer_.allNotesOff();
}

void SpectralHarmonizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // FFT size switch: no allocation, new latency reported to the host.
    if (requestedFftSize() != harmonizer_.getFftSize())
    {
        harmonizer_.setFftSize (requestedFftSize());
        setLatencySamples (harmonizer_.getLatencySamples());
    }

    const auto params     = buildParams();
    const int numChannels = std::min (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    // Split the block at each MIDI event so note changes land on time.
    int pos = 0;
    auto runTo = [&] (int end)
    {
        if (end <= pos) return;
        const float* in [2] = {};
        float*       out[2] = {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            in [ch] = buffer.getReadPointer  (ch, pos);
            out[ch] = buffer.getWritePointer (ch, pos);
        }
        harmonizer_.process (in, out, numChannels, end - pos, params);
        pos = end;
    };

    for (const auto meta : midi)
    {
        runTo (juce::jlimit (0, numSamples, meta.samplePosition));
        handleMidi (meta.getMessage());
    }
    runTo (numSamples);

    const int notes = harmonizer_.getActiveNoteCount();
    followingMidi_.store (params.source == Harmonizer::TargetSource::Midi
                          || (params.source == Harmonizer::TargetSource::Auto && notes > 0));
    heldNotes_.store (notes);
}

void SpectralHarmonizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kCustomMaskProp, (int) customMask_.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SpectralHarmonizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasProperty (kCustomMaskProp))
            customMask_.store ((std::uint32_t) (int) tree.getProperty (kCustomMaskProp) & 0xffffffu);
        apvts.replaceState (tree);
    }
}

juce::AudioProcessorEditor* SpectralHarmonizerAudioProcessor::createEditor()
{
    return new SpectralHarmonizerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralHarmonizerAudioProcessor();
}
