#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/harmonizer/harmonizer.h"

//==============================================================================
// SpectralHarmonizerAudioProcessor
// Place in: glitchcore/plugins/harmonizer/source/PluginProcessor.h
//==============================================================================

namespace ParamID
{
    inline const juce::String Amount       = "amount";
    inline const juce::String Sustain      = "sustain";
    inline const juce::String Root         = "root";
    inline const juce::String Scale        = "scale";
    inline const juce::String Source       = "target_source";
    inline const juce::String MidiTargets  = "midi_targets";
    inline const juce::String Harmonics    = "harmonics";
    inline const juce::String Latch        = "latch";
    inline const juce::String Mpe          = "mpe";
    inline const juce::String BendRange    = "bend_range";
    inline const juce::String MpeBendRange = "mpe_bend_range";
    inline const juce::String FftSize      = "fft_size";
}

class SpectralHarmonizerAudioProcessor : public juce::AudioProcessor
{
public:
    SpectralHarmonizerAudioProcessor();
    ~SpectralHarmonizerAudioProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset          () override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const juce::String getName() const override { return "Spectral Harmonizer"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Custom scale: 24 bits, bit 0 = C of the low octave, bit 23 = B of the high.
    // Stored in the plugin state (not automatable), read lock-free by the audio thread.
    std::uint32_t getCustomMask() const { return customMask_.load(); }
    void          setCustomMask (std::uint32_t mask);

    // For the editor's status line: are MIDI notes currently steering the targets?
    bool isFollowingMidi()  const { return followingMidi_.load(); }
    int  getHeldNoteCount() const { return heldNotes_.load(); }

private:
    void handleMidi (const juce::MidiMessage&);
    GlitchCore::SpectralHarmonizer::Params buildParams() const;
    GlitchCore::SpectralHarmonizer::FftSize requestedFftSize() const;

    GlitchCore::SpectralHarmonizer harmonizer_;

    std::atomic<float>* pAmount       = nullptr;
    std::atomic<float>* pSustain      = nullptr;
    std::atomic<float>* pRoot         = nullptr;
    std::atomic<float>* pScale        = nullptr;
    std::atomic<float>* pSource       = nullptr;
    std::atomic<float>* pMidiTargets  = nullptr;
    std::atomic<float>* pHarmonics    = nullptr;
    std::atomic<float>* pLatch        = nullptr;
    std::atomic<float>* pMpe          = nullptr;
    std::atomic<float>* pBendRange    = nullptr;
    std::atomic<float>* pMpeBendRange = nullptr;
    std::atomic<float>* pFftSize      = nullptr;

    std::atomic<std::uint32_t> customMask_ { 0x91u | (0x91u << 12) };  // C E G in both octaves
    std::atomic<bool> followingMidi_ { false };
    std::atomic<int>  heldNotes_     { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralHarmonizerAudioProcessor)
};
