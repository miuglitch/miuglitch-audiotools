#pragma once

#include "PluginProcessor.h"

class ParticulateAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ParticulateAudioProcessorEditor (ParticulateAudioProcessor&);
    ~ParticulateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ParticulateAudioProcessor& processorRef;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticulateAudioProcessorEditor)
};
