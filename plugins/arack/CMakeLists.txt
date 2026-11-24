# ARAck: an ARA2 VST3 plug-in that hosts a chain of up to 12 VST3 plug-ins.
# Released under Type 2 Studio (not MIUGLITCH), so it keeps its own
# juce_add_plugin call and codes instead of miuglitch_add_plugin().
# Needs the Celemony ARA SDK: -DARA_SDK_PATH=D:/SDKs/ARA_SDK

juce_set_ara_sdk_path("${ARA_SDK_PATH}")

if (WIN32)
    set(ARACK_VST3_COPY_DIR "C:/Program Files/Common Files/VST3/ARAck" CACHE PATH "Where each ARAck VST3 build is installed")
    set(arack_copy_dir VST3_COPY_DIR "${ARACK_VST3_COPY_DIR}")
endif()

juce_add_plugin(ARAck
    PRODUCT_NAME                "ARAck"
    COMPANY_NAME                "Type 2 Studio"
    BUNDLE_ID                   "com.type2studio.arack"
    PLUGIN_MANUFACTURER_CODE    Typ2
    PLUGIN_CODE                 Arck
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_ARA_EFFECT               TRUE
    ARA_TRANSFORMATION_FLAGS    kARAPlaybackTransformationNoChanges
    VST3_CATEGORIES             Fx Tools
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     ${MIUGLITCH_COPY_PLUGINS}
    ${arack_copy_dir})

juce_add_binary_data(ARAckData SOURCES resources/miu-logo.svg)

target_sources(ARAck PRIVATE
    source/ARADocument.cpp
    source/ChainEngine.cpp
    source/PluginProcessor.cpp
    source/UI/PluginEditor.cpp
    source/UI/AutomationPanel.cpp
    source/UI/ChainPanel.cpp)

target_compile_features(ARAck PRIVATE cxx_std_20)

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
