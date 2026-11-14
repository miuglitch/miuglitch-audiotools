#include "ChainEngine.h"

using namespace juce;

//==============================================================================
PluginCatalog::PluginCatalog()
{
    PropertiesFile::Options o;
    o.applicationName = "ARAck";
    o.folderName = "ARAck";
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (o);

    addDefaultFormatsToManager (formats);

    for (auto* f : formats.getFormats())
        if (f->getName() == "VST3")
            vst3 = f;

    arackLog ("Catalog: " + String (formats.getNumFormats()) + " format(s), VST3 "
              + (vst3 != nullptr ? "available" : "MISSING"));

    if (auto xml = props.getUserSettings()->getXmlValue ("knownPlugins"))
        known.recreateFromXml (*xml);

    known.addChangeListener (this);
}

PluginCatalog::~PluginCatalog()
{
    stopTimer();
    known.removeChangeListener (this);
}

void PluginCatalog::startScan()
{
    if (scanning || vst3 == nullptr)
        return;

    PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (known, deadMansPedal());

    auto paths = vst3->getDefaultLocationsToSearch();
    pending.clear();

    for (auto& id : vst3->searchPathsForPlugins (paths, true, false))
    {
        // Never scan ARAck itself (it would load into itself).
        if (id.containsIgnoreCase ("ARAck.vst3"))
            continue;

        if (! known.getBlacklistedFiles().contains (id))
            pending.add (id);
    }

    arackLog ("Scan: searching " + paths.toString() + ", found " + String (pending.size()) + " file(s)");

    scanTotal = pending.size();
    scanDone = 0;
    props.getUserSettings()->setValue ("scanComplete", false);
    scanning = true;
    sendChangeMessage();
    startTimer (20);
}

String PluginCatalog::scanStatus() const
{
    if (! scanning)
        return {};

    return "Scanning " + String (scanDone + 1) + " / " + String (scanTotal) + ": "
         + File (scanCurrent).getFileNameWithoutExtension();
}

void PluginCatalog::timerCallback()
{
    if (pending.isEmpty())
    {
        stopTimer();
        scanning = false;
        deadMansPedal().deleteFile();
        props.getUserSettings()->setValue ("scanComplete", true);
        save();
        arackLog ("Scan: done, " + String (known.getNumTypes()) + " plug-in(s) known");
        sendChangeMessage();
        return;
    }

    scanCurrent = pending[0];
    pending.remove (0);
    sendChangeMessage();

    // Remember which file is being scanned in case it takes the host down.
    deadMansPedal().replaceWithText (scanCurrent);

    OwnedArray<PluginDescription> found;
    known.scanAndAddFile (scanCurrent, true, found, *vst3);
    arackLog ("Scan: " + scanCurrent + " -> " + String (found.size()) + " type(s)");

    deadMansPedal().replaceWithText ({});
    ++scanDone;
}

void arackLog (const String& line)
{
    auto f = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("ARAck").getChildFile ("ARAck.log");
    f.getParentDirectory().createDirectory();

    if (f.getSize() > 2 * 1024 * 1024)
        f.deleteFile();

    f.appendText (Time::getCurrentTime().toString (true, true, true, true) + "  " + line + "\n");
}

void PluginCatalog::save()
{
    if (auto xml = known.createXml())
        props.getUserSettings()->setValue ("knownPlugins", xml.get());

    props.saveIfNeeded();
}

Array<PluginDescription> PluginCatalog::usableTypes() const
{
    Array<PluginDescription> out;

    for (auto& t : known.getTypes())
        if (! t.name.equalsIgnoreCase ("ARAck"))
            out.add (t);

    return out;
}

File PluginCatalog::deadMansPedal()
{
    return props.getUserSettings()->getFile().getSiblingFile ("ARAckScanCrashes.txt");
}

//==============================================================================
struct ChainEngine::ParamWatcher final : public AudioProcessorParameter::Listener
{
    ParamWatcher (ChainEngine& e, int uid, AudioPluginInstance& inst) : engine (&e), slotUid (uid), instance (inst)
    {
        for (auto* p : instance.getParameters())
            p->addListener (this);
    }

    ~ParamWatcher() override
    {
        for (auto* p : instance.getParameters())
            p->removeListener (this);
    }

    void parameterValueChanged (int, float) override {}

    void parameterGestureChanged (int index, bool starting) override
    {
        float value = 0.0f;
        if (auto* p = instance.getParameters()[index])
            value = p->getValue();

        MessageManager::callAsync ([e = engine, uid = slotUid, index, starting, value]
        {
            if (e != nullptr)
                e->gestureChanged (uid, index, starting, value);
        });
    }

