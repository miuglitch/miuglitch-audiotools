#pragma once

#include "../ARADocument.h"
#include "Theme.h"

// Left panel: a plug-in search bar on top (live results, arrow keys, Enter adds to
// the selected empty slot or the first empty one, Ctrl+F jumps here), then the 12
// CHAIN slots: drag handle, power (enable) button, name, divider, latency.
class ChainPanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ChainPanel (ClipModel&);
    ~ChainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void focusSearch();
    void runSelfTest (const juce::String& query);

    std::function<void (int slotUid)> onSelect;

    static constexpr int headerHeight = 36;
    static constexpr int rowHeight = 34;

private:
    class SlotRow;
    class SearchBox;
    class ResultsList;
    friend class SlotRow;
    friend class SearchBox;
    friend class ResultsList;

    void showLoadMenu (int slotUid);
    void showSlotMenu (int slotUid);
    void openScanner();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void updatePlaceholder();
    void updateResults();
    void moveResult (int delta);
    void addSelectedResult();
    void closeSearch();
    int targetSlotForAdd() const;

    juce::SharedResourcePointer<PluginCatalog> catalog;

    ClipModel& model;
    juce::TextButton latencyButton { "Latency" };
    juce::OwnedArray<SlotRow> rows;
    std::unique_ptr<SearchBox> search;
    std::unique_ptr<ResultsList> results;
    bool showLatency = true;
};
