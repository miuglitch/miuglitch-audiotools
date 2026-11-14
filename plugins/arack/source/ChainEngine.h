#pragma once

#include "Session.h"

// Appends a line to %APPDATA%\ARAck\ARAck.log (for diagnosing host issues).
void arackLog (const juce::String&);

// Shared, per-process catalogue of scanned plug-ins (saved in %APPDATA%\ARAck).
// Scans the VST3 folders itself, one file per timer tick on the message thread,
// skipping ARAck's own bundle. A plug-in that crashes the scan is skipped next time.
struct PluginCatalog final : public juce::ChangeBroadcaster,
                             private juce::ChangeListener,
                             private juce::Timer
{
    PluginCatalog();
    ~PluginCatalog() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { save(); }
    void save();
    juce::Array<juce::PluginDescription> usableTypes() const; // everything except ARAck itself
    juce::File deadMansPedal();

    void startScan();
    bool isScanning() const { return scanning; }
    juce::String scanStatus() const; // "Scanning 3 / 40: Name"

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;
    juce::ApplicationProperties props;

private:
    void timerCallback() override;

    juce::StringArray pending;
    juce::AudioPluginFormat* vst3 = nullptr;
    bool scanning = false;
    int scanTotal = 0, scanDone = 0;
    juce::String scanCurrent;
};

// Hosts the plug-ins of the 12 CHAIN slots and applies automation.
// The ValueTree in Session is the single source of truth: this class watches it and
// loads/unloads plug-ins to match, so undo/redo of any slot edit just works.
// Disabled slots are left out of processing and out of the latency sum entirely.
class ChainEngine final : private juce::ValueTree::Listener,
                          private juce::AsyncUpdater,
                          private juce::AudioProcessorListener
{
public:
    ChainEngine (Session&, juce::AudioProcessor& owner);
    ~ChainEngine() override;

    void prepare (double sampleRate, int blockSize);
    void release();

    // Audio thread. posMs < 0 means "no position": automation is not applied.
    void process (juce::AudioBuffer<float>&, double posMs) noexcept;

    juce::AudioPluginInstance* getInstance (int slotUid) const;
    bool isLoading (int slotUid) const { return loading.count (slotUid) > 0; }
    juce::String getLoadError (int slotUid) const;
    int getTotalLatencySamples() const { return totalLatency; }
    double getSampleRate() const { return sampleRate; }

    // Edits (message thread). Each one is a single undo step.
    void loadPlugin (int slotUid, const juce::PluginDescription&);
    void removePlugin (int slotUid);
    void setEnabled (int slotUid, bool);
    void moveSlot (int slotUid, int newIndex);
    void storePluginStates(); // writes hosted plug-in states into the tree (not undoable)

    std::function<void (int slotUid)> onBeforeInstanceRemoved;
    std::function<void()> onChanged;

private:
    struct LaneRT
    {
        juce::AudioProcessorParameter* param = nullptr;
        std::vector<std::pair<double, float>> points;
        float lastSent = -1.0f;
    };

    struct Snapshot
    {
        std::vector<juce::AudioPluginInstance*> chain;
        std::vector<LaneRT> lanes;
    };

    struct ParamWatcher;
    struct ParamAction;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { triggerAsyncUpdate(); }
    void valueTreeRedirected (juce::ValueTree&) override { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override;

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override;

    void startLoad (int slotUid, const juce::String& descXml);
    void configure (juce::AudioPluginInstance&) const;
    void rebuildSnapshot();
    void updateLatency();
    void gestureChanged (int slotUid, int paramIndex, bool starting, float value);

    Session& session;
    juce::AudioProcessor& owner;
    juce::SharedResourcePointer<PluginCatalog> catalog;

    struct Hosted
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        std::unique_ptr<ParamWatcher> watcher;
        juce::String descXml;
    };

    std::map<int, Hosted> hosted;
    std::map<int, juce::String> loading;     // slotUid -> desc being loaded
    std::map<int, juce::String> loadErrors;
    std::map<std::pair<int, int>, float> gestureStartValues;

    juce::SpinLock snapshotLock;
    std::unique_ptr<Snapshot> snapshot;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midi;

    double sampleRate = 0.0;
    int blockSize = 0;
    int totalLatency = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ChainEngine)
};