    WeakReference<ChainEngine> engine;
    int slotUid;
    AudioPluginInstance& instance;
};

// One undo step for a hosted plug-in parameter change (a whole knob drag).
struct ChainEngine::ParamAction final : public UndoableAction
{
    ParamAction (ChainEngine& e, int uid, int idx, float from, float to)
        : engine (&e), slotUid (uid), index (idx), oldValue (from), newValue (to) {}

    bool perform() override { return apply (newValue); }
    bool undo() override    { return apply (oldValue); }
    int getSizeInUnits() override { return 1; }

    bool apply (float v)
    {
        if (engine != nullptr)
            if (auto* inst = engine->getInstance (slotUid))
                if (auto* p = inst->getParameters()[index])
                    p->setValueNotifyingHost (v);

        return true;
    }

    WeakReference<ChainEngine> engine;
    int slotUid, index;
    float oldValue, newValue;
};

//==============================================================================
ChainEngine::ChainEngine (Session& s, AudioProcessor& o) : session (s), owner (o)
{
    session.root.addListener (this);
    snapshot = std::make_unique<Snapshot>();
}

ChainEngine::~ChainEngine()
{
    session.root.removeListener (this);
    cancelPendingUpdate();

    {
        const SpinLock::ScopedLockType sl (snapshotLock);
        snapshot.reset();
    }

    for (auto& [uid, h] : hosted)
    {
        h.watcher.reset();
        h.instance->removeListener (this);
    }

    hosted.clear();
}

void ChainEngine::prepare (double sr, int bs)
{
    sampleRate = sr;
    blockSize = bs;
    scratch.setSize (32, bs, false, true, false);
    midi.ensureSize (2048);

    for (auto& [uid, h] : hosted)
        configure (*h.instance);
}

void ChainEngine::release()
{
    for (auto& [uid, h] : hosted)
        h.instance->releaseResources();
}

void ChainEngine::configure (AudioPluginInstance& p) const
{
    const double sr = sampleRate > 0 ? sampleRate : 48000.0;
    const int bs = blockSize > 0 ? blockSize : 512;

    p.disableNonMainBuses();
    auto layout = p.getBusesLayout();

    if (! layout.inputBuses.isEmpty())  layout.inputBuses.getReference (0)  = AudioChannelSet::stereo();
    if (! layout.outputBuses.isEmpty()) layout.outputBuses.getReference (0) = AudioChannelSet::stereo();

    p.setBusesLayout (layout);
    p.setRateAndBufferSizeDetails (sr, bs);
    p.prepareToPlay (sr, bs);
}

AudioPluginInstance* ChainEngine::getInstance (int slotUid) const
{
    auto it = hosted.find (slotUid);
    return it != hosted.end() ? it->second.instance.get() : nullptr;
}

String ChainEngine::getLoadError (int slotUid) const
{
    auto it = loadErrors.find (slotUid);
    return it != loadErrors.end() ? it->second : String();
}

//==============================================================================
void ChainEngine::process (AudioBuffer<float>& buffer, double posMs) noexcept
{
    const SpinLock::ScopedTryLockType sl (snapshotLock);

    if (! sl.isLocked() || snapshot == nullptr)
        return;

    if (posMs >= 0.0)
    {
        for (auto& lane : snapshot->lanes)
        {
            const auto& pts = lane.points;
            if (pts.empty() || lane.param == nullptr)
                continue;

            float v;
            if (posMs <= pts.front().first)      v = pts.front().second;
            else if (posMs >= pts.back().first)  v = pts.back().second;
            else
            {
                size_t i = 1;
                while (i < pts.size() && pts[i].first < posMs) ++i;
                const auto& a = pts[i - 1];
                const auto& b = pts[i];
                const double t = (posMs - a.first) / jmax (1.0e-9, b.first - a.first);
                v = (float) (a.second + (b.second - a.second) * t);
            }

            if (std::abs (v - lane.lastSent) > 1.0e-5f)
            {
                lane.param->setValue (v);
                lane.lastSent = v;
            }
        }
    }

    const int n = buffer.getNumSamples();
    const int bufCh = buffer.getNumChannels();

    for (auto* p : snapshot->chain)
    {
        midi.clear();
        const int need = jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());

        if (need <= bufCh)
        {
            AudioBuffer<float> view (buffer.getArrayOfWritePointers(), need, n);
            p->processBlock (view, midi);
        }
        else if (need <= scratch.getNumChannels() && n <= scratch.getNumSamples())
        {
            AudioBuffer<float> view (scratch.getArrayOfWritePointers(), need, n);
            view.clear();
            for (int c = 0; c < bufCh; ++c)
                view.copyFrom (c, 0, buffer, c, 0, n);

            p->processBlock (view, midi);

            for (int c = 0; c < bufCh; ++c)
                buffer.copyFrom (c, 0, view, c, 0, n);
        }
    }
}

