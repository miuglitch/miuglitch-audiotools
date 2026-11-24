#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/disintegrate/disintegrate.h"

class DisintegrateProcessor : public juce::AudioProcessor
{
public:
    DisintegrateProcessor();
    ~DisintegrateProcessor() override;

    // Playback
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Plugin info
    const juce::String getName() const override { return "Disintegrate"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs (not used)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // State save/load
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter tree — accessed by the editor
    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    GlitchCore::Disintegrate disintegrate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DisintegrateProcessor)
};