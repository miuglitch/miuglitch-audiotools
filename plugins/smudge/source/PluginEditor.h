#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// SmudgeAudioProcessorEditor
// Placeholder — replace with designed UI when ready
// Place in: free/smudge-vst/source/PluginEditor.h
//==============================================================================

class SmudgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SmudgeAudioProcessorEditor (SmudgeAudioProcessor&);
    ~SmudgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SmudgeAudioProcessor& processor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmudgeAudioProcessorEditor)
};