//==============================================================================
void ChainEngine::handleAsyncUpdate()
{
    // 1. Work out which hosted instances no longer match the tree.
    std::vector<int> toRemove;

    for (auto& [uid, h] : hosted)
    {
        auto slot = session.slotByUid (uid);
        if (! slot.isValid() || slot[IDs::desc].toString() != h.descXml)
            toRemove.push_back (uid);
    }

    // 2. Pull them out of the map and out of the audio snapshot before deleting.
    std::vector<Hosted> doomed;

    for (auto uid : toRemove)
    {
        if (onBeforeInstanceRemoved)
            onBeforeInstanceRemoved (uid);

        auto it = hosted.find (uid);
        it->second.watcher.reset();
        it->second.instance->removeListener (this);
        doomed.push_back (std::move (it->second));
        hosted.erase (it);
    }

    rebuildSnapshot(); // swaps under the lock, so the audio thread is done with the old ones
    doomed.clear();

    // 3. Start loading anything the tree has that we don't.
    for (auto slot : session.slots())
    {
        const int uid = slot[IDs::uid];
        const auto desc = slot[IDs::desc].toString();

        if (desc.isNotEmpty() && hosted.count (uid) == 0)
        {
            auto l = loading.find (uid);
            if (l == loading.end() || l->second != desc)
                startLoad (uid, desc);
        }
    }

    updateLatency();

    if (onChanged)
        onChanged();
}

void ChainEngine::startLoad (int slotUid, const String& descXml)
{
    PluginDescription d;
    if (auto xml = parseXML (descXml))
        d.loadFromXml (*xml);

    loading[slotUid] = descXml;
    loadErrors.erase (slotUid);

    const double sr = sampleRate > 0 ? sampleRate : 48000.0;
    const int bs = blockSize > 0 ? blockSize : 512;

    catalog->formats.createPluginInstanceAsync (d, sr, bs,
        [self = WeakReference<ChainEngine> (this), slotUid, descXml] (std::unique_ptr<AudioPluginInstance> inst, const String& error)
    {
        if (self == nullptr)
            return;

        auto& e = *self;
        auto l = e.loading.find (slotUid);

        // Ignore results that are stale (slot changed again while loading).
        if (l == e.loading.end() || l->second != descXml)
            return;

        e.loading.erase (l);
        auto slot = e.session.slotByUid (slotUid);

        if (inst == nullptr || ! slot.isValid() || slot[IDs::desc].toString() != descXml)
        {
            if (inst == nullptr)
                e.loadErrors[slotUid] = error.isNotEmpty() ? error : String ("Could not load plug-in");

            if (e.onChanged) e.onChanged();
            return;
        }

        e.configure (*inst);

        MemoryBlock mb;
        if (mb.fromBase64Encoding (slot[IDs::state].toString()) && mb.getSize() > 0)
            inst->setStateInformation (mb.getData(), (int) mb.getSize());

        inst->addListener (&e);

        Hosted h;
        h.descXml = descXml;
        h.watcher = std::make_unique<ParamWatcher> (e, slotUid, *inst);
        h.instance = std::move (inst);
        e.hosted[slotUid] = std::move (h);

        e.rebuildSnapshot();
        e.updateLatency();

        if (e.onChanged) e.onChanged();
    });
}

void ChainEngine::rebuildSnapshot()
{
    auto snap = std::make_unique<Snapshot>();

    for (auto slot : session.slots())
        if ((bool) slot[IDs::enabled])
            if (auto* inst = getInstance (slot[IDs::uid]))
                snap->chain.push_back (inst);

    for (auto lane : session.lanes())
    {
        if (! (bool) lane[IDs::active])
            continue;

        auto* inst = getInstance (lane[IDs::slotUid]);
        if (inst == nullptr)
            continue;

        LaneRT rt;
        rt.param = inst->getParameters()[(int) lane[IDs::param]];

        for (auto pt : lane)
            rt.points.emplace_back ((double) pt[IDs::ms], (float) pt[IDs::value]);

        std::sort (rt.points.begin(), rt.points.end());
        snap->lanes.push_back (std::move (rt));
    }

    {
        const SpinLock::ScopedLockType sl (snapshotLock);
        std::swap (snapshot, snap);
    }
}

void ChainEngine::updateLatency()
{
    int sum = 0;

    for (auto slot : session.slots())
        if ((bool) slot[IDs::enabled])
            if (auto* inst = getInstance (slot[IDs::uid]))
                sum += inst->getLatencySamples();

    if (sum != totalLatency)
    {
        totalLatency = sum;
        owner.setLatencySamples (sum);
    }
}

void ChainEngine::audioProcessorChanged (AudioProcessor*, const ChangeDetails& details)
{
    if (details.latencyChanged)
        MessageManager::callAsync ([self = WeakReference<ChainEngine> (this)]
        {
            if (self != nullptr)
            {
                self->updateLatency();
                if (self->onChanged) self->onChanged();
            }
        });
}

//==============================================================================
void ChainEngine::loadPlugin (int slotUid, const PluginDescription& d)
{
    auto slot = session.slotByUid (slotUid);
    if (! slot.isValid())
        return;

    auto xml = d.createXml();
    session.undo.beginNewTransaction ("Load " + d.name);
    slot.setProperty (IDs::state, String(), &session.undo);
    slot.setProperty (IDs::name, d.name, &session.undo);
    slot.setProperty (IDs::enabled, true, &session.undo);
    slot.setProperty (IDs::desc, xml->toString (XmlElement::TextFormat().singleLine()), &session.undo);
}

void ChainEngine::removePlugin (int slotUid)
{
    auto slot = session.slotByUid (slotUid);
    if (! slot.isValid())
        return;

    // Keep the plug-in's current settings in the tree so undo brings it back exactly.
    if (auto* inst = getInstance (slotUid))
    {
        MemoryBlock mb;
        inst->getStateInformation (mb);
        slot.setProperty (IDs::state, mb.toBase64Encoding(), nullptr);
    }

    session.undo.beginNewTransaction ("Remove " + slot[IDs::name].toString());

    // Its automation lanes go too (same undo step).
    auto lanes = session.lanes();
    for (int i = lanes.getNumChildren(); --i >= 0;)
        if ((int) lanes.getChild (i)[IDs::slotUid] == slotUid)
            lanes.removeChild (i, &session.undo);

    slot.setProperty (IDs::desc, String(), &session.undo);
    slot.setProperty (IDs::name, String(), &session.undo);
}

void ChainEngine::setEnabled (int slotUid, bool on)
{
    auto slot = session.slotByUid (slotUid);
    if (! slot.isValid())
        return;

    session.undo.beginNewTransaction ((on ? "Enable " : "Disable ") + slot[IDs::name].toString());
    slot.setProperty (IDs::enabled, on, &session.undo);
}

void ChainEngine::moveSlot (int slotUid, int newIndex)
{
    auto slots = session.slots();
    auto slot = session.slotByUid (slotUid);
    const int from = slots.indexOf (slot);

    if (from < 0 || newIndex == from)
        return;

    session.undo.beginNewTransaction ("Move " + slot[IDs::name].toString());
    slots.moveChild (from, jlimit (0, slots.getNumChildren() - 1, newIndex), &session.undo);
}

void ChainEngine::storePluginStates()
{
    for (auto& [uid, h] : hosted)
    {
        MemoryBlock mb;
        h.instance->getStateInformation (mb);
        session.slotByUid (uid).setProperty (IDs::state, mb.toBase64Encoding(), nullptr);
    }
}

//==============================================================================
void ChainEngine::gestureChanged (int slotUid, int paramIndex, bool starting, float value)
{
    auto* inst = getInstance (slotUid);
    if (inst == nullptr)
        return;

    auto* p = inst->getParameters()[paramIndex];
    if (p == nullptr)
        return;

    const auto key = std::make_pair (slotUid, paramIndex);

    if (starting)
    {
        gestureStartValues[key] = value;
        session.undo.beginNewTransaction ("Change " + p->getName (64));

        // Touching a parameter gives it an automation lane (first touch only).
        if (! session.findLane (slotUid, paramIndex).isValid())
        {
            ValueTree lane { IDs::LANE };
            lane.setProperty (IDs::uid, session.newLaneUid(), nullptr);
            lane.setProperty (IDs::slotUid, slotUid, nullptr);
            lane.setProperty (IDs::param, paramIndex, nullptr);
            lane.setProperty (IDs::paramName, p->getName (64), nullptr);
            lane.setProperty (IDs::active, true, nullptr);
            session.lanes().appendChild (lane, &session.undo);
        }
    }
    else
    {
        auto it = gestureStartValues.find (key);
        const float from = it != gestureStartValues.end() ? it->second : value;
        gestureStartValues.erase (key);

        const float to = p->getValue();
        if (std::abs (to - from) > 1.0e-6f)
            session.undo.perform (new ParamAction (*this, slotUid, paramIndex, from, to));
    }
}
