#include "PluginEditor.h"

ParticulateAudioProcessorEditor::ParticulateAudioProcessorEditor (ParticulateAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      gainAttachment (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
          p.getAPVTS(),
          "gain",
          gainSlider))
{
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
    addAndMakeVisible (gainSlider);

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);

    setSize (360, 280);
}

ParticulateAudioProcessorEditor::~ParticulateAudioProcessorEditor() = default;

void ParticulateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1e));

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawFittedText ("Particulate", getLocalBounds().removeFromTop (48),
                      juce::Justification::centred, 1);
}

void ParticulateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24);
    area.removeFromTop (40);

    gainLabel.setBounds (area.removeFromTop (24));
    gainSlider.setBounds (area.withSizeKeepingCentre (160, 160));
}
