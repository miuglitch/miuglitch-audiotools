cmake_minimum_required(VERSION 3.22)

project(ARAck VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External SDKs live outside the (Dropbox-synced) project folder.
set(ARACK_JUCE_DIR    "C:/JUCE"         CACHE PATH "Path to the JUCE source tree")
set(ARACK_ARA_SDK_DIR "C:/SDKs/ARA_SDK" CACHE PATH "Path to the Celemony ARA SDK")
set(ARACK_VST3_INSTALL_DIR "C:/Program Files/Common Files/VST3/ARAck" CACHE PATH "Where each VST3 build is installed")

add_subdirectory("${ARACK_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
juce_set_ara_sdk_path("${ARACK_ARA_SDK_DIR}")

juce_add_plugin(ARAck
    PRODUCT_NAME "ARAck"
    COMPANY_NAME "Type 2 Studio"
    BUNDLE_ID "com.type2studio.arack"
    PLUGIN_MANUFACTURER_CODE Typ2
    PLUGIN_CODE Arck
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_ARA_EFFECT TRUE
    ARA_TRANSFORMATION_FLAGS kARAPlaybackTransformationNoChanges
    VST3_CATEGORIES Fx Tools
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "${ARACK_VST3_INSTALL_DIR}")

juce_add_binary_data(ARAckData SOURCES Resources/miu-logo.svg)

target_sources(ARAck PRIVATE
    Source/ARADocument.cpp
    Source/ChainEngine.cpp
    Source/PluginProcessor.cpp
    Source/UI/PluginEditor.cpp
    Source/UI/AutomationPanel.cpp
    Source/UI/ChainPanel.cpp)

target_compile_definitions(ARAck PUBLIC
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(ARAck
    PRIVATE
        ARAckData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Finished binaries are also copied into the project folder (Dropbox) under Bin/<Config>.
# Intermediate build files stay in the binary dir outside Dropbox.
set(ARACK_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Bin" CACHE PATH "Where finished binaries are copied")

add_custom_command(TARGET ARAck_VST3 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_FILE_DIR:ARAck_VST3>/../.."
            "${ARACK_OUTPUT_DIR}/$<CONFIG>/ARAck.vst3"
    VERBATIM)

add_custom_command(TARGET ARAck_Standalone POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARACK_OUTPUT_DIR}/$<CONFIG>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:ARAck_Standalone>" "${ARACK_OUTPUT_DIR}/$<CONFIG>/"
    VERBATIM)
