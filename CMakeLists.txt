cmake_minimum_required(VERSION 3.22)
project(GlitchCore VERSION 0.1.0)

# Collect all DSP source files
add_library(GlitchCore STATIC
    dsp/disintegrate/Disintegrate.cpp
)

# Where to find the header files
target_include_directories(GlitchCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Require C++17
target_compile_features(GlitchCore
    PUBLIC
        cxx_std_17
)