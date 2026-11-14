#pragma once

#include <JuceHeader.h>

// Starter window: scans for VST3 plug-ins and reports which ones expose ARA.
class MainComponent final : public juce::Component,
                            private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateSummary();
    void saveKnownPlugins();

    juce::ApplicationProperties appProperties;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    juce::Label summary;
    std::unique_ptr<juce::PluginListComponent> pluginList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
