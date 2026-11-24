# MiuglitchShared: small JUCE-side helpers shared by plugins
# (restored from the original MiuglitchShared library, commit bc834dc).
#
# An INTERFACE library: its sources compile inside each plugin that links it,
# against that plugin's own JUCE modules, so JUCE is never built twice.

add_library(MiuglitchShared INTERFACE)
add_library(Miuglitch::Shared ALIAS MiuglitchShared)

target_sources(MiuglitchShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ParameterSmoother.cpp)
target_include_directories(MiuglitchShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
