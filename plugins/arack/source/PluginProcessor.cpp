#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

using namespace juce;

ARAckProcessor::ARAckProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true))
{
}

ARAckProcessor::~ARAckProcessor() = default;

void ARAckProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    chain.prepare (sampleRate, samplesPerBlock);

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
    chain.release();
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

    double posMs = -1.0;

    if (isPlaybackRenderer())
    {
        // ARA mode: the clip audio comes from the host's audio source.
        processBlockForARA (buffer, isRealtime(), ph);

        if (auto* r = getPlaybackRenderer<ARAckPlaybackRenderer>())
        {
            const auto s = r->clipPositionSamples.load();
            if (s != std::numeric_limits<int64>::min() && getSampleRate() > 0)
                posMs = (double) s / getSampleRate() * 1000.0;
        }
    }
    else if (playing)
    {
        // Plain VST3 mode: process the track input, automation follows host time.
        posMs = hostTimeSeconds.load() * 1000.0;
    }

    clipPositionMs.store (playing ? posMs : -1.0);
    chain.process (buffer, playing ? posMs : -1.0);
}

void ARAckProcessor::getStateInformation (MemoryBlock& dest)
{
    if (! MessageManager::existsAndIsCurrentThread())
    {
        // Hosts normally ask on the message thread; if not, save the tree as it is.
        if (auto xml = session.root.createXml())
            copyXmlToBinary (*xml, dest);
        return;
    }

    chain.storePluginStates();

    if (auto xml = session.root.createXml())
        copyXmlToBinary (*xml, dest);
}

void ARAckProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = ValueTree::fromXml (*xml);

        if (MessageManager::existsAndIsCurrentThread())
            session.replaceWith (tree);
        else
            MessageManager::callAsync ([safe = WeakReference<ARAckProcessor> (this), tree]
            {
                if (safe != nullptr)
                    safe->session.replaceWith (tree);
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
