#pragma once

#include <array>
#include <cmath>

namespace liq
{
// A set of active pitch classes (absolute, C = 0) plus a root.
struct Scale
{
    std::array<bool, 12> mask {};
    int root = 0;
    bool chromatic = true;
    float a4 = 440.0f;

    // Preset intervals (relative to root) for scale type index (see scaleTypeNames()).
    static std::array<bool, 12> presetMask (int type, int root);

    void set (const std::array<bool, 12>& m, int rootPc, float tuneA4);

    bool contains (int midiNote) const noexcept { return mask[(size_t) (((midiNote % 12) + 12) % 12)]; }

    // Nearest note in scale to a (fractional) midi pitch.
    int nearest (float midi) const noexcept;

    // Move a scale note by a number of scale degrees (semitones if chromatic).
    int shiftDegrees (int midiNote, int degrees) const noexcept;

    // Semitone offset produced by stepping 'degrees' scale degrees from the root.
    int degreeToSemis (int degrees) const noexcept;

    // Map a 0..1 control onto scale notes between lo..hi midi (inclusive).
    int noteFromNormalised (float v, int lo, int hi) const noexcept;

    float midiToHz (float midi) const noexcept { return a4 * std::pow (2.0f, (midi - 69.0f) / 12.0f); }
    float hzToMidi (float hz) const noexcept   { return 69.0f + 12.0f * std::log2 (hz / a4); }

    // Quantise a frequency to the scale.
    float quantiseHz (float hz) const noexcept { return midiToHz ((float) nearest (hzToMidi (hz))); }
};
}
