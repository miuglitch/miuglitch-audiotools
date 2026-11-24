#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Two-octave keyboard for the Custom scale. Click a key to include/exclude it.
// Bit 0 = C of the low octave ... bit 23 = B of the high octave. The pattern
// repeats every two octaves across the whole range.
//==============================================================================

class CustomScaleKeyboard : public juce::Component
{
public:
    std::function<void (std::uint32_t)> onChange;

    void setMask (std::uint32_t m) { if (m != mask_) { mask_ = m; repaint(); } }
    std::uint32_t getMask() const   { return mask_; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> keyBounds (int note) const;
    int  noteAt (juce::Point<float>) const;
    void applyAt (juce::Point<float>);

    std::uint32_t mask_ = 0;
    int  lastNote_  = -1;
    bool paintOn_   = true;
};

//==============================================================================
class SpectralHarmonizerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit SpectralHarmonizerAudioProcessorEditor (SpectralHarmonizerAudioProcessor&);
    ~SpectralHarmonizerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateCustomVisibility();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabelledSlider
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attach;
    };
    struct LabelledCombo
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<ComboAttachment> attach;
    };

    void setupSlider (LabelledSlider&, const juce::String& paramID, const juce::String& text, bool rotary);
    void setupCombo  (LabelledCombo&,  const juce::String& paramID, const juce::String& text);

    SpectralHarmonizerAudioProcessor& processor_;

    LabelledSlider amount_, sustain_, harmonics_, bendRange_, mpeBendRange_;
    LabelledCombo  root_, scale_, source_, midiTargets_, fftSize_;
    juce::ToggleButton latch_ { "Latch" }, mpe_ { "MPE" };
    std::unique_ptr<ButtonAttachment> latchAttach_, mpeAttach_;

    CustomScaleKeyboard keyboard_;
    juce::TextButton clearButton_ { "Clear" }, fromScaleButton_ { "Copy scale" };
    juce::Label statusLabel_;
    bool customShown_ = false;
    int  lastRealScale_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralHarmonizerAudioProcessorEditor)
};
