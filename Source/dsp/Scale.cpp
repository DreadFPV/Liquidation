#include "Scale.h"
#include <algorithm>
#include <vector>

namespace liq
{
std::array<bool, 12> Scale::presetMask (int type, int root)
{
    static const std::vector<std::vector<int>> intervals {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, // Chromatic
        { 0, 2, 4, 5, 7, 9, 11 },                  // Major
        { 0, 2, 3, 5, 7, 8, 10 },                  // Minor
        { 0, 2, 3, 5, 7, 8, 11 },                  // Harmonic minor
        { 0, 2, 3, 5, 7, 9, 11 },                  // Melodic minor
        { 0, 2, 3, 5, 7, 9, 10 },                  // Dorian
        { 0, 1, 3, 5, 7, 8, 10 },                  // Phrygian
        { 0, 2, 4, 6, 7, 9, 11 },                  // Lydian
        { 0, 2, 4, 5, 7, 9, 10 },                  // Mixolydian
        { 0, 1, 3, 5, 6, 8, 10 },                  // Locrian
        { 0, 2, 4, 7, 9 },                         // Major pentatonic
        { 0, 3, 5, 7, 10 },                        // Minor pentatonic
        { 0, 3, 5, 6, 7, 10 },                     // Blues
        { 0, 1, 4, 5, 7, 8, 10 },                  // Phrygian dominant
        { 0, 2, 3, 6, 7, 8, 11 },                  // Hungarian minor
        { 0, 1, 4, 5, 7, 8, 11 },                  // Double harmonic
        { 0, 2, 4, 6, 8, 10 },                     // Whole tone
    };

    std::array<bool, 12> m {};
    const auto t = (size_t) std::clamp (type, 0, (int) intervals.size() - 1);
    for (int iv : intervals[t])
        m[(size_t) ((root + iv) % 12)] = true;
    return m;
}

void Scale::set (const std::array<bool, 12>& m, int rootPc, float tuneA4)
{
    mask = m;
    root = ((rootPc % 12) + 12) % 12;
    a4 = tuneA4;

    int count = 0;
    for (bool b : mask) count += b ? 1 : 0;
    if (count == 0) { mask.fill (true); count = 12; }   // never allow an empty scale
    chromatic = (count == 12);
}

int Scale::nearest (float midi) const noexcept
{
    const int base = (int) std::lround (midi);
    for (int d = 0; d <= 12; ++d)
    {
        // prefer the closer of the two candidates
        const int up = base + d, dn = base - d;
        const bool hasUp = contains (up), hasDn = contains (dn);
        if (hasUp && hasDn)
            return (std::abs ((float) up - midi) <= std::abs ((float) dn - midi)) ? up : dn;
        if (hasUp) return up;
        if (hasDn) return dn;
    }
    return base;
}

int Scale::shiftDegrees (int note, int degrees) const noexcept
{
    if (chromatic) return note + degrees;
    int n = nearest ((float) note);
    const int dir = degrees >= 0 ? 1 : -1;
    for (int k = 0; k < std::abs (degrees); ++k)
    {
        do { n += dir; } while (! contains (n));
    }
    return n;
}

int Scale::degreeToSemis (int degrees) const noexcept
{
    const int r = 60 + ((root - 60 % 12) + 12) % 12; // root pitch class near middle C
    return shiftDegrees (r, degrees) - r;
}

int Scale::noteFromNormalised (float v, int lo, int hi) const noexcept
{
    int notes[128]; int n = 0;
    for (int k = lo; k <= hi && n < 128; ++k)
        if (contains (k)) notes[n++] = k;
    if (n == 0) return lo;
    const int idx = std::clamp ((int) std::lround (v * (float) (n - 1)), 0, n - 1);
    return notes[idx];
}
}
