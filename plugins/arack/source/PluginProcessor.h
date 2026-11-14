#pragma once

#include "ARADocument.h"
#include "ChainEngine.h"

class ARAckProcessor final : public juce::AudioProcessor,
                             public juce::AudioProcessorARAExtension
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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorARAExtension* getARAClientExtensions() override { return this; }

    Session session;
    ChainEngine chain { session, *this };

    // For the UI.
    std::atomic<double> clipPositionMs { -1.0 }; // -1 = outside any clip
    std::atomic<double> hostTimeSeconds { 0.0 };
    std::atomic<bool> hostPlaying { false };
    int selectedSlotUid = 0; // UI state, not part of undo

private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (ARAckProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAckProcessor)
};
