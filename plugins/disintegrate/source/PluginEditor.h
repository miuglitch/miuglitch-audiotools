#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class DisintegrateEditor : public juce::AudioProcessorEditor
{
public:
    DisintegrateEditor(DisintegrateProcessor&);
    ~DisintegrateEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DisintegrateProcessor& processor;

    juce::Slider amountKnob;
    juce::Slider minDropKnob;
    juce::Slider jitterKnob;
    juce::ToggleButton stereoLinkButton;

    juce::Label amountLabel;
    juce::Label minDropLabel;
    juce::Label jitterLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> minDropAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> jitterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stereoLinkAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DisintegrateEditor)
};