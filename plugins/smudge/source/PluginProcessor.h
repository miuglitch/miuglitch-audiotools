#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/smudge/smudge.h"

//==============================================================================
class SmudgeAudioProcessor : public juce::AudioProcessor
{
public:
    SmudgeAudioProcessor();
    ~SmudgeAudioProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset          () override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const juce::String getName() const override { return "Smudge"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    GlitchCore::Smudge smudge_;

    std::atomic<float>* pAmount   = nullptr;
    std::atomic<float>* pWarble   = nullptr;
    std::atomic<float>* pRate     = nullptr;
    std::atomic<float>* pStereo   = nullptr;
    std::atomic<float>* pFlutter  = nullptr;
    std::atomic<float>* pHP       = nullptr;
    std::atomic<float>* pLP       = nullptr;
    std::atomic<float>* pStuffing = nullptr;

    GlitchCore::Smudge::Params buildParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmudgeAudioProcessor)
};
