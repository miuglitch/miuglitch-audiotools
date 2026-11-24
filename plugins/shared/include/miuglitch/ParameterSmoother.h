#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace miuglitch
{

/** One-pole parameter smoother for click-free automation. */
class ParameterSmoother
{
public:
    void prepare (double sampleRate, double rampTimeMs = 50.0);
    void reset (float value) noexcept;
    void setTarget (float value) noexcept;
    float getNextValue() noexcept;
    float getCurrentValue() const noexcept { return smoothed.getCurrentValue(); }

private:
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothed;
};

} // namespace miuglitch
