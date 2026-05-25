#include <miuglitch/ParameterSmoother.h>

namespace miuglitch
{

void ParameterSmoother::prepare (double sampleRate, double rampTimeMs)
{
    smoothed.reset (sampleRate, rampTimeMs * 0.001);
}

void ParameterSmoother::reset (float value) noexcept
{
    smoothed.setCurrentAndTargetValue (value);
}

void ParameterSmoother::setTarget (float value) noexcept
{
    smoothed.setTargetValue (value);
}

float ParameterSmoother::getNextValue() noexcept
{
    return smoothed.getNextValue();
}

} // namespace miuglitch
