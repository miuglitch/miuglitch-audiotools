#pragma once

#include <juce_core/juce_core.h>

// Clip time display (Docs/Requirements.md):
//   under 10 s   -> milliseconds with 2 decimals   "9876.54"      unit "MS"
//   under 1 min  -> seconds:milliseconds           "45:120"       unit "S:MS"
//   1 min and up -> minutes:seconds:milliseconds   "02:45:644"    unit "M:S:MS"
// Minutes and seconds have two digits, milliseconds three.
struct ClipTime
{
    juce::String value, unit;

    // scaleMs picks the format (normally the clip length) so every time shown for one
    // clip uses the same format. Leave it out to pick by the value itself.
    static ClipTime format (double ms, double scaleMs = -1.0)
    {
        ms = juce::jmax (0.0, ms);
        const double scale = scaleMs >= 0.0 ? scaleMs : ms;

        if (scale < 10000.0)
            return { juce::String (ms, 2), "MS" };

        const auto total = (juce::int64) std::llround (ms);
        const auto milli = (int) (total % 1000);
        const auto secs  = (int) ((total / 1000) % 60);
        const auto mins  = (int) (total / 60000);

        auto two   = [] (int v) { return juce::String (v).paddedLeft ('0', 2); };
        auto three = [] (int v) { return juce::String (v).paddedLeft ('0', 3); };

        if (scale < 60000.0)
            return { two (secs) + ":" + three (milli), "S:MS" };

        return { two (mins) + ":" + two (secs) + ":" + three (milli), "M:S:MS" };
    }
};
