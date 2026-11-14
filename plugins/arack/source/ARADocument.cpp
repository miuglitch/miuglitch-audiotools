#include "ARADocument.h"

using namespace juce;

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
        auto* source = region->getAudioModification()->getAudioSource();

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

bool ARAckPlaybackRenderer::processBlock (AudioBuffer<float>& buffer, AudioProcessor::Realtime realtime,
                                          const AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    const auto lock = lockInterface.getProcessingLock();

    if (! lock.isLocked())
        return true;

    const auto numSamples = buffer.getNumSamples();
    const auto timeInSamples = positionInfo.getTimeInSamples().orFallback (0);
    bool success = true;
    bool rendered = false;
    auto clipPos = std::numeric_limits<int64>::min();

    if (positionInfo.getIsPlaying())
    {
        const auto blockRange = Range<int64>::withStartAndLength (timeInSamples, numSamples);

        for (auto* region : getPlaybackRegions())
        {
            const auto playbackRange = region->getSampleRange (sampleRate, ARAPlaybackRegion::IncludeHeadAndTail::no);
            auto renderRange = blockRange.getIntersectionWith (playbackRange);

            if (renderRange.isEmpty())
                continue;

            if (clipPos == std::numeric_limits<int64>::min())
                clipPos = jmax ((int64) 0, blockRange.getStart() - playbackRange.getStart());

            Range<int64> modRange { region->getStartInAudioModificationSamples(), region->getEndInAudioModificationSamples() };
            const auto modOffset = modRange.getStart() - playbackRange.getStart();
            renderRange = renderRange.getIntersectionWith (modRange.movedToStartAt (playbackRange.getStart()));

            if (renderRange.isEmpty())
                continue;

            auto* source = region->getAudioModification()->getAudioSource();
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

            auto& target = rendered ? *tempBuffer : buffer;

            if (! reader.get()->read (&target, startInBuffer, toRead, startInSource, true, true))
            {
                success = false;
                continue;
            }

            if (rendered)
            {
                for (int c = 0; c < numChannels; ++c)
                    buffer.addFrom (c, startInBuffer, *tempBuffer, c, startInBuffer, toRead);
            }
            else
            {
                if (startInBuffer != 0)
                    buffer.clear (0, startInBuffer);

                const int end = startInBuffer + toRead;
                if (end < numSamples)
                    buffer.clear (end, numSamples - end);

                rendered = true;
            }
        }
    }

    clipPositionSamples.store (clipPos);

    if (! rendered)
        buffer.clear();

    return success;
}

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<ARAckDocumentController>();
}
