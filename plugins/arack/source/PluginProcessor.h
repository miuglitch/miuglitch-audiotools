#pragma once

#include "ARADocument.h"

class ARAckProcessor final : public juce::AudioProcessor,
                             public juce::AudioProcessorARAExtension,
                             private juce::Timer
{
public:
    ARAckProcessor();
    ~ARAckProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ARAck"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // Only used when ARAck runs as a plain (non-ARA) VST3. In ARA mode each clip's
    // state is saved through the ARA document archive instead.
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorARAExtension* getARAClientExtensions() override { return this; }

    // The clip model used when there is no ARA (plain insert / standalone).
    ClipModel localModel;

    std::atomic<double> hostTimeSeconds { 0.0 };
    std::atomic<bool> hostPlaying { false };

private:
    void timerCallback() override; // reports chain latency to the host

    JUCE_DECLARE_WEAK_REFERENCEABLE (ARAckProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAckProcessor)
};
