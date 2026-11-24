#pragma once

#include "../ARADocument.h"
#include "Theme.h"

// Bottom panel.
// - ADD AUTO (vertical, on the left): record mode. While on, every plug-in parameter you
//   touch gets a tab. Hover for a tooltip.
// - Tabs: one per parameter, checkbox = on/off. Active lanes are overlaid over the clip
//   waveform; the selected one is full opacity and editable, the rest faded.
// - Editing: click to add a point, drag points to move, drag on empty space for a box
//   selection (then drag any selected point to move them all), Delete removes selected
//   points, double-click a point to delete it, Alt+drag draws with a pencil (cleaned up
//   on release).
// - View: always fits the whole clip. Drag the strip at the top (down/up = zoom, sideways
//   = scroll), Ctrl+wheel zooms, middle mouse drag pans, double-click the strip to fit.
class AutomationPanel final : public juce::Component,
                              private juce::ARAAudioSource::Listener
{
public:
    explicit AutomationPanel (ClipModel&);
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
    bool keyPressed (const juce::KeyPress&) override;

private:
    static constexpr int sideWidth = 30;
    static constexpr int tabsHeight = 32;
    static constexpr int rulerHeight = 24;
    static constexpr int zoomStripHeight = 22;

    class VerticalToggle;
    class Peaks;

    struct Tab { juce::ValueTree lane; juce::Rectangle<int> bounds, box; };

    juce::Rectangle<int> content() const;
    std::vector<Tab> layoutTabs() const;
    juce::Rectangle<int> areaBounds() const;
    juce::Rectangle<int> lanesBounds() const;
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
    void finishPencil();
    bool isSelected (const juce::ValueTree& pt) const;

    void paintTabs (juce::Graphics&);
    void paintArea (juce::Graphics&);
    void paintRuler (juce::Graphics&);

    void willDestroyAudioSource (juce::ARAAudioSource*) override;

    ClipModel& model;
    std::unique_ptr<VerticalToggle> addAutoButton;

    int selectedLaneUid = 0;
    double clipLengthMs = 30000.0;
    double clipStartSeconds = 0.0;
    double viewStart = 0.0, viewEnd = 30000.0;
    float hoverX = -1.0f;

    enum class Drag { none, zoom, pan, movePoints, box, pencil, pendingClick };
    Drag drag = Drag::none;
    double dragStartView0 = 0.0, dragStartView1 = 0.0, dragAnchorMs = 0.0;
    juce::Array<juce::ValueTree> selection;
    std::vector<std::pair<double, float>> moveOrigins;
    juce::Rectangle<int> boxRect;
    std::vector<std::pair<double, float>> pencil;

    juce::ARAAudioSource* audioSource = nullptr;
    std::unique_ptr<Peaks> peaks;
};
