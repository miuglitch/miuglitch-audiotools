#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ClipModel.h"

// ARA side of ARAck.
// Hosts like Nuendo run several copies of ARAck for one document: one plays the audio
// (playback renderer), another shows the window (editor view). So each clip's CHAIN,
// automation and undo history live on the ARA audio modification (ARAckAudioModification),
// which every copy can reach. The playback renderer reads the clip audio and runs it
// through that clip's chain.
// Based on the JUCE ARAPluginDemo (ISC licence).

class ARAckAudioModification final : public juce::ARAAudioModification
{
public:
    ARAckAudioModification (juce::ARAAudioSource* source, ARA::ARAAudioModificationHostRef hostRef,
                            const ARAckAudioModification* optionalModificationToClone)
        : juce::ARAAudioModification (source, hostRef, optionalModificationToClone)
    {
        if (optionalModificationToClone != nullptr)
            model.session.replaceWith (optionalModificationToClone->model.session.root.createCopy());
    }

    ClipModel model;
};

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

    // Highest chain latency among the clips this renderer plays (message thread).
    int getChainLatencySamples() const;

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

    juce::ARAAudioModification* doCreateAudioModification (juce::ARAAudioSource* audioSource,
                                                           ARA::ARAAudioModificationHostRef hostRef,
                                                           const juce::ARAAudioModification* toClone) noexcept override
    {
        return new ARAckAudioModification (audioSource, hostRef, static_cast<const ARAckAudioModification*> (toClone));
    }

    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new ARAckPlaybackRenderer (getDocumentController(), *this);
    }

    // Each clip's chain (with plug-in settings) and automation are saved in the host project.
    bool doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) noexcept override;
    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept override;

private:
    juce::ScopedTryReadLock getProcessingLock() override { return juce::ScopedTryReadLock { processLock }; }
    juce::ReadWriteLock processLock;
};
