cmake_minimum_required(VERSION 3.22)
project(MiuglitchAudioTools VERSION 0.2.0 LANGUAGES C CXX)

# One build for everything:
#   dsp/      -> GlitchCore static library (pure C++17, no JUCE)
#   plugins/  -> one JUCE plugin per folder, each linking GlitchCore
#
# Typical Windows configure:
#   cmake -B build -DJUCE_PATH=D:/SDKs/JUCE
#   cmake --build build --config Release

option(MIUGLITCH_BUILD_PLUGINS "Build the JUCE plugins in plugins/" ON)
option(MIUGLITCH_BUILD_TESTS   "Build the offline DSP tests in tests/" OFF)
option(MIUGLITCH_COPY_PLUGINS  "Copy built plugins to the system plugin folder" ON)

set(MIUGLITCH_JUCE_VERSION "8.0.13" CACHE STRING "JUCE tag to download when JUCE_PATH is empty")
set(JUCE_PATH "" CACHE PATH "Local JUCE checkout, e.g. D:/SDKs/JUCE (empty = download JUCE ${MIUGLITCH_JUCE_VERSION})")

add_subdirectory(dsp)

if (MIUGLITCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (MIUGLITCH_BUILD_PLUGINS)
    if (JUCE_PATH)
        add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${MIUGLITCH_JUCE_VERSION}
            GIT_SHALLOW    ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    include(cmake/MiuglitchPlugin.cmake)
    add_subdirectory(plugins/shared)
    add_subdirectory(plugins/disintegrate)
    add_subdirectory(plugins/smudge)
    add_subdirectory(plugins/polychromatic)
    add_subdirectory(plugins/particulate)
endif()
