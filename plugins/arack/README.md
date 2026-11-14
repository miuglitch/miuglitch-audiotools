# ARAck

An ARA2 VST3 plug-in that hosts a chain of up to 12 VST3 plug-ins, with full disable
(no latency), per-parameter automation over the clip, and full undo/redo. Built on JUCE.

See `Docs/Requirements.md` for what it must do and `Docs/DesignLanguage.md` for the look.
The chosen layout is `Mockups/arack-4b-compact`.

## Requirements

- JUCE 8 at `C:\JUCE` (override with `-DARACK_JUCE_DIR=...`)
- Celemony ARA SDK at `C:\SDKs\ARA_SDK` (override with `-DARACK_ARA_SDK_DIR=...`)
- Visual Studio 2026 with the C++ desktop workload

## Build

    cmake --preset vs2026
    cmake --build --preset release

Each build installs `ARAck.vst3` to `C:\Program Files\Common Files\VST3\ARAck\` and copies
the VST3 and the standalone app to `Bin\<Config>\` (Dropbox, not in git). Intermediate files
stay in `C:\Builds\ARAck`.

## Code map

- `Source/Session.h` – the whole document as one ValueTree + the UndoManager. Every edit goes
  through it, which is what makes undo/redo complete.
- `Source/ChainEngine.*` – hosts the slot plug-ins, mirrors the tree, builds a lock-protected
  snapshot for the audio thread, sums latency of enabled slots, plays automation, turns
  parameter gestures into undo steps and automation lanes. Also the shared plug-in catalogue.
- `Source/ARADocument.*` – ARA document controller and playback renderer (reads the clip audio).
- `Source/PluginProcessor.*` – the plug-in: ARA render, then the chain.
- `Source/UI/` – header, CHAIN panel, plug-in panel, automation panel, look and feel.
