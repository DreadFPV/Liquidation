#pragma once

#include "Scale.h"
#include <array>
#include <vector>
#include <cstdint>

namespace liq
{
// Musical phrase generator: writes complete, repeating phrases in the current key.
// Deterministic: the same style + variation + settings always give the same phrase.
enum MelodyStyle { mlGoaLead = 0, mlRollingArp, mlAcid, mlStabs, mlDarkPulse, mlHook, mlReese, mlChords,
                   mlRollingBass, mlNumStyles };

enum MelodyTrigger { trigGrid = 0, trigKickStep, trigKickGate, trigBetweenKicks };

struct MelodyStep
{
    bool  on = false;
    int   degree = 0;     // scale degrees relative to the root (-3 .. 7)
    int   octave = 0;     // added octaves
    int   length = 1;     // in steps
    float velocity = 0.8f;
    bool  slide = false;
    bool  chord = false;  // play a triad on this step
};

struct MelodySettings
{
    int   style = mlGoaLead;
    int   variation = 1;  // 1..99
    float density = 0.7f; // 0..1
    int   range = 2;      // octaves 1..3
    int   steps = 32;     // phrase length in steps (16, 32, 64)
};

struct MelodyPhrase
{
    static constexpr int maxSteps = 64;
    std::array<MelodyStep, maxSteps> steps {};
    int length = 16;
};

MelodyPhrase generatePhrase (const MelodySettings& s);

// Midi note of a step (root = midi root note of the phrase).
int stepNote (const MelodyStep& st, const Scale& scale, int root);
}
