#pragma once
#include <cmath>
#include <array>
#include <vector>
#include <random>

namespace GlitchCore
{

//==============================================================================
// Smudge — feedforward allpass + multi-tap chorus cascade smear
// Pure C++ DSP, no JUCE dependency.
// Place in: glitchcore/dsp/smudge/smudge.h
//
// Signal flow:
//   input -> Stuffing clip -> HP -> 4 parallel chains -> sum -> LP -> Amount
//   each chain: [ allpass(12dB/oct) -> 3-tap chorus ] x kStagesPerChain
//==============================================================================

class Smudge
{
public:
    //==========================================================================
    static constexpr int   kNumChains       = 4;
    static constexpr int   kStagesPerChain  = 4;     // allpass+chorus pairs
    static constexpr int   kTapsPerChorus   = 3;
    static constexpr float kFadeInEndAmt    = 0.05f;
    static constexpr float kBaseDelayMs     = 4.0f;
    static constexpr float kMinWarbleMs     = 0.5f;
    static constexpr float kMaxWarbleMs     = 100.0f;
    static constexpr float kMinRateHz       = 0.6f;
    static constexpr float kMaxRateHz       = 10.0f;
    static constexpr float kMaxDelayBuffMs  = 220.0f; // headroom: base+warble+tap spread
    static constexpr float kHpMinHz         = 20.0f;
    static constexpr float kHpMaxHz         = 2000.0f;
    static constexpr float kLpMinHz         = 1000.0f;
    static constexpr float kLpMaxHz         = 20000.0f;
    static constexpr float kAllpassLowHz    = 100.0f;
    static constexpr float kAllpassHighHz   = 6000.0f;

    //==========================================================================
    struct Params
    {
        float amount        = 0.0f;
        float warble        = 0.3f;
        float rate          = 0.3f;
        float stereo        = 0.0f;
        float stereoFlutter = 0.0f;
        float highpass      = 0.0f;
        float lowpass       = 1.0f;
        float stuffing      = 0.0f;
    };

    //==========================================================================
    Smudge();
    ~Smudge() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (const float* const* inputChannels,
                  float* const*       outputChannels,
                  int                 numChannels,
                  int                 numSamples,
                  const Params&       params);

private:
    //==========================================================================
    // 12dB/oct allpass = two first-order allpass stages in series
    //==========================================================================
    struct Allpass12
    {
        float s1 = 0.0f, s2 = 0.0f;
        float g  = 0.0f;

        inline float process (float x)
        {
            const float y1 = -g * x  + s1;  s1 = x  + g * y1;
            const float y2 = -g * y1 + s2;  s2 = y1 + g * y2;
            return y2;
        }
        void reset() { s1 = s2 = 0.0f; }
    };

    //==========================================================================
    // 3-tap modulated delay (the chorus). Each tap = base delay + per-tap offset
    // + per-tap modulated depth from a triangle LFO.
    //==========================================================================
    struct ChorusDelay
    {
        std::vector<float> buffer;
        int   writePos = 0;
        int   size     = 0;

        // Shared LFO across all 3 taps but with per-tap phase offsets (in 0..1).
        float lfoPhase   = 0.0f;
        float lfoInc     = 0.0f;
        std::array<float, kTapsPerChorus> tapPhaseOffset { 0.0f, 0.333f, 0.667f };

        // Per-tap base delay (samples) and modulation depth (samples).
        std::array<float, kTapsPerChorus> tapBaseSamps  { 0, 0, 0 };
        std::array<float, kTapsPerChorus> tapDepthSamps { 0, 0, 0 };

        void  prepare (int maxSamples);
        void  reset();

        // Returns the sum of the 3 taps. Guards against zero size / bad indices.
        inline float process (float x);
    };

    //==========================================================================
    struct Chain
    {
        std::array<Allpass12,   kStagesPerChain> allpass;
        std::array<ChorusDelay, kStagesPerChain> chorus;

        void reset();
        float process (float x);
    };

    //==========================================================================
    struct OnePoleLP
    {
        float y = 0.0f, coeff = 1.0f;
        inline float process (float x) { y += coeff * (x - y); return y; }
        void reset() { y = 0.0f; }
    };

    //==========================================================================
    struct BlockSmoother
    {
        float current = 0.0f, step = 0.0f;
        void  setRampBlocks (float b) { step = (b > 0.0f) ? 1.0f / b : 1.0f; }
        void  snap (float v) { current = v; }
        float process (float target)
        {
            const float d = target - current;
            if      (d >  step) current += step;
            else if (d < -step) current -= step;
            else                current  = target;
            return current;
        }
    };

    //==========================================================================
    struct RandomLFO
    {
        float current = 0.0f, target = 0.0f, phase = 0.0f;
        std::mt19937* rng = nullptr;
        void  init (std::mt19937& r) { rng = &r; }
        float tick (float phaseInc, float smoothCoeff);
    };

    //==========================================================================
    float hpCutoffToHz       (float norm) const;
    float lpCutoffToHz       (float norm) const;
    float warbleToDepthMs    (float warble) const;
    float rateToHz           (float rateNorm) const;
    float stuffToThreshold   (float stuffing) const;
    float amountToChainGain  (float amount, int chainIndex) const;

    //==========================================================================
    double sampleRate_   = 44100.0;
    int    maxBlockSize_ = 512;

    std::array<Chain, kNumChains> chains_;

    OnePoleLP hpL_, hpR_;
    OnePoleLP lpL_, lpR_;

    std::array<RandomLFO, kNumChains> flutterLFO_;

    BlockSmoother smAmount_, smWarble_, smRate_, smStereo_;
    BlockSmoother smFlutter_, smHP_, smLP_, smStuffing_;

    std::mt19937 rng_;
    bool prepared_ = false;

    std::vector<float> dryL_, dryR_;
};

} // namespace GlitchCore
