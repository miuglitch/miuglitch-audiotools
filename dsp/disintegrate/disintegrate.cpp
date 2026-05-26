#include "disintegrate.h"
#include <cmath>
#include <random>

Disintegrate::Disintegrate()
    : sampleRate(44100.0)
    , numChannels(2)
    , amount(0.0f)
    , minDropMs(1.0f)
    , holdSamples(44)
    , jitter(0.0f)
    , stereoLink(true)
    , dist(0.0f, 1.0f)
{
    std::random_device rd;
    rng.seed(rd());
}

void Disintegrate::prepare(double newSampleRate, int newNumChannels)
{
    sampleRate  = newSampleRate;
    numChannels = newNumChannels;

    holdCounters.assign(numChannels, 0);
    dropping.assign(numChannels, false);

    jitterCounters.assign(numChannels, 0);
    jittering.assign(numChannels, false);

    recalculateHoldSamples();
}

void Disintegrate::process(float** channels, int numChannels, int numSamples)
{
    const float probability = amountToProbability(amount);
    const bool  anyAmount   = probability > 0.0f;
    const bool  anyJitter   = jitter > 0.0f;

    if (!anyAmount && !anyJitter)
        return;

    if (probability >= 1.0f && !anyJitter)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < numSamples; ++s)
                channels[ch][s] = 0.0f;
        return;
    }

    for (int s = 0; s < numSamples; ++s)
    {
        if (stereoLink)
        {
            bool drop    = anyAmount && shouldDrop(0);
            bool jit     = anyJitter && shouldJitter(0);
            bool silence = drop || jit;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (ch > 0)
                {
                    dropping[ch]       = dropping[0];
                    holdCounters[ch]   = holdCounters[0];
                    jittering[ch]      = jittering[0];
                    jitterCounters[ch] = jitterCounters[0];
                }
                if (silence)
                    channels[ch][s] = 0.0f;
            }
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                bool drop = anyAmount && shouldDrop(ch);
                bool jit  = anyJitter && shouldJitter(ch);
                if (drop || jit)
                    channels[ch][s] = 0.0f;
            }
        }
    }
}

void Disintegrate::setAmount(float newAmount)
{
    amount = std::fmax(0.0f, std::fmin(1.0f, newAmount));
}

void Disintegrate::setMinDropMs(float ms)
{
    minDropMs = std::fmax(1.0f, ms);
    recalculateHoldSamples();
}

void Disintegrate::setJitter(float newJitter)
{
    jitter = std::fmax(0.0f, std::fmin(1.0f, newJitter));
}

void Disintegrate::setStereoLink(bool linked)
{
    stereoLink = linked;
    holdCounters.assign(numChannels, 0);
    dropping.assign(numChannels, false);
    jitterCounters.assign(numChannels, 0);
    jittering.assign(numChannels, false);
}

void Disintegrate::recalculateHoldSamples()
{
    holdSamples = std::max(1, (int)(minDropMs * sampleRate / 1000.0));
}

bool Disintegrate::shouldDrop(int channel)
{
    if (dropping[channel])
    {
        holdCounters[channel]--;
        if (holdCounters[channel] <= 0)
            dropping[channel] = false;
        return true;
    }

    if (dist(rng) < amountToProbability(amount))
    {
        dropping[channel]     = true;
        holdCounters[channel] = holdSamples;
        return true;
    }

    return false;
}

bool Disintegrate::shouldJitter(int channel)
{
    if (jittering[channel])
    {
        jitterCounters[channel]--;
        if (jitterCounters[channel] <= 0)
            jittering[channel] = false;
        return true;
    }

    const float eventsPerSec  = jitter * 10.0f;
    const float probPerSample = eventsPerSec / (float)sampleRate;

    if (dist(rng) < probPerSample)
    {
        const float minJitterMs    = 10.0f;
        const float maxJitterMs    = 50.0f;
        const float durationMs     = minJitterMs + dist(rng) * (maxJitterMs - minJitterMs);
        const int   durationSamples = (int)(durationMs * sampleRate / 1000.0f);

        jittering[channel]      = true;
        jitterCounters[channel] = std::max(1, durationSamples);
        return true;
    }

    return false;
}

float Disintegrate::amountToProbability(float amount) const
{
    return std::pow(amount, 4.5f);
}