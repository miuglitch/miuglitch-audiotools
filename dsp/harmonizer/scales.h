#pragma once
#include <array>
#include <cstdint>

namespace GlitchCore
{

//==============================================================================
// Scale table for the Spectral Harmonizer's root + scale selector.
// Names and intervals follow the scale list in Ableton Live's scale chooser,
// in the same order. "Custom" is handled separately (24-note mask, see
// SpectralHarmonizer::Params::customMask).
//
// Each mask is a 12-bit pitch-class set relative to the root:
// bit n set => the note n semitones above the root is in the scale.
//==============================================================================

struct ScaleDef
{
    const char*   name;
    std::uint16_t mask;
};

namespace ScaleDetail
{
    constexpr std::uint16_t m (std::initializer_list<int> steps)
    {
        std::uint16_t r = 0;
        for (int s : steps) r = (std::uint16_t) (r | (1u << s));
        return r;
    }
}

inline constexpr std::array<ScaleDef, 36> kScales
{{
    { "Major",              ScaleDetail::m ({ 0, 2, 4, 5, 7, 9, 11 }) },
    { "Minor",              ScaleDetail::m ({ 0, 2, 3, 5, 7, 8, 10 }) },
    { "Dorian",             ScaleDetail::m ({ 0, 2, 3, 5, 7, 9, 10 }) },
    { "Mixolydian",         ScaleDetail::m ({ 0, 2, 4, 5, 7, 9, 10 }) },
    { "Lydian",             ScaleDetail::m ({ 0, 2, 4, 6, 7, 9, 11 }) },
    { "Phrygian",           ScaleDetail::m ({ 0, 1, 3, 5, 7, 8, 10 }) },
    { "Locrian",            ScaleDetail::m ({ 0, 1, 3, 5, 6, 8, 10 }) },
    { "Whole Tone",         ScaleDetail::m ({ 0, 2, 4, 6, 8, 10 }) },
    { "Half-whole Dim.",    ScaleDetail::m ({ 0, 1, 3, 4, 6, 7, 9, 10 }) },
    { "Whole-half Dim.",    ScaleDetail::m ({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
    { "Minor Blues",        ScaleDetail::m ({ 0, 3, 5, 6, 7, 10 }) },
    { "Minor Pentatonic",   ScaleDetail::m ({ 0, 3, 5, 7, 10 }) },
    { "Major Pentatonic",   ScaleDetail::m ({ 0, 2, 4, 7, 9 }) },
    { "Harmonic Minor",     ScaleDetail::m ({ 0, 2, 3, 5, 7, 8, 11 }) },
    { "Harmonic Major",     ScaleDetail::m ({ 0, 2, 4, 5, 7, 8, 11 }) },
    { "Dorian #4",          ScaleDetail::m ({ 0, 2, 3, 6, 7, 9, 10 }) },
    { "Phrygian Dominant",  ScaleDetail::m ({ 0, 1, 4, 5, 7, 8, 10 }) },
    { "Melodic Minor",      ScaleDetail::m ({ 0, 2, 3, 5, 7, 9, 11 }) },
    { "Lydian Augmented",   ScaleDetail::m ({ 0, 2, 4, 6, 8, 9, 11 }) },
    { "Lydian Dominant",    ScaleDetail::m ({ 0, 2, 4, 6, 7, 9, 10 }) },
    { "Super Locrian",      ScaleDetail::m ({ 0, 1, 3, 4, 6, 8, 10 }) },
    { "8-Tone Spanish",     ScaleDetail::m ({ 0, 1, 3, 4, 5, 6, 8, 10 }) },
    { "Bhairav",            ScaleDetail::m ({ 0, 1, 4, 5, 7, 8, 11 }) },
    { "Hungarian Minor",    ScaleDetail::m ({ 0, 2, 3, 6, 7, 8, 11 }) },
    { "Hirajoshi",          ScaleDetail::m ({ 0, 2, 3, 7, 8 }) },
    { "In-Sen",             ScaleDetail::m ({ 0, 1, 5, 7, 10 }) },
    { "Iwato",              ScaleDetail::m ({ 0, 1, 5, 6, 10 }) },
    { "Kumoi",              ScaleDetail::m ({ 0, 2, 3, 7, 9 }) },
    { "Pelog Selisir",      ScaleDetail::m ({ 0, 1, 3, 7, 8 }) },
    { "Pelog Tembung",      ScaleDetail::m ({ 0, 1, 5, 7, 8 }) },
    { "Messiaen 3",         ScaleDetail::m ({ 0, 2, 3, 4, 6, 7, 8, 10, 11 }) },
    { "Messiaen 4",         ScaleDetail::m ({ 0, 1, 2, 5, 6, 7, 8, 11 }) },
    { "Messiaen 5",         ScaleDetail::m ({ 0, 1, 5, 6, 7, 11 }) },
    { "Messiaen 6",         ScaleDetail::m ({ 0, 2, 4, 5, 6, 8, 10, 11 }) },
    { "Messiaen 7",         ScaleDetail::m ({ 0, 1, 2, 3, 5, 6, 7, 8, 9, 11 }) },
    { "Chromatic",          ScaleDetail::m ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }) },
}};

// Index of the "Custom" entry in a scale chooser built as kScales + "Custom".
inline constexpr int kCustomScaleIndex = (int) kScales.size();

} // namespace GlitchCore
