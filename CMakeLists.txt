cmake_minimum_required(VERSION 3.22)

# Parent project must call add_subdirectory(JUCE) before adding this directory.

add_library(MiuglitchShared STATIC
    src/ParameterSmoother.cpp
)

add_library(Miuglitch::Shared ALIAS MiuglitchShared)

target_include_directories(MiuglitchShared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(MiuglitchShared PUBLIC cxx_std_17)

target_link_libraries(MiuglitchShared
    PUBLIC
        juce::juce_audio_basics
        juce::juce_dsp
)
