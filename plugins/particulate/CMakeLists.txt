miuglitch_add_plugin(Particulate
    NAME    "Particulate"
    CODE    Part
    FORMATS VST3 Standalone
    SOURCES
        source/PluginProcessor.cpp
        source/PluginProcessor.h
        source/PluginEditor.cpp
        source/PluginEditor.h)

target_link_libraries(Particulate PRIVATE Miuglitch::Shared)
