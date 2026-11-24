# Polychromatic — JUCE wrapper around GlitchCore::Polychromatic.
# Built from the glitchcore root with -DGLITCHCORE_BUILD_PLUGINS=ON
# and -DJUCE_PATH=<path to JUCE>.

if (NOT TARGET juce::juce_audio_processors)
    set(JUCE_PATH "" CACHE PATH "Path to JUCE")
    if (NOT JUCE_PATH)
        message(FATAL_ERROR "Set JUCE_PATH to your JUCE checkout, e.g. -DJUCE_PATH=C:/JUCE")
    endif()
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
endif()

option(GLITCHCORE_COPY_PLUGINS "Copy built plugins to the system plugin folder" OFF)

juce_add_plugin(Polychromatic
    COMPANY_NAME             "MIUGLITCH"
    PLUGIN_MANUFACTURER_CODE Miug
    PLUGIN_CODE              Plch
    FORMATS                  VST3
    PRODUCT_NAME             "Polychromatic"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          "Fx" "Spectral" "Pitch Shift"
    VST3_CAN_REPLACE_VST2    FALSE
    COPY_PLUGIN_AFTER_BUILD  ${GLITCHCORE_COPY_PLUGINS}
)

target_sources(Polychromatic PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.cpp
    source/PluginEditor.h
)

target_include_directories(Polychromatic PRIVATE source)

target_compile_features(Polychromatic PRIVATE cxx_std_17)

target_compile_definitions(Polychromatic PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(Polychromatic
    PRIVATE
        GlitchCore
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
