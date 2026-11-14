#include "MainComponent.h"

MainComponent::MainComponent()
{
    juce::PropertiesFile::Options options;
    options.applicationName     = "ARAck";
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    juce::addDefaultFormatsToManager (formatManager);

    if (auto xml = appProperties.getUserSettings()->getXmlValue ("knownPlugins"))
        knownPlugins.recreateFromXml (*xml);

    knownPlugins.addChangeListener (this);

    auto deadMansPedal = appProperties.getUserSettings()->getFile()
                             .getSiblingFile ("ARAckScanCrashes.txt");

    pluginList = std::make_unique<juce::PluginListComponent> (formatManager, knownPlugins,
                                                              deadMansPedal,
                                                              appProperties.getUserSettings(), true);
    addAndMakeVisible (*pluginList);

    summary.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (summary);
    updateSummary();

    setSize (900, 600);
}

MainComponent::~MainComponent()
{
    knownPlugins.removeChangeListener (this);
    saveKnownPlugins();
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (8);
    summary.setBounds (area.removeFromTop (28));
    area.removeFromTop (4);
    pluginList->setBounds (area);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateSummary();
    saveKnownPlugins();
}

void MainComponent::updateSummary()
{
    const auto types = knownPlugins.getTypes();

    juce::StringArray araNames;
    for (const auto& t : types)
        if (t.hasARAExtension)
            araNames.add (t.name);

    summary.setText ("ARAck: " + juce::String (types.size()) + " plug-ins known, "
                         + juce::String (araNames.size()) + " with ARA"
                         + (araNames.isEmpty() ? juce::String() : ": " + araNames.joinIntoString (", ")),
                     juce::dontSendNotification);
}

void MainComponent::saveKnownPlugins()
{
    if (auto xml = knownPlugins.createXml())
        appProperties.getUserSettings()->setValue ("knownPlugins", xml.get());

    appProperties.saveIfNeeded();
}
