#include "ARADocument.h"

using namespace juce;

//==============================================================================
void ARAckPlaybackRenderer::prepareToPlay (double sr, int maximumSamplesPerBlock, int channels,
                                           AudioProcessor::ProcessingPrecision, AlwaysNonRealtime alwaysNonRealtime)
{
    numChannels = channels;
    sampleRate = sr;
    maxBlock = maximumSamplesPerBlock;
    tempBuffer = std::make_unique<AudioBuffer<float>> (numChannels, maxBlock);
    useBuffered = alwaysNonRealtime == AlwaysNonRealtime::no;

    for (auto* region : getPlaybackRegions())
    {
        auto* mod = region->getAudioModification<ARAckAudioModification>();
        mod->model.chain.prepare (sampleRate, maxBlock);

        auto* source = mod->getAudioSource();

        if (source->getSampleRate() != sampleRate)
            arackLog ("Renderer: clip sample rate " + String (source->getSampleRate()) + " Hz differs from project "
                      + String (sampleRate) + " Hz");

        if (readers.find (source) != readers.end())
            continue;

        auto reader = std::make_unique<ARAAudioSourceReader> (source);

        if (! useBuffered)
        {
            readers.emplace (source, Reader { std::unique_ptr<AudioFormatReader> (std::move (reader)) });
        }
        else
        {
            const auto readAhead = jmax (4 * maxBlock, roundToInt (2.0 * sampleRate));
            readers.emplace (source, Reader { std::make_unique<BufferingAudioReader> (reader.release(), *sharedThread, readAhead) });
        }
    }
}

void ARAckPlaybackRenderer::releaseResources()
{
    readers.clear();
    tempBuffer.reset();
}

int ARAckPlaybackRenderer::getChainLatencySamples() const
{
    int latency = 0;
    for (auto* region : getPlaybackRegions())
        latency = jmax (latency, region->getAudioModification<ARAckAudioModification>()->model.chain.getTotalLatencySamples());
    return latency;
}

bool ARAckPlaybackRenderer::processBlock (AudioBuffer<float>& buffer, AudioProcessor::Realtime realtime,
                                          const AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    buffer.clear();

    const auto lock = lockInterface.getProcessingLock();

    if (! lock.isLocked() || tempBuffer == nullptr)
        return true;

    const auto numSamples = buffer.getNumSamples();
    const auto timeInSamples = positionInfo.getTimeInSamples().orFallback (0);
    const bool playing = positionInfo.getIsPlaying();
    bool success = true;

    const auto blockRange = Range<int64>::withStartAndLength (timeInSamples, numSamples);

    for (auto* region : getPlaybackRegions())
    {
        auto* mod = region->getAudioModification<ARAckAudioModification>();
        auto& model = mod->model;

        const auto playbackRange = region->getSampleRange (sampleRate, ARAPlaybackRegion::IncludeHeadAndTail::no);
        auto renderRange = blockRange.getIntersectionWith (playbackRange);

        if (! playing || renderRange.isEmpty())
        {
            model.playPositionMs.store (-1.0);
            continue;
        }

        // Position of this block inside the clip, for the clip's automation.
        const double posMs = (double) (blockRange.getStart() - playbackRange.getStart()) / sampleRate * 1000.0;
        model.playPositionMs.store (jmax (0.0, posMs));

        Range<int64> modRange { region->getStartInAudioModificationSamples(), region->getEndInAudioModificationSamples() };
        const auto modOffset = modRange.getStart() - playbackRange.getStart();
        renderRange = renderRange.getIntersectionWith (modRange.movedToStartAt (playbackRange.getStart()));

        if (renderRange.isEmpty())
            continue;

        auto* source = mod->getAudioSource();
        auto it = readers.find (source);

        // Prototype: plays only when channel count and sample rate match the project.
        if (std::make_tuple (source->getChannelCount(), source->getSampleRate()) != std::make_tuple (numChannels, sampleRate)
            || it == readers.end())
        {
            success = false;
            continue;
        }

        auto& reader = it->second;
        reader.setReadTimeout (realtime == AudioProcessor::Realtime::no ? 100 : 0);

        const int toRead = (int) renderRange.getLength();
        const int startInBuffer = (int) (renderRange.getStart() - blockRange.getStart());
        const auto startInSource = renderRange.getStart() + modOffset;

        AudioBuffer<float> temp (tempBuffer->getArrayOfWritePointers(), numChannels, numSamples);
        temp.clear();

        if (! reader.get()->read (&temp, startInBuffer, toRead, startInSource, true, true))
        {
            success = false;
            continue;
        }

        // Run this clip's audio through its CHAIN, then mix into the output.
        model.chain.process (temp, jmax (0.0, posMs));

        for (int c = 0; c < numChannels; ++c)
            buffer.addFrom (c, 0, temp, c, 0, numSamples);
    }

    return success;
}

//==============================================================================
bool ARAckDocumentController::doStoreObjectsToStream (ARAOutputStream& output, const ARAStoreObjectsFilter* filter) noexcept
{
    const auto& mods = filter->getAudioModificationsToStore<ARAckAudioModification>();

    if (! output.writeInt64 (2)) // archive version
        return false;

    if (! output.writeInt64 ((int64) mods.size()))
        return false;

    for (auto* mod : mods)
    {
        if (MessageManager::existsAndIsCurrentThread())
            const_cast<ARAckAudioModification*> (mod)->model.chain.storePluginStates();

        String xml;
        if (auto e = mod->model.session.root.createXml())
            xml = e->toString (XmlElement::TextFormat().singleLine());

        if (! output.writeString (mod->getPersistentID()) || ! output.writeString (xml))
            return false;
    }

    return true;
}

bool ARAckDocumentController::doRestoreObjectsFromStream (ARAInputStream& input, const ARARestoreObjectsFilter* filter) noexcept
{
    const auto version = input.readInt64();

    if (version < 2)
        return ! input.failed(); // prototype v0.1 stored nothing

    const auto count = input.readInt64();

    for (int64 i = 0; i < count && ! input.failed(); ++i)
    {
        const String persistentID = input.readString();
        const String xml = input.readString();

        if (auto* mod = filter->getAudioModificationToRestoreStateWithID<ARAckAudioModification> (persistentID.getCharPointer()))
            if (auto e = parseXML (xml))
                mod->model.session.replaceWith (ValueTree::fromXml (*e));
    }

    return ! input.failed();
}

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<ARAckDocumentController>();
}
