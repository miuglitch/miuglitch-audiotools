cmake_minimum_required(VERSION 3.22)
project(Disintegrate VERSION 0.1.0)

if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
endif()

if(NOT DEFINED GLITCHCORE_PATH)
    set(GLITCHCORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../glitchcore" CACHE PATH "Path to GlitchCore")
endif()

add_subdirectory(${JUCE_PATH} JUCE-build EXCLUDE_FROM_ALL)
add_subdirectory(${GLITCHCORE_PATH} GlitchCore-build)

juce_add_plugin(Disintegrate
    COMPANY_NAME "MIUGLITCH"
    PLUGIN_MANUFACTURER_CODE Miug
    PLUGIN_CODE Dsin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Disintegrate"
)

target_sources(Disintegrate PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
)

target_include_directories(Disintegrate
    PRIVATE
        ${GLITCHCORE_PATH}/dsp/disintegrate
)

target_compile_definitions(Disintegrate
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Disintegrate
    PRIVATE
        GlitchCore
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)