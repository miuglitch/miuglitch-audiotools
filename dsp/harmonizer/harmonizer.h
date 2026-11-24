#pragma once
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>
#include "fft.h"
#include "scales.h"

namespace GlitchCore
{

//==============================================================================
// SpectralHarmonizer — STFT morph that pulls every bin's energy onto the
// nearest target note, turning any input into tuned, ringing partials.
// Pure C++ DSP, no JUCE dependency.
// Place in: glitchcore/dsp/harmonizer/harmonizer.h
//
// Signal flow (per channel, per hop):
//   last N input samples -> Hann -> FFT -> [ sieve residual  ]-> mix -> IFFT
//                                       -> [ capture energy  ]
//                                       -> [ resynth partials]
//   -> Hann -> overlap-add (75 % overlap) -> output, N samples late
//
//   Amount 0 % : bins untouched; the plugin outputs the input delayed by
//                exactly getLatencySamples() (bit-exact, via a dry delay line
//                once no processed frame is left in the overlap-add buffer).
//   Amount up  : bins far from a target are attenuated (narrowing sieve) and
//                their energy is re-emitted as phase-continuous sinusoids at
//                the target frequencies. At 100 % the output is fully tonal.
//
// Targets come from held MIDI notes (harmonic series or all octaves of each
// note, with per-note pitch bend / pressure / CC74 in MPE mode) or, when no
// notes are held, from a root + scale (all Live scales, or a custom
// two-octave note set).
//
// No transient preservation by design: this is built for soundscapes.
//==============================================================================

class SpectralHarmonizer
{
public:
    //==========================================================================
    static constexpr int kMaxChannels   = 2;
    static constexpr int kMaxFftOrder   = 12;           // 4096
    static constexpr int kMaxFftSize    = 1 << kMaxFftOrder;
    static constexpr int kOverlap       = 4;            // hop = N / 4
    static constexpr int kMaxVoices     = 16;
    static constexpr int kMaxPartialsPerVoice = 32;
    static constexpr int kNumScaleIds   = 144;          // MIDI pitch 0..143
    static constexpr int kNumPartialIds = kNumScaleIds + kMaxVoices * kMaxPartialsPerVoice;
    static constexpr int kMpeMasterChannel = 1;         // MPE lower zone

    enum class FftSize      { k2048 = 0, k4096 = 1 };
    enum class TargetSource { Auto = 0, Scale = 1, Midi = 2 };
    enum class MidiTargets  { Harmonics = 0, AllOctaves = 1 };

    //==========================================================================
    struct Params
    {
        float amount        = 0.0f;   // 0..1 morph toward the targets
        float sustain       = 0.0f;   // 0..1 how long partials ring after the input stops
        int   root          = 0;      // 0 = C .. 11 = B
        int   scale         = 0;      // index into kScales, or kCustomScaleIndex
        std::uint32_t customMask = 0x91u | (0x91u << 12); // 24 bits, bit 0 = C in the low octave

        TargetSource source      = TargetSource::Auto;
        MidiTargets  midiTargets = MidiTargets::Harmonics;
        int   numHarmonics  = 16;     // 1..kMaxPartialsPerVoice (Harmonics mode)
        bool  latch         = false;  // keep MIDI notes after release
        bool  mpe           = false;  // MPE lower zone (ch 1 master, 2..16 members)
        float bendRange     = 2.0f;   // semitones, normal / MPE master bend
        float mpeBendRange  = 48.0f;  // semitones, MPE per-note bend
    };

    //==========================================================================
    SpectralHarmonizer();

    // Allocates for the largest FFT; switching size later never allocates.
    void prepare (double sampleRate, int maxBlockSize, FftSize size = FftSize::k4096);
    void setFftSize (FftSize size);   // resets the stream; call getLatencySamples() after
    FftSize getFftSize() const { return fftSize_; }
    int  getLatencySamples() const { return fftN_; }
    double getTailLengthSeconds (float sustain) const;
    void reset();

