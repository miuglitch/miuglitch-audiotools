#include "smudge.h"
#include <algorithm>

namespace GlitchCore
{

static constexpr float kPi = 3.14159265358979f;

//==============================================================================
// ChorusDelay
//==============================================================================

void Smudge::ChorusDelay::prepare (int maxSamples)
{
    size = std::max (maxSamples, 16);
    buffer.assign ((size_t) size, 0.0f);
    writePos = 0;
}

void Smudge::ChorusDelay::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
}

float Smudge::ChorusDelay::process (float x)
{
    // Guard against any path where process is called before prepare
    if (size < 16 || buffer.empty()) return x;

    // Advance shared LFO
    lfoPhase += lfoInc;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    if (lfoPhase <  0.0f) lfoPhase += 1.0f;

    // Write current sample
    buffer[(size_t) writePos] = x;

    // Sum 3 taps, each with its own phase-offset triangle LFO
    float sum = 0.0f;
    for (int t = 0; t < kTapsPerChorus; ++t)
    {
        float ph = lfoPhase + tapPhaseOffset[(size_t) t];
        if (ph >= 1.0f) ph -= 1.0f;
        const float tri = (ph < 0.5f) ? (4.0f * ph - 1.0f)
                                      : (3.0f - 4.0f * ph);

        float delaySamps = tapBaseSamps[(size_t) t]
                         + tri * tapDepthSamps[(size_t) t];

        // Clamp to valid read range
        if (delaySamps < 1.0f)              delaySamps = 1.0f;
        if (delaySamps > (float)(size - 2)) delaySamps = (float)(size - 2);

        float readPos = (float) writePos - delaySamps;
        while (readPos < 0.0f)         readPos += (float) size;
        while (readPos >= (float) size) readPos -= (float) size;

        int i0 = (int) readPos;
        if (i0 < 0)        i0 = 0;
        if (i0 >= size)    i0 = size - 1;
        int i1 = i0 + 1;
        if (i1 >= size)    i1 = 0;

        const float frac = readPos - (float) i0;
        sum += buffer[(size_t) i0] * (1.0f - frac)
             + buffer[(size_t) i1] * frac;
    }

    writePos = (writePos + 1) % size;

    // Average the 3 taps to keep level consistent
    return sum * (1.0f / (float) kTapsPerChorus);
}

//==============================================================================
// Chain
//==============================================================================

void Smudge::Chain::reset()
{
    for (auto& a : allpass) a.reset();
    for (auto& c : chorus)  c.reset();
}

float Smudge::Chain::process (float x)
{
    for (int i = 0; i < kStagesPerChain; ++i)
    {
        x = allpass[(size_t) i].process (x);
        x = chorus[(size_t) i].process (x);
    }
    return x;
}

//==============================================================================
// RandomLFO
//==============================================================================

float Smudge::RandomLFO::tick (float phaseInc, float smoothCoeff)
{
    phase += phaseInc;
    if (phase >= 1.0f)
    {
        phase -= 1.0f;
        if (rng != nullptr)
        {
            std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
            target = dist (*rng);
        }
    }
    current += smoothCoeff * (target - current);
    return current;
}

//==============================================================================
// Smudge
//==============================================================================

Smudge::Smudge()
    : rng_ (std::random_device{}())
{
}

void Smudge::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_   = sampleRate;
    maxBlockSize_ = (maxBlockSize > 0) ? maxBlockSize : 512;

    const float rampBlocks = 30.0f;
    smAmount_.setRampBlocks   (rampBlocks);
    smWarble_.setRampBlocks   (rampBlocks);
    smRate_.setRampBlocks     (rampBlocks);
    smStereo_.setRampBlocks   (rampBlocks);
    smFlutter_.setRampBlocks  (rampBlocks);
    smHP_.setRampBlocks       (rampBlocks);
    smLP_.setRampBlocks       (rampBlocks);
    smStuffing_.setRampBlocks (rampBlocks);

    smAmount_.snap   (0.0f);
    smWarble_.snap   (0.3f);
    smRate_.snap     (0.3f);
    smStereo_.snap   (0.0f);
    smFlutter_.snap  (0.0f);
    smHP_.snap       (0.0f);
    smLP_.snap       (1.0f);
    smStuffing_.snap (0.0f);

    // Delay buffer headroom: base + max warble + max tap cluster spread.
    const int maxDelaySamps =
        (int) std::ceil (kMaxDelayBuffMs * 0.001f * (float) sampleRate_) + 16;

    // ---- 16 allpass cutoffs spread log across kAllpassLowHz..kAllpassHighHz
    const float logLow  = std::log (kAllpassLowHz);
    const float logHigh = std::log (kAllpassHighHz);
    const int totalFilters = kNumChains * kStagesPerChain;
    std::array<float, 16> apHz {};
    for (int i = 0; i < totalFilters; ++i)
    {
        const float t = (float) i / (float) (totalFilters - 1);
        apHz[(size_t) i] = std::exp (logLow + t * (logHigh - logLow));
    }
    // Interleave so consecutive chains/stages don't sit on neighbouring bands.
    // chain 0 -> indices 0,4,8,12 ; chain 1 -> 1,5,9,13 ; etc.
    for (int c = 0; c < kNumChains; ++c)
    {
        const float perChainDetune = ((float) c - 1.5f) * 0.015f; // small detune
        for (int i = 0; i < kStagesPerChain; ++i)
        {
            const int hzIdx = i * kNumChains + c;
            const float hz  = apHz[(size_t) hzIdx];
            const float w   = std::tan (kPi * hz / (float) sampleRate_);
            float g         = (w - 1.0f) / (w + 1.0f);
            g = std::clamp (g + perChainDetune, -0.97f, 0.97f);
            chains_[c].allpass[(size_t) i].g = g;
        }
    }

    // ---- Chorus instances — prepare buffers, set per-instance offsets
    int instanceIdx = 0;
    const int totalChorus = kNumChains * kStagesPerChain;
    for (int c = 0; c < kNumChains; ++c)
    {
        for (int i = 0; i < kStagesPerChain; ++i)
        {
            auto& ch = chains_[c].chorus[(size_t) i];
            ch.prepare (maxDelaySamps);

            // Stagger LFO start phases so nothing moves in unison
            std::uniform_real_distribution<float> ph (0.0f, 1.0f);
            ch.lfoPhase = ph (rng_);

            // Per-instance tap phase offsets — clustered, slightly randomised
            // so taps don't sit on identical positions across chains.
            const float jitter = (float) instanceIdx / (float) totalChorus;
            ch.tapPhaseOffset[0] = 0.00f + jitter * 0.05f;
            ch.tapPhaseOffset[1] = 0.18f + jitter * 0.05f;
            ch.tapPhaseOffset[2] = 0.36f + jitter * 0.05f;

            ++instanceIdx;
        }
    }

    for (auto& lfo : flutterLFO_)
    {
        lfo.init (rng_);
        std::uniform_real_distribution<float> ph (0.0f, 1.0f);
        lfo.phase = ph (rng_);
    }

    dryL_.assign ((size_t) maxBlockSize_, 0.0f);
    dryR_.assign ((size_t) maxBlockSize_, 0.0f);

    reset();
    prepared_ = true;
}

void Smudge::reset()
{
    for (auto& chain : chains_) chain.reset();
    hpL_.reset(); hpR_.reset();
    lpL_.reset(); lpR_.reset();
}

//==============================================================================
// Parameter mapping
//==============================================================================

float Smudge::hpCutoffToHz (float norm) const
{
    return kHpMinHz * std::pow (kHpMaxHz / kHpMinHz, norm);   // 20..2000 Hz
}

float Smudge::lpCutoffToHz (float norm) const
{
    return kLpMinHz * std::pow (kLpMaxHz / kLpMinHz, norm);   // 1000..20000 Hz
}

float Smudge::warbleToDepthMs (float warble) const
{
    const float t = std::pow (warble, 0.7f);
    return kMinWarbleMs + t * (kMaxWarbleMs - kMinWarbleMs);
}

float Smudge::rateToHz (float rateNorm) const
{
    const float t = std::pow (rateNorm, 0.7f);
    return kMinRateHz + t * (kMaxRateHz - kMinRateHz);
}

float Smudge::stuffToThreshold (float stuffing) const
{
    return 1.0f - std::pow (stuffing, 2.0f) * 0.92f;
}

float Smudge::amountToChainGain (float amount, int chainIndex) const
{
    const float perChain = kFadeInEndAmt / (float) kNumChains;
    const float start    = (float) chainIndex * perChain;
    const float end      = start + perChain;
    if (amount <= start) return 0.0f;
    if (amount >= end)   return 1.0f;
    return (amount - start) / (end - start);
}

//==============================================================================
// Process
//==============================================================================

void Smudge::process (const float* const* inputChannels,
                      float* const*       outputChannels,
                      int                 numChannels,
                      int                 numSamples,
                      const Params&       params)
{
    if (! prepared_ || numChannels < 1 || numSamples < 1)
    {
        // If not prepared yet (e.g. state restore before prepareToPlay),
        // pass input straight through so the host doesn't get garbage.
        for (int ch = 0; ch < numChannels; ++ch)
            if (inputChannels[ch] != outputChannels[ch])
                std::copy (inputChannels[ch],
                           inputChannels[ch] + numSamples,
                           outputChannels[ch]);
        return;
    }

    // ---- Per-block parameter smoothing -------------------------------------
    const float amount   = smAmount_.process   (params.amount);
    const float warble   = smWarble_.process   (params.warble);
    const float rate     = smRate_.process     (params.rate);
    const float stereo   = smStereo_.process   (params.stereo);
    const float flutter  = smFlutter_.process  (params.stereoFlutter);
    const float hp       = smHP_.process       (params.highpass);
    const float lp       = smLP_.process       (params.lowpass);
    const float stuffing = smStuffing_.process (params.stuffing);

    // ---- Hard bypass at zero ----------------------------------------------
    if (amount < 0.0005f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            if (inputChannels[ch] != outputChannels[ch])
                std::copy (inputChannels[ch],
                           inputChannels[ch] + numSamples,
                           outputChannels[ch]);
        return;
    }

    // ---- Derived values ----------------------------------------------------
    const float stuffThreshold = stuffToThreshold (stuffing);
    const float hpHz           = hpCutoffToHz (hp);
    const float lpHz           = lpCutoffToHz (lp);
    const float warbleMs       = warbleToDepthMs (warble);
    const float rateHz         = rateToHz (rate);

    auto onePoleCoeff = [this] (float hz)
    {
        const float w = 2.0f * kPi * hz / (float) sampleRate_;
        return std::clamp (w / (1.0f + w), 0.0f, 1.0f);
    };
    hpL_.coeff = hpR_.coeff = onePoleCoeff (hpHz);
    lpL_.coeff = lpR_.coeff = onePoleCoeff (lpHz);

    // ---- Update all 16 choruses with global Warble + Rate (small per-stage
    //      variation: max 2ms warble difference, max 0.05Hz rate difference)
    const float baseDelaySamps = kBaseDelayMs * 0.001f * (float) sampleRate_;
    const int totalChorus = kNumChains * kStagesPerChain;
    int chIdx = 0;
    for (int c = 0; c < kNumChains; ++c)
    {
        for (int i = 0; i < kStagesPerChain; ++i)
        {
            auto& ch = chains_[c].chorus[(size_t) i];

            // Per-instance fractional offset 0..1
            const float frac = (float) chIdx / (float) (totalChorus - 1);

            // Warble depth per instance: warbleMs ± 1ms ( = max 2ms spread)
            const float depthMs = std::max (0.0f,
                                  warbleMs + (frac - 0.5f) * 2.0f);
            const float depthSamps = depthMs * 0.001f * (float) sampleRate_;

            // Rate per instance: rateHz ± 0.025Hz ( = max 0.05Hz spread)
            const float instRateHz = std::max (0.01f,
                                     rateHz + (frac - 0.5f) * 0.05f);
            ch.lfoInc = instRateHz / (float) sampleRate_;

            // Tap base delays — clustered tightly around kBaseDelayMs.
            // Cluster offsets in samples (small, gives instant width).
            const float clusterMs[kTapsPerChorus] = { 0.0f, 1.2f, 2.4f };
            for (int t = 0; t < kTapsPerChorus; ++t)
            {
                const float tapMs = kBaseDelayMs + clusterMs[t];
                ch.tapBaseSamps[(size_t) t] =
                    tapMs * 0.001f * (float) sampleRate_;
                ch.tapDepthSamps[(size_t) t] = depthSamps;
            }
            // Sanity: ensure base + depth never overruns the buffer
            const float maxRead = baseDelaySamps + 3.0f + depthSamps;
            const float bufLim  = (float)(ch.size - 4);
            if (maxRead > bufLim)
            {
                const float clipped = std::max (0.0f, bufLim - baseDelaySamps - 3.0f);
                for (int t = 0; t < kTapsPerChorus; ++t)
                    ch.tapDepthSamps[(size_t) t] = clipped;
            }
            ++chIdx;
        }
    }

    // Flutter LFO (fixed rate)
    const float flutterRateHz = 6.0f;
    const float flutterInc    = flutterRateHz / (float) sampleRate_;
    const float flutterSmooth = std::clamp (flutterRateHz * 6.0f
                                            / (float) sampleRate_,
                                            0.0001f, 1.0f);

    // ---- Stereo base pan: baseline spread at Stereo=0, widens with Stereo
    float basePan[kNumChains];
    {
        const float pos[kNumChains] = { -1.0f, 1.0f, -0.5f, 0.5f };
        const float baseline = 0.35f;
        for (int c = 0; c < kNumChains; ++c)
        {
            const float spread = baseline + (1.0f - baseline) * stereo;
            basePan[c] = pos[c] * spread;
        }
    }

    // ---- Pre-capture dry signal ------------------------------------------
    const int n = std::min (numSamples, maxBlockSize_);
    std::copy (inputChannels[0], inputChannels[0] + n, dryL_.data());
    if (numChannels >= 2)
        std::copy (inputChannels[1], inputChannels[1] + n, dryR_.data());
    else
        std::copy (dryL_.begin(), dryL_.begin() + n, dryR_.begin());

    const float wetNorm = 0.5f;

    // ---- Sample loop -------------------------------------------------------
    for (int s = 0; s < n; ++s)
    {
        const float inL = dryL_[(size_t) s];
        const float inR = dryR_[(size_t) s];

        // Stuffing (input clipper, pre-filter)
        float preL = inL, preR = inR;
        if (stuffThreshold < 0.999f)
        {
            const float g = 1.0f / stuffThreshold;
            preL = std::clamp (preL * g, -1.0f, 1.0f) * stuffThreshold;
            preR = std::clamp (preR * g, -1.0f, 1.0f) * stuffThreshold;
        }

        // Input highpass (LP-subtract derivation)
        preL = preL - hpL_.process (preL);
        preR = preR - hpR_.process (preR);

        // Run chains. Even chains receive L, odd receive R — keeps mono->stereo
        // path open via the distinct delay fingerprints baked into each chain.
        float outL = 0.0f, outR = 0.0f;
        for (int c = 0; c < kNumChains; ++c)
        {
            const float g = amountToChainGain (amount, c);
            const float chainIn = (c % 2 == 0) ? preL : preR;
            const float wet = chains_[c].process (chainIn * g);

            float flut = flutterLFO_[(size_t) c].tick (flutterInc, flutterSmooth);
            if (c % 2 == 1) flut = -flut;
            const float pan = std::clamp (basePan[c] + flut * flutter,
                                          -1.0f, 1.0f);
            const float angle = (pan + 1.0f) * 0.25f * kPi;
            outL += wet * std::cos (angle);
            outR += wet * std::sin (angle);
        }
        outL *= wetNorm;
        outR *= wetNorm;

        // Output lowpass (post-chains, post-sum)
        outL = lpL_.process (outL);
        outR = lpR_.process (outR);

        // Wet/dry blend
        const float wet = std::min (amount / kFadeInEndAmt, 1.0f);
        const float dry = 1.0f - wet;

        if (numChannels >= 2)
        {
            outputChannels[0][s] = inL * dry + outL * wet;
            outputChannels[1][s] = inR * dry + outR * wet;
        }
        else
        {
            outputChannels[0][s] = inL * dry + (outL + outR) * 0.5f * wet;
        }
    }

    // Passthrough for any surround channels beyond stereo
    for (int ch = 2; ch < numChannels; ++ch)
        if (inputChannels[ch] != outputChannels[ch])
            std::copy (inputChannels[ch], inputChannels[ch] + n,
                       outputChannels[ch]);
}

} // namespace GlitchCore
