#pragma once

#include "ChainEngine.h"

// Everything ARAck holds for one clip: its CHAIN, automation and undo history.
// In an ARA host it lives on the ARA audio modification, so the copy of ARAck that
// plays the audio and the copy that shows the window share it. As a plain VST3,
// the processor owns one.
struct ClipModel
{
    ClipModel() = default;

    Session session;
    ChainEngine chain { session };

    // Written by the audio side, read by the UI.
    std::atomic<double> playPositionMs { -1.0 }; // -1 = not playing inside this clip

    int selectedSlotUid = 0; // UI state, not part of undo

    JUCE_DECLARE_NON_COPYABLE (ClipModel)
};
