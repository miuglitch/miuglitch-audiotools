#include "PluginEditor.h"

DisintegrateEditor::DisintegrateEditor(DisintegrateProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Amount knob
    amountKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    amountKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    amountKnob.setRange(0.0, 100.0, 1.0);
    amountKnob.setTextValueSuffix("%");
    addAndMakeVisible(amountKnob);

    amountLabel.setText("Amount", juce::dontSendNotification);
    amountLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(amountLabel);

    // Min Drop Time knob
    minDropKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    minDropKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    minDropKnob.setRange(1.0, 500.0, 0.1);
    minDropKnob.setTextValueSuffix(" ms");
    addAndMakeVisible(minDropKnob);

    minDropLabel.setText("Min Drop Time", juce::dontSendNotification);
    minDropLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(minDropLabel);

    // Jitter knob
    jitterKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    jitterKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    jitterKnob.setRange(0.0, 100.0, 1.0);
    jitterKnob.setTextValueSuffix("%");
    addAndMakeVisible(jitterKnob);

    jitterLabel.setText("Jitter", juce::dontSendNotification);
    jitterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jitterLabel);

    // Stereo Link toggle
    stereoLinkButton.setButtonText("Stereo Link");
    stereoLinkButton.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(stereoLinkButton);

    // Attach controls to parameters
    amountAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                            (processor.apvts, "amount", amountKnob);
    minDropAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                            (processor.apvts, "minDropMs", minDropKnob);
    jitterAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                            (processor.apvts, "jitter", jitterKnob);
    stereoLinkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                            (processor.apvts, "stereoLink", stereoLinkButton);

    setSize(400, 200);
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
    auto area      = getLocalBounds().reduced(10);
    area.removeFromTop(30);

    auto knobArea  = area.removeFromTop(120);
    int  knobWidth = knobArea.getWidth() / 3;

    auto amountArea  = knobArea.removeFromLeft(knobWidth);
    auto minDropArea = knobArea.removeFromLeft(knobWidth);
    auto jitterArea  = knobArea;

    amountLabel.setBounds(amountArea.removeFromTop(20));
    amountKnob.setBounds(amountArea);

    minDropLabel.setBounds(minDropArea.removeFromTop(20));
    minDropKnob.setBounds(minDropArea);

    jitterLabel.setBounds(jitterArea.removeFromTop(20));
    jitterKnob.setBounds(jitterArea);

    stereoLinkButton.setBounds(area.removeFromTop(30).reduced(80, 0));
}