#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"
#include "ChainPanel.h"
#include "AutomationPanel.h"

// Small icon button (undo / redo / settings / pop-out).
class IconButton final : public juce::Button
{
public:
    enum class Kind { undo, redo, settings, popout };
    IconButton (Kind k) : juce::Button ({}), kind (k) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool flat = false; // no filled background (used in panel headers)
private:
    Kind kind;
};

// Black display strip: logo, clip length, ARA file name, undo/redo, settings.
class HeaderBar final : public juce::Component
{
public:
    HeaderBar();
    void paint (juce::Graphics&) override;
    void resized() override;
    void setClip (double lengthMs, const juce::String& fileName);

    IconButton undoButton { IconButton::Kind::undo }, redoButton { IconButton::Kind::redo },
               settingsButton { IconButton::Kind::settings };

private:
    std::unique_ptr<juce::Drawable> logo;
    double clipMs = -1.0;
    juce::String file;
};

// Middle panel: the selected slot's own plug-in window. Plug-in windows are kept open
// per slot once created and just shown/hidden when switching, so switching doesn't flicker.
class PluginPanel final : public juce::Component,
                          private juce::ComponentListener
{
public:
    explicit PluginPanel (ClipModel&);
    ~PluginPanel() override;

    void showSlot (int slotUid);
    void instanceGoing (int slotUid);
    void refresh();

    // Size of the docked plug-in window (0,0 if none), and a callback when it changes.
    juce::Point<int> dockedEditorSize() const;
    std::function<void()> onContentSizeChanged;
    static constexpr int padding = 11;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class FloatWindow;
    juce::AudioProcessorEditor* currentEditor() const;
    void closeEditor (int slotUid);
    void closeAll();
    void setFloating (bool);
    void componentMovedOrResized (juce::Component&, bool, bool wasResized) override;

    ClipModel& model;
    int slotUid = 0;
    std::map<int, std::unique_ptr<juce::AudioProcessorEditor>> editors;
    std::unique_ptr<FloatWindow> floatWindow;
    IconButton popoutButton { IconButton::Kind::popout };
    juce::Point<int> lastReportedSize;
};

// One clip's worth of interface (search + CHAIN, plug-in window, automation).
// Rebuilt when the selected clip changes.
struct ClipView
{
    explicit ClipView (ClipModel& m) : model (m), chainPanel (m), pluginPanel (m), automation (m) {}
    ClipModel& model;
    ChainPanel chainPanel;
    PluginPanel pluginPanel;
    AutomationPanel automation;
};

class ARAckEditor final : public juce::AudioProcessorEditor,
                          public juce::AudioProcessorEditorARAExtension,
                          public juce::DragAndDropContainer,
                          private juce::Timer,
                          private juce::ValueTree::Listener,
                          private juce::ARAAudioModification::Listener
{
public:
    explicit ARAckEditor (ARAckProcessor&);
    ~ARAckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    juce::AudioProcessorEditorARAExtension* getARAClientExtensions() override { return this; }

private:
    void timerCallback() override;
    void updateClip();
    void bindTo (ClipModel*, juce::ARAAudioModification*);
    void select (int slotUid);
    void anyChange();
    void fitToPlugin();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { anyChange(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { anyChange(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { anyChange(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { anyChange(); }
    void valueTreeRedirected (juce::ValueTree&) override { anyChange(); }

    void willDestroyAudioModification (juce::ARAAudioModification*) override;

    static constexpr int defaultWidth = 1100, defaultHeight = 770;
    static constexpr int headerHeight = 60, chainWidth = 270, automationHeight = 240;

    ARAckProcessor& proc;
    ARAckLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 1000 };
    HeaderBar header;

    ClipModel* model = nullptr;
    juce::ARAAudioModification* boundModification = nullptr;
    std::unique_ptr<ClipView> view;
    juce::ARAPlaybackRegion* lastRegion = nullptr;
    int tick = 0;
};
