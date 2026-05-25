# juce-shared-modules

Reusable C++ library for [Miuglitch](https://github.com/miuglitch) JUCE audio plugins.

## Contents

- **MiuglitchShared** — CMake static library (`Miuglitch::Shared`)
- **miuglitch::ParameterSmoother** — linear ramp for automation-friendly parameters

## Usage in a plugin

Your plugin `CMakeLists.txt` must load JUCE first, then this repo:

```cmake
add_subdirectory(${JUCE_PATH} JUCE-build EXCLUDE_FROM_ALL)
add_subdirectory(modules/juce-shared-modules)

juce_add_plugin(MyPlugin ...)

target_link_libraries(MyPlugin
    PRIVATE
        Miuglitch::Shared
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

As a git submodule:

```bash
git submodule add https://github.com/miuglitch/juce-shared-modules.git modules/juce-shared-modules
```

## Requirements

- CMake 3.22+
- JUCE 7+ (parent project provides `juce::` targets)
