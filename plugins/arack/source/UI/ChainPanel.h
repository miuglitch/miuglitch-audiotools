#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// Left panel: CHAIN with 12 slots. Drag handle, power (enable) button, name,
// divider, latency. Drag a handle to reorder. Click an empty slot to load.
class ChainPanel final : public juce::Component
{
public:
    explicit ChainPanel (ARAckProcessor&);
    ~ChainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    std::function<void (int slotUid)> onSelect;

    static constexpr int headerHeight = 36;
    static constexpr int rowHeight = 34;

private:
    class SlotRow;
    friend class SlotRow;

    void showLoadMenu (int slotUid);
    void showSlotMenu (int slotUid);
    void openScanner();

    ARAckProcessor& proc;
    juce::TextButton latencyButton { "Latency" };
    juce::OwnedArray<SlotRow> rows;
    bool showLatency = true;
    std::unique_ptr<juce::DialogWindow> scanWindow;
};
