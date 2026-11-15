#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

using namespace juce;

ARAckProcessor::ARAckProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true))
{
    startTimer (250);
}

ARAckProcessor::~ARAckProcessor()
{
    stopTimer();
}

void ARAckProcessor::timerCallback()
{
    int latency = localModel.chain.getTotalLatencySamples();

    if (auto* r = getPlaybackRenderer<ARAckPlaybackRenderer>())
        latency = r->getChainLatencySamples();

    if (latency != getLatencySamples())
        setLatencySamples (latency);
}

void ARAckProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    localModel.chain.prepare (sampleRate, samplesPerBlock);

    int regions = 0;
    if (auto* r = getPlaybackRenderer<ARAckPlaybackRenderer>())
        regions = (int) r->getPlaybackRegions().size();

    arackLog ("Prepare: " + String (sampleRate) + " Hz, block " + String (samplesPerBlock)
              + ", ARA playback renderer " + String (isPlaybackRenderer() ? "yes" : "no")
              + ", editor renderer " + String (isEditorRenderer() ? "yes" : "no")
              + ", editor view " + String (isEditorView() ? "yes" : "no")
              + ", " + String (regions) + " clip(s) assigned");
}

void ARAckProcessor::releaseResources()
{
    releaseResourcesForARA();
    localModel.chain.release();
}

bool ARAckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == out;
}

double ARAckProcessor::getTailLengthSeconds() const
{
    double tail = 0.0;
    getTailLengthSecondsForARA (tail);
    return tail;
}

void ARAckProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* ph = getPlayHead();
    const auto pos = ph != nullptr ? ph->getPosition() : Optional<AudioPlayHead::PositionInfo>();

    const bool playing = pos.hasValue() && pos->getIsPlaying();
    hostPlaying.store (playing);
    if (pos.hasValue())
        hostTimeSeconds.store (pos->getTimeInSeconds().orFallback (0.0));

    if (isPlaybackRenderer())
    {
        // ARA mode: the renderer reads each clip and runs it through that clip's CHAIN.
        processBlockForARA (buffer, isRealtime(), ph);
        return;
    }

    if (isEditorRenderer() || isEditorView())
    {
        buffer.clear(); // ARA helper copies of ARAck don't make sound
        return;
    }

    // Plain VST3: process the track input; automation follows host time.
    const double posMs = playing ? hostTimeSeconds.load() * 1000.0 : -1.0;
    localModel.playPositionMs.store (posMs);
    localModel.chain.process (buffer, posMs);
}

void ARAckProcessor::getStateInformation (MemoryBlock& dest)
{
    if (! MessageManager::existsAndIsCurrentThread())
    {
        // Hosts normally ask on the message thread; if not, save the tree as it is.
        if (auto xml = localModel.session.root.createXml())
            copyXmlToBinary (*xml, dest);
        return;
    }

    localModel.chain.storePluginStates();

    if (auto xml = localModel.session.root.createXml())
        copyXmlToBinary (*xml, dest);
}

void ARAckProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = ValueTree::fromXml (*xml);

        if (MessageManager::existsAndIsCurrentThread())
            localModel.session.replaceWith (tree);
        else
            MessageManager::callAsync ([safe = WeakReference<ARAckProcessor> (this), tree]
            {
                if (safe != nullptr)
                    safe->localModel.session.replaceWith (tree);
            });
    }
}

AudioProcessorEditor* ARAckProcessor::createEditor()
{
    return new ARAckEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ARAckProcessor();
}
