# GlitchCore: every plugin's DSP, pure C++17 with no JUCE dependency.
# Headers are included by their path from the repo root, e.g.
#   #include "dsp/smudge/smudge.h"

add_library(GlitchCore STATIC
    disintegrate/disintegrate.cpp
    disintegrate/disintegrate.h
    smudge/smudge.cpp
    smudge/smudge.h
    polychromatic/polychromatic.cpp
    polychromatic/polychromatic.h
    polychromatic/scales.h
    polychromatic/fft.h
)

target_include_directories(GlitchCore PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(GlitchCore PUBLIC cxx_std_17)

# Plugins are shared libraries, so the DSP must be position independent
set_target_properties(GlitchCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