    //==========================================================================
    // MIDI input. Channels are 1..16. Values are normalised:
    // velocity / pressure / cc in 0..1, pitch bend in -1..1.
    void noteOn          (int channel, int note, float velocity);
    void noteOff         (int channel, int note);
    void pitchBend       (int channel, float bend);
    void channelPressure (int channel, float pressure);
    void polyPressure    (int channel, int note, float pressure);
    void controller      (int channel, int cc, float value);
    void allNotesOff();
    bool hasActiveNotes() const;
    int  getActiveNoteCount() const;
    void setLatch (bool shouldLatch);

    //==========================================================================
    // Channels beyond kMaxChannels are delayed by the latency but not processed.
    void process (const float* const* inputChannels,
                  float* const*       outputChannels,
                  int                 numChannels,
                  int                 numSamples,
                  const Params&       params);

private:
    using Complex = std::complex<float>;

    //==========================================================================
    struct Voice
    {
        bool  active    = false;   // contributes targets
        bool  keyDown   = false;   // physically held
        int   channel   = 1;
        int   note      = 60;
        float velocity  = 1.0f;
        float pressure  = 0.0f;
        float timbre    = 0.5f;    // CC74 (MPE)
        std::uint32_t age = 0;
    };

    struct Target
    {
        float freq;
        float logFreq;   // log2(freq)
        float weight;
        int   id;
    };

    struct Partial
    {
        float  freq    = 0.0f;
        float  weight  = 0.0f;
        float  energy  = 0.0f;     // smoothed captured energy
        bool   isTarget = false;
    };

    struct ChannelState
    {
        std::vector<float>   inRing;     // last N input samples (circular)
        std::vector<float>   outAccum;   // overlap-add accumulator (linear)
        std::vector<float>   dryRing;    // dry delay line for the exact null
        std::vector<Complex> spectrum;
        std::vector<Complex> tonal;
        std::array<Partial, kNumPartialIds> partials {};
    };

    //==========================================================================
    void processFrame (int channel);
    void buildTargets (const Params& p);
    void updateFrameAmount (float target);
    float noteFrequency (const Voice& v, const Params& p) const;
    int  findVoice (int channel, int note) const;
    int  allocateVoice();
    void releaseVoice (Voice& v);

    static float lobeKernel (float x);

    //==========================================================================
    double sampleRate_   = 44100.0;
    int    maxBlockSize_ = 512;
    bool   prepared_     = false;

    FftSize fftSize_ = FftSize::k4096;
    int     fftN_    = kMaxFftSize;
    int     hop_     = kMaxFftSize / kOverlap;
    int     numBins_ = kMaxFftSize / 2 + 1;

    RadixTwoFFT fft2048_, fft4096_;
    const RadixTwoFFT* fft_ = nullptr;

    std::vector<float> window_;        // periodic Hann, length N
    std::vector<float> binLog2_;       // log2 of each bin's centre frequency
    std::vector<int>   binTarget_;     // nearest target index per bin (-1 = none)
    std::vector<float> binCents_;      // distance to that target, cents

    std::array<ChannelState, kMaxChannels> chans_;
    std::vector<std::vector<float>> extraDry_;  // delay lines for channels > kMaxChannels
    std::array<std::vector<float>, kMaxChannels> blockIn_;

    int ringPos_          = 0;   // shared write index into inRing / dryRing
    int hopCounter_       = 0;
    int wetSamplesInFlight_ = 0; // > 0 while processed frames are still in outAccum

    float frameAmount_    = 0.0f;
    float frameSustain_   = 0.0f;
    std::array<float, kNumPartialIds> partialPhase_ {};   // shared so stereo stays coherent

    std::array<Target, kNumPartialIds> targets_ {};
    int numTargets_ = 0;

    // MIDI state
    std::array<Voice, kMaxVoices> voices_ {};
    std::array<float, 17> channelBend_ {};
    std::array<float, 17> channelPressure_ {};
    std::array<float, 17> channelTimbre_ {};
    std::uint32_t voiceClock_ = 0;
    bool latch_ = false;
    bool sustainPedal_ = false;
    bool mpe_ = false;
};

} // namespace GlitchCore
