#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// All of ARAck's document state lives in one ValueTree. Every user edit goes through
// Session's UndoManager, so undo/redo covers everything by construction.
namespace IDs
{
   #define ARACK_ID(name) inline const juce::Identifier name { #name };
    ARACK_ID (ARACK)
    ARACK_ID (SLOTS)
    ARACK_ID (SLOT)
    ARACK_ID (LANES)
    ARACK_ID (LANE)
    ARACK_ID (POINT)
    ARACK_ID (uid)
    ARACK_ID (desc)       // PluginDescription as XML text; empty = no plug-in
    ARACK_ID (name)
    ARACK_ID (state)      // hosted plug-in state, base64
    ARACK_ID (enabled)
    ARACK_ID (slotUid)
    ARACK_ID (param)
    ARACK_ID (paramName)
    ARACK_ID (colour)
    ARACK_ID (active)     // lane shown and applied
    ARACK_ID (ms)
    ARACK_ID (value)
    ARACK_ID (nextUid)
   #undef ARACK_ID
}

class Session
{
public:
    static constexpr int numSlots = 12;

    Session()
    {
        root = createEmpty();
    }

    static juce::ValueTree createEmpty()
    {
        juce::ValueTree r { IDs::ARACK };
        juce::ValueTree slots { IDs::SLOTS };

        for (int i = 0; i < numSlots; ++i)
        {
            juce::ValueTree s { IDs::SLOT };
            s.setProperty (IDs::uid, i + 1, nullptr);
            s.setProperty (IDs::desc, juce::String(), nullptr);
            s.setProperty (IDs::enabled, true, nullptr);
            slots.appendChild (s, nullptr);
        }

        r.appendChild (slots, nullptr);
        r.appendChild (juce::ValueTree { IDs::LANES }, nullptr);
        r.setProperty (IDs::nextUid, 1, nullptr);
        return r;
    }

    juce::ValueTree slots() const { return root.getChildWithName (IDs::SLOTS); }
    juce::ValueTree lanes() const { return root.getChildWithName (IDs::LANES); }

    juce::ValueTree slotByUid (int uid) const
    {
        return slots().getChildWithProperty (IDs::uid, uid);
    }

    juce::ValueTree findLane (int slotUid, int paramIndex) const
    {
        for (auto l : lanes())
            if ((int) l[IDs::slotUid] == slotUid && (int) l[IDs::param] == paramIndex)
                return l;

        return {};
    }

    int newLaneUid()
    {
        const int id = root[IDs::nextUid];
        root.setProperty (IDs::nextUid, id + 1, nullptr);
        return id;
    }

    // Replace the whole document (state restore). Clears undo history.
    void replaceWith (const juce::ValueTree& newRoot)
    {
        if (! newRoot.hasType (IDs::ARACK))
            return;

        root.copyPropertiesAndChildrenFrom (newRoot, nullptr);

        // Make sure there are always exactly 12 slots, whatever was saved.
        auto s = slots();
        while (s.getNumChildren() < numSlots)
        {
            juce::ValueTree slot { IDs::SLOT };
            int maxUid = 0;
            for (auto c : s) maxUid = juce::jmax (maxUid, (int) c[IDs::uid]);
            slot.setProperty (IDs::uid, maxUid + 1, nullptr);
            slot.setProperty (IDs::desc, juce::String(), nullptr);
            slot.setProperty (IDs::enabled, true, nullptr);
            s.appendChild (slot, nullptr);
        }

        undo.clearUndoHistory();
    }

    juce::ValueTree root;
    juce::UndoManager undo { 0, 200 };
};
