cmake_minimum_required(VERSION 3.22)
project(Disintegrate VERSION 0.1.0)

# Where is JUCE on your machine?
if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
endif()

# Where is GlitchCore?
if(NOT DEFINED GLITCHCORE_PATH)
    set(GLITCHCORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../glitchcore" CACHE PATH "Path to GlitchCore")
endif()

# Pull in JUCE and GlitchCore
add_subdirectory(${JUCE_PATH} JUCE-build EXCLUDE_FROM_ALL)
add_subdirectory(${GLITCHCORE_PATH} GlitchCore-build)

# Define the plugin
juce_add_plugin(Disintegrate
    COMPANY_NAME "Miuglitch"
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

# Our source files
target_sources(Disintegrate PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
)

# Link to JUCE and GlitchCore
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