#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

// ARA side of ARAck: the document controller and the playback renderer.
// The renderer only produces the clip audio from the host's audio source. The
// processor then runs that audio through the CHAIN.
// Based on the JUCE ARAPluginDemo (ISC licence).

class SharedTimeSliceThread final : public juce::TimeSliceThread
{
public:
    SharedTimeSliceThread() : juce::TimeSliceThread ("ARAck ARA Sample Reading Thread")
    {
        startThread (Priority::high);
    }
};

struct ProcessingLockInterface
{
    virtual ~ProcessingLockInterface() = default;
    virtual juce::ScopedTryReadLock getProcessingLock() = 0;
};

class ARAckPlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    ARAckPlaybackRenderer (ARA::PlugIn::DocumentController* dc, ProcessingLockInterface& lockIn)
        : juce::ARAPlaybackRenderer (dc), lockInterface (lockIn) {}

    void prepareToPlay (double sampleRate, int maximumSamplesPerBlock, int numChannels,
                        juce::AudioProcessor::ProcessingPrecision, AlwaysNonRealtime) override;
    void releaseResources() override;

    bool processBlock (juce::AudioBuffer<float>& buffer,
                       juce::AudioProcessor::Realtime realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

    using juce::ARAPlaybackRenderer::processBlock;

    // Position of the current block inside the clip (playback region), in samples.
    // INT64_MIN when the playhead is outside every region.
    std::atomic<juce::int64> clipPositionSamples { std::numeric_limits<juce::int64>::min() };

private:
    class Reader
    {
    public:
        Reader() = default;
        explicit Reader (std::unique_ptr<juce::BufferingAudioReader> r)
            : setTimeout ([p = r.get()] (int ms) { p->setReadTimeout (ms); }), reader (std::move (r)) {}
        explicit Reader (std::unique_ptr<juce::AudioFormatReader> r) : reader (std::move (r)) {}
        void setReadTimeout (int ms) { if (setTimeout) setTimeout (ms); }
        juce::AudioFormatReader* get() const { return reader.get(); }
    private:
        std::function<void (int)> setTimeout;
        std::unique_ptr<juce::AudioFormatReader> reader;
    };

    ProcessingLockInterface& lockInterface;
    juce::SharedResourcePointer<SharedTimeSliceThread> sharedThread;
    std::map<juce::ARAAudioSource*, Reader> readers;
    bool useBuffered = true;
    int numChannels = 2;
    double sampleRate = 48000.0;
    int maxBlock = 128;
    std::unique_ptr<juce::AudioBuffer<float>> tempBuffer;
};

class ARAckDocumentController final : public juce::ARADocumentControllerSpecialisation,
                                      private ProcessingLockInterface
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

protected:
    void willBeginEditing (juce::ARADocument*) override { processLock.enterWrite(); }
    void didEndEditing (juce::ARADocument*) override    { processLock.exitWrite(); }

    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new ARAckPlaybackRenderer (getDocumentController(), *this);
    }

    bool doRestoreObjectsFromStream (juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter*) noexcept override
    {
        input.readInt64(); // format version; nothing stored per ARA object yet
        return ! input.failed();
    }

    bool doStoreObjectsToStream (juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter*) noexcept override
    {
        return output.writeInt64 (1);
    }

private:
    juce::ScopedTryReadLock getProcessingLock() override { return juce::ScopedTryReadLock { processLock }; }
    juce::ReadWriteLock processLock;
};
