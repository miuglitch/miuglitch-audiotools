cmake_minimum_required(VERSION 3.22)

project(ARAck VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External SDKs live outside the (Dropbox-synced) project folder.
set(ARACK_JUCE_DIR    "C:/JUCE"         CACHE PATH "Path to the JUCE source tree")
set(ARACK_ARA_SDK_DIR "C:/SDKs/ARA_SDK" CACHE PATH "Path to the Celemony ARA SDK")

add_subdirectory("${ARACK_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
juce_set_ara_sdk_path("${ARACK_ARA_SDK_DIR}")

juce_add_gui_app(ARAck
    PRODUCT_NAME "ARAck"
    COMPANY_NAME "ARAck")

juce_generate_juce_header(ARAck)

target_sources(ARAck PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp)

target_compile_definitions(ARAck PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ARAck,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ARAck,JUCE_VERSION>"
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_ARA=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ARAck
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Final app/plug-in binaries are copied into the project folder (Dropbox) under Bin/<Config>.
# Intermediate build files stay in the binary dir outside Dropbox.
set(ARACK_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Bin" CACHE PATH "Where finished binaries are copied")

add_custom_command(TARGET ARAck POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARACK_OUTPUT_DIR}/$<CONFIG>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:ARAck>" "${ARACK_OUTPUT_DIR}/$<CONFIG>/"
    VERBATIM)
