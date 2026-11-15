#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Bottom panel. ADD AUTO (record mode) on the left: while it's on, every plug-in
// parameter you touch gets a tab. Tabs have an on/off checkbox; active lanes are
// overlaid over the clip waveform, the selected lane at full opacity and editable.
// The view fits the whole clip; drag the strip at the top of the waveform (up/down
// zooms, left/right scrolls) or Ctrl+mouse wheel to zoom. Double-click the strip to fit.
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
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int tabsHeight = 32;
    static constexpr int rulerHeight = 24;
    static constexpr int zoomStripHeight = 22;
    static constexpr int addAutoWidth = 96;

    struct Tab { juce::ValueTree lane; juce::Rectangle<int> bounds, box; };

    std::vector<Tab> layoutTabs() const;
    juce::Rectangle<int> areaBounds() const;
    juce::Rectangle<int> zoomStrip() const;
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
    void setView (double start, double end);
    void zoomAround (double anchorMs, float anchorX, double factor);

    void paintTabs (juce::Graphics&);
    void paintArea (juce::Graphics&);
    void paintRuler (juce::Graphics&);

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void willDestroyAudioSource (juce::ARAAudioSource*) override;

    ARAckProcessor& proc;
    juce::TextButton addAutoButton { "Add auto" };

    int selectedLaneUid = 0;
    double clipLengthMs = 30000.0;
    double clipStartSeconds = 0.0;
    double viewStart = 0.0, viewEnd = 30000.0;
    float hoverX = -1.0f;
    juce::ValueTree dragPoint;

    // zoom-strip drag
    bool zoomDragging = false;
    double dragStartView0 = 0.0, dragStartView1 = 0.0, dragAnchorMs = 0.0;

    juce::ARAAudioSource* audioSource = nullptr;
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbCache { 4 };
    juce::AudioThumbnail thumbnail { 256, formatManager, thumbCache };
};
