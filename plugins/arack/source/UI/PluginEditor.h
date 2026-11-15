#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"
#include "ChainPanel.h"
#include "AutomationPanel.h"

// Small icon button (undo / redo / settings) for the display header.
class IconButton final : public juce::Button
{
public:
    enum class Kind { undo, redo, settings };
    IconButton (Kind k) : juce::Button ({}), kind (k) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    Kind kind;
};

// Black display strip: logo, clip length (ms), ARA file name, undo/redo, settings.
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

// Middle panel: shows the selected slot's own plug-in interface.
class PluginPanel final : public juce::Component,
                          private juce::ComponentListener
{
public:
    explicit PluginPanel (ARAckProcessor&);
    ~PluginPanel() override;

    void showSlot (int slotUid);
    void instanceGoing (int slotUid);
    void refresh();

    // Size of the docked plug-in window (0,0 if none), and a callback when it changes.
    juce::Point<int> dockedEditorSize() const;
    std::function<void()> onContentSizeChanged;
    static constexpr int padding = 11;

private:
    juce::Point<int> lastReportedSize;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class FloatWindow;
    void closeEditor();
    void setFloating (bool);
    void componentMovedOrResized (juce::Component&, bool, bool wasResized) override;

    ARAckProcessor& proc;
    int slotUid = 0;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::unique_ptr<FloatWindow> floatWindow;
    juce::Viewport viewport;
    juce::TextButton floatButton { "Float" }, onButton { "On" };
};

class ARAckEditor final : public juce::AudioProcessorEditor,
                          public juce::AudioProcessorEditorARAExtension,
                          public juce::DragAndDropContainer,
                          private juce::Timer,
                          private juce::ValueTree::Listener
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
    void updateClipInfo();
    void select (int slotUid);
    void anyChange();
    void fitToPlugin();

    static constexpr int defaultWidth = 1100, defaultHeight = 770;
    static constexpr int headerHeight = 60, chainWidth = 270, automationHeight = 240;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { anyChange(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { anyChange(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { anyChange(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { anyChange(); }
    void valueTreeRedirected (juce::ValueTree&) override { anyChange(); }

    ARAckProcessor& proc;
    ARAckLookAndFeel lnf;
    HeaderBar header;
    ChainPanel chainPanel;
    PluginPanel pluginPanel;
    AutomationPanel automation;
    juce::ARAPlaybackRegion* lastRegion = nullptr;
};
