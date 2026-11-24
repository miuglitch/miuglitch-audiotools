# ARAck

An ARA2 VST3 plug-in that hosts a chain of up to 12 VST3 plug-ins, with full disable
(no latency), per-parameter automation over the clip, and full undo/redo. Built on JUCE.

See `docs/arack/Requirements.md` for what it must do and `docs/arack/DesignLanguage.md` for the look.
The chosen layout is `docs/arack/mockups/arack-4b-compact`.

## Build

ARAck builds with the rest of the repo once the ARA SDK path is set (see the root README):

    cmake -B build -DJUCE_PATH=D:/SDKs/JUCE -DARA_SDK_PATH=D:/SDKs/ARA_SDK
    cmake --build build --config Release --target ARAck_All

Each build installs `ARAck.vst3` to `C:\Program Files\Common Files\VST3\ARAck\`
(turn off with `-DMIUGLITCH_COPY_PLUGINS=OFF`). The standalone app is in
`build/plugins/arack/ARAck_artefacts/Release/Standalone/`.

## Code map

- `source/Session.h` – the whole document as one ValueTree + the UndoManager. Every edit goes
  through it, which is what makes undo/redo complete.
- `source/ChainEngine.*` – hosts the slot plug-ins, mirrors the tree, builds a lock-protected
  snapshot for the audio thread, sums latency of enabled slots, plays automation, turns
  parameter gestures into undo steps and automation lanes. Also the shared plug-in catalogue.
- `source/ARADocument.*` – ARA document controller and playback renderer (reads the clip audio).
- `source/PluginProcessor.*` – the plug-in: ARA render, then the chain.
- `source/UI/` – header, CHAIN panel, plug-in panel, automation panel, look and feel.
