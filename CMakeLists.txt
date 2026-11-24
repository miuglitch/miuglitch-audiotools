cmake_minimum_required(VERSION 3.22)
project(GlitchCore VERSION 0.1.0)

option(GLITCHCORE_BUILD_PLUGINS "Build the JUCE plugin wrappers in plugins/ (needs JUCE_PATH)" OFF)
option(GLITCHCORE_BUILD_TESTS   "Build the offline DSP tests in tests/" OFF)

# Collect all DSP source files
add_library(GlitchCore STATIC
    dsp/disintegrate/disintegrate.cpp
    dsp/polychromatic/polychromatic.cpp
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

# Plugin wrappers link into a shared library, so the DSP must be PIC
set_target_properties(GlitchCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (GLITCHCORE_BUILD_TESTS)
    enable_testing()
    add_executable(polychromatic_test tests/polychromatic_test.cpp)
    target_link_libraries(polychromatic_test PRIVATE GlitchCore)
    add_test(NAME polychromatic_test COMMAND polychromatic_test)
endif()

if (GLITCHCORE_BUILD_PLUGINS)
    add_subdirectory(plugins/polychromatic)
endif()
