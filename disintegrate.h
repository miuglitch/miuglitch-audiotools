#pragma once
#include <vector>
#include <random>

// Pure C++ DSP class — no JUCE dependency.
// Applies random sample dropouts to an audio buffer.
// Dropout probability increases with Amount (0.0 - 1.0).
// Minimum drop duration is specified in milliseconds.
// Supports any number of channels (stereo, surround, etc.)
// Zero latency.

class Disintegrate
{
public:
    Disintegrate();

    // Call this when the plugin is initialized or sample rate changes.
    // numChannels: how many audio channels (2 = stereo, 6 = 5.1, etc.)
    void prepare(double sampleRate, int numChannels);

    // Process a buffer of audio in-place.
    // channels: array of pointers, one per channel, each of length numSamples.
    void process(float** channels, int numChannels, int numSamples);

    // Amount: 0.0 = no effect, 1.0 = full silence
    void setAmount(float amount);

    // Minimum dropout duration in milliseconds (e.g. 1ms to 500ms)
    void setMinDropMs(float ms);

    // If true, all channels drop together.
    // If false, each channel is randomized independently.
    void setStereoLink(bool linked);

private:
    void recalculateHoldSamples();
    bool shouldDrop(int channel);
    float amountToProbability(float amount) const;

    double sampleRate;
    int numChannels;

    float amount;
    float minDropMs;
    int holdSamples;

    bool stereoLink;

    std::vector<int> holdCounters;
    std::vector<bool> dropping;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;
};