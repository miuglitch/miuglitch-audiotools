# Miuglitch Audio Tools

Every Miuglitch plugin in one repo. The DSP for all plugins lives in one shared
library, **GlitchCore**, and each plugin is a thin JUCE wrapper that builds its
own VST3.

| Plugin | Folder | Code | Formats | Status |
| --- | --- | --- | --- | --- |
| Disintegrate | `plugins/disintegrate` | `Dsin` | VST3, Standalone | free |
| Smudge | `plugins/smudge` | `SMUD` | VST3 | free, WIP |
| Polychromatic | `plugins/polychromatic` | `Plch` | VST3 (MIDI/MPE in) | in progress |
| Particulate | `plugins/particulate` | `Part` | VST3, Standalone | template (gain only) |

Manufacturer code is `Miug` for all of them. **Never change a plugin code after
release**: hosts use it to find the plugin in saved projects.

## Layout

```
CMakeLists.txt          one build for everything
cmake/                  miuglitch_add_plugin() and other build helpers
dsp/                    GlitchCore: pure C++17 DSP, no JUCE, namespace GlitchCore
  <plugin>/             one folder per plugin's DSP
plugins/                JUCE wrappers, one folder per plugin
  <plugin>/source/      PluginProcessor / PluginEditor
  shared/               JUCE-side helpers shared by plugins (ParameterSmoother)
tests/                  offline DSP tests (no JUCE needed)
docs/                   diagrams and UI mock-ups, one folder per plugin
```

Rules of thumb:

- No JUCE in `dsp/`. If it needs JUCE, it goes in `plugins/`.
- Include DSP headers by their path from the repo root: `#include "dsp/smudge/smudge.h"`.
- A new plugin = `dsp/<name>/` + `plugins/<name>/` + one `add_subdirectory` line
  in the root `CMakeLists.txt`.

## Build

Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows).

```
cmake -B build -DJUCE_PATH=D:/SDKs/JUCE
cmake --build build --config Release
```

Leave out `JUCE_PATH` and CMake downloads JUCE (`MIUGLITCH_JUCE_VERSION`, default
8.0.13, the same as `D:\SDKs\JUCE`) instead. Keep the two in step when you upgrade JUCE.

Options:

| Option | Default | What it does |
| --- | --- | --- |
| `JUCE_PATH` | empty | Local JUCE checkout |
| `MIUGLITCH_BUILD_PLUGINS` | ON | Build the JUCE plugins |
| `MIUGLITCH_COPY_PLUGINS` | ON | Copy each built plugin to the system plugin folder |
| `MIUGLITCH_BUILD_TESTS` | OFF | Build `tests/`; run with `ctest --test-dir build -C Release` |

## History

This repo merges `glitchcore`, `smudge-vst` and `disintegrate-vst` with their
full git history (`git log --follow` works on every file), plus Polychromatic
from glitchcore's `spectral-harmonizer` branch and Particulate from a local,
never-committed folder.
