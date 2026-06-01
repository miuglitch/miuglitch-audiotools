cmake_minimum_required(VERSION 3.22)
project(SmudgeVST VERSION 0.1.0)

# Path to JUCE — matches your setup
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# Path to GlitchCore relative to this CMakeLists
set(GLITCHCORE_PATH "D:/MIUGLITCH PLUGINS/glitchcore" CACHE PATH "Path to GlitchCore")

juce_add_plugin(Smudge
    COMPANY_NAME            "MIUGLITCH"
    PLUGIN_MANUFACTURER_CODE Miug
    PLUGIN_CODE             SMUD
    FORMATS                 VST3
    PRODUCT_NAME            "Smudge"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Fx"
    VST3_CAN_REPLACE_VST2   FALSE
)

target_sources(Smudge PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.cpp
    source/PluginEditor.h
    ${GLITCHCORE_PATH}/dsp/smudge/smudge.cpp
    ${GLITCHCORE_PATH}/dsp/smudge/smudge.h
)

target_include_directories(Smudge PRIVATE
    source
    ${GLITCHCORE_PATH}
)

target_compile_features(Smudge PRIVATE cxx_std_17)

target_compile_definitions(Smudge PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Smudge PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Auto-install VST3 to system folder after build
set(VST3_INSTALL_DIR "C:/Program Files/Common Files/VST3")
add_custom_command(TARGET Smudge_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<TARGET_FILE_DIR:Smudge_VST3>/../../../Smudge.vst3"
        "${VST3_INSTALL_DIR}/Smudge.vst3"
    COMMENT "Installing Smudge.vst3 to ${VST3_INSTALL_DIR}"
)
