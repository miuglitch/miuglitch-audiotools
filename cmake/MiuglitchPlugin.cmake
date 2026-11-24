# miuglitch_add_plugin(<Target>
#     NAME    "Product Name"
#     CODE    Abcd                 # 4-char plugin code; never change after release
#     FORMATS VST3 [Standalone]    # default: VST3
#     [MIDI_INPUT]                 # plugin receives MIDI
#     [CATEGORIES "Fx" ...]        # VST3 categories, default "Fx"
#     SOURCES source/....cpp ...)
#
# Shared settings for every Miuglitch plugin: company name, manufacturer
# code, warning flags, GlitchCore linkage and copy-after-build.

function(miuglitch_add_plugin target)
    cmake_parse_arguments(P "MIDI_INPUT" "NAME;CODE" "FORMATS;CATEGORIES;SOURCES" ${ARGN})

    if (NOT P_FORMATS)
        set(P_FORMATS VST3)
    endif()
    if (NOT P_CATEGORIES)
        set(P_CATEGORIES "Fx")
    endif()
    if (P_MIDI_INPUT)
        set(needs_midi TRUE)
    else()
        set(needs_midi FALSE)
    endif()

    juce_add_plugin(${target}
        COMPANY_NAME                "MIUGLITCH"
        PLUGIN_MANUFACTURER_CODE    Miug
        PLUGIN_CODE                 ${P_CODE}
        PRODUCT_NAME                "${P_NAME}"
        FORMATS                     ${P_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            ${needs_midi}
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES             ${P_CATEGORIES}
        VST3_CAN_REPLACE_VST2       FALSE
        COPY_PLUGIN_AFTER_BUILD     ${MIUGLITCH_COPY_PLUGINS})

    target_sources(${target} PRIVATE ${P_SOURCES})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
    target_compile_features(${target} PRIVATE cxx_std_17)

    target_compile_definitions(${target} PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(${target}
        PRIVATE
            GlitchCore
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
