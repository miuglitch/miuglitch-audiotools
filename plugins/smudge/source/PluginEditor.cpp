#include "PluginEditor.h"

SmudgeAudioProcessorEditor::SmudgeAudioProcessorEditor (SmudgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p)
{
    setSize (400, 300);
}

void SmudgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("Smudge — UI coming soon",
                      getLocalBounds(), juce::Justification::centred, 1);
}

void SmudgeAudioProcessorEditor::resized() {}
