#pragma once
#include <vector>
#include <random>

// Pure C++ DSP class — no JUCE dependency.
// Two independent dropout layers:
// Layer 1 (Amount): dense grainy texture, short dropouts at minimum drop time
// Layer 2 (Jitter): sparse discrete glitches, 10-50ms duration, up to 10/sec
// Supports any number of channels. Zero latency. Sample rate agnostic.

class Disintegrate
{
public:
    Disintegrate();

    void prepare(double sampleRate, int numChannels);
    void process(float** channels, int numChannels, int numSamples);

    void setAmount(float amount);        // 0.0 - 1.0
    void setMinDropMs(float ms);         // 1ms - 500ms
    void setJitter(float jitter);        // 0.0 - 1.0
    void setStereoLink(bool linked);

private:
    void recalculateHoldSamples();

    // Layer 1
    bool shouldDrop(int channel);
    float amountToProbability(float amount) const;

    // Layer 2
    bool shouldJitter(int channel);

    double sampleRate;
    int numChannels;

    float amount;
    float minDropMs;
    int holdSamples;
    float jitter;

    bool stereoLink;

    // Layer 1 state
    std::vector<int>  holdCounters;
    std::vector<bool> dropping;

    // Layer 2 state
    std::vector<int>  jitterCounters;
    std::vector<bool> jittering;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;
};