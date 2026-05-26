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

    // Controls
    juce::Slider amountKnob;
    juce::Slider minDropKnob;
    juce::ToggleButton stereoLinkButton;

    // Labels
    juce::Label amountLabel;
    juce::Label minDropLabel;

    // Attachments — keep knobs in sync with parameters automatically
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> minDropAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stereoLinkAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DisintegrateEditor)
};