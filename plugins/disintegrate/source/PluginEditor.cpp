#include "PluginEditor.h"

DisintegrateEditor::DisintegrateEditor(DisintegrateProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Amount knob
    amountKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    amountKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    amountKnob.setRange(0.0, 1.0);
    addAndMakeVisible(amountKnob);

    amountLabel.setText("Amount", juce::dontSendNotification);
    amountLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(amountLabel);

    // Min Drop Time knob
    minDropKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    minDropKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    minDropKnob.setRange(1.0, 500.0);
    minDropKnob.setTextValueSuffix(" ms");
    addAndMakeVisible(minDropKnob);

    minDropLabel.setText("Min Drop Time", juce::dontSendNotification);
    minDropLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(minDropLabel);

    // Stereo Link toggle
    stereoLinkButton.setButtonText("Stereo Link");
    stereoLinkButton.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(stereoLinkButton);

    // Attach controls to parameters
    amountAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                            (processor.apvts, "amount", amountKnob);
    minDropAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                            (processor.apvts, "minDropMs", minDropKnob);
    stereoLinkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                            (processor.apvts, "stereoLink", stereoLinkButton);

    setSize(300, 200);
}

DisintegrateEditor::~DisintegrateEditor() {}

void DisintegrateEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawFittedText("Disintegrate", getLocalBounds().removeFromTop(30),
                     juce::Justification::centred, 1);
}

void DisintegrateEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // title

    // Two knobs side by side
    auto knobArea = area.removeFromTop(120);
    auto amountArea  = knobArea.removeFromLeft(knobArea.getWidth() / 2);
    auto minDropArea = knobArea;

    amountLabel.setBounds(amountArea.removeFromTop(20));
    amountKnob.setBounds(amountArea);

    minDropLabel.setBounds(minDropArea.removeFromTop(20));
    minDropKnob.setBounds(minDropArea);

    // Stereo link button at the bottom
    stereoLinkButton.setBounds(area.removeFromTop(30).reduced(60, 0));
}