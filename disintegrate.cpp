#include "Disintegrate.h"
#include <cmath>
#include <random>

Disintegrate::Disintegrate()
    : sampleRate(44100.0)
    , numChannels(2)
    , amount(0.0f)
    , minDropMs(1.0f)
    , holdSamples(44)
    , stereoLink(true)
    , dist(0.0f, 1.0f)
{
    // Seed RNG from hardware entropy
    std::random_device rd;
    rng.seed(rd());
}

void Disintegrate::prepare(double newSampleRate, int newNumChannels)
{
    sampleRate   = newSampleRate;
    numChannels  = newNumChannels;

    holdCounters.assign(numChannels, 0);
    dropping.assign(numChannels, false);

    recalculateHoldSamples();
}

void Disintegrate::process(float** channels, int numChannels, int numSamples)
{
    const float probability = amountToProbability(amount);

    if (probability <= 0.0f)
        return;

    if (probability >= 1.0f)
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
            bool drop = shouldDrop(0);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (ch > 0)
                {
                    dropping[ch]     = dropping[0];
                    holdCounters[ch] = holdCounters[0];
                }
                if (drop)
                    channels[ch][s] = 0.0f;
            }
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (shouldDrop(ch))
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
    minDropMs = std::fmax(0.0f, ms);
    recalculateHoldSamples();
}

void Disintegrate::setStereoLink(bool linked)
{
    stereoLink = linked;
    holdCounters.assign(numChannels, 0);
    dropping.assign(numChannels, false);
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

float Disintegrate::amountToProbability(float amount) const
{
    return amount * amount * amount;
}