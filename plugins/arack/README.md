# ARAck

An ARA2-capable VST3 host with utility processing and automation, built on JUCE.

## Requirements

- JUCE 8 at `C:\JUCE` (override with `-DARACK_JUCE_DIR=...`)
- Celemony ARA SDK at `C:\SDKs\ARA_SDK` (override with `-DARACK_ARA_SDK_DIR=...`)
  `git clone --recursive https://github.com/Celemony/ARA_SDK.git C:\SDKs\ARA_SDK`
- Visual Studio 2026 with the C++ desktop workload

## Build

Build output goes to `C:\Builds\ARAck`, outside Dropbox.

    cmake --preset vs2026
    cmake --build --preset debug

The app ends up in `C:\Builds\ARAck\ARAck_artefacts\Debug\ARAck.exe`.
