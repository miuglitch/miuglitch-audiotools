#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Bottom panel: one tab per touched parameter (checkbox = on/off), all active lanes
// overlaid over the clip waveform, the selected lane at full opacity and editable,
// a millisecond ruler along the bottom and a hover readout.
class AutomationPanel final : public juce::Component,
                              private juce::ChangeListener,
                              private juce::ARAAudioSource::Listener
{
public:
    explicit AutomationPanel (ARAckProcessor&);
    ~AutomationPanel() override;

    // Clip shown in the panel. source may be null (no ARA clip).
    void setClip (juce::ARAAudioSource* source, double startInSourceSeconds, double lengthMs);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    static constexpr int tabsHeight = 32;
    static constexpr int rulerHeight = 24;

    struct Tab { juce::ValueTree lane; juce::Rectangle<int> bounds, box; };

    std::vector<Tab> layoutTabs() const;
    juce::Rectangle<int> areaBounds() const;
    juce::Rectangle<int> rulerBounds() const;
    float msToX (double ms) const;
    double xToMs (float x) const;
    float valueToY (float v) const;
    float yToValue (float y) const;
    juce::ValueTree selectedLane() const;
    juce::AudioProcessorParameter* paramFor (const juce::ValueTree& lane) const;
    juce::String valueText (const juce::ValueTree& lane, float v) const;
    float valueAt (const juce::ValueTree& lane, double ms) const;
    juce::ValueTree pointAt (juce::Point<float>) const;
    double rulerStep() const;

    void paintTabs (juce::Graphics&);
    void paintArea (juce::Graphics&);
    void paintRuler (juce::Graphics&);

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void willDestroyAudioSource (juce::ARAAudioSource*) override;

    ARAckProcessor& proc;
    int selectedLaneUid = 0;
    double clipLengthMs = 30000.0;
    double clipStartSeconds = 0.0;
    float hoverX = -1.0f;
    juce::ValueTree dragPoint;

    juce::ARAAudioSource* audioSource = nullptr;
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbCache { 4 };
    juce::AudioThumbnail thumbnail { 256, formatManager, thumbCache };
};
