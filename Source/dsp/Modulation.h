#pragma once

#include "../Params.h"
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>

namespace liq
{
struct ModulatorParams
{
    int   type = modLfo;
    float rateHz = 1.0f;
    bool  sync = true;
    double divBeats = 1.0;
    float shape = 0.0f;      // LFO morph: sine, triangle, saw, square, random
    float phase = 0.0f;
    int   steps = 8;
    float glide = 0.0f;
    float attackMs = 10.0f, releaseMs = 150.0f, gainDb = 6.0f;
    double bpm = 120.0;
};

// LFO, curve sequencer or envelope follower (output 0..1).
class Modulator
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { phase = 0.0; env = 0.0f; value = 0.0f; seqValue = 0.0f; lastCycle = -1; held = 0.5f; nextHeld = 0.5f; }

    // Advance by n samples; 'peak' is the input envelope level over the sub-block.
    float advance (const ModulatorParams& p, int n, double ppq, bool playing, float peak,
                   const std::array<float, numSeqSteps>& steps) noexcept;

    float current() const noexcept { return value; }
    double currentPhase() const noexcept { return phase; }

    static float lfoShape (float shape, double ph, float randomHeld, float randomNext) noexcept;

private:
    double sr = 48000.0, phase = 0.0;
    float env = 0.0f, value = 0.0f, seqValue = 0.0f;
    long long lastCycle = -1;
    float held = 0.5f, nextHeld = 0.5f;
    uint32_t rng = 0xabcdefu;
    float rnd() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f); }
};

// Two macros + four modulators feeding a 16-slot matrix.
class ModSystem
{
public:
    void prepare (double sampleRate);
    void reset();

    struct MatrixSlot { int src = 0, dst = 0; float amount = 0.0f; };

    // Computes all sources and accumulates the offset (in normalised units) for each target.
    void process (const std::array<ModulatorParams, numModulators>& mp,
                  const std::array<MatrixSlot, numMatrixSlots>& matrix,
                  float macro1, float macro2, int n, double ppq, bool playing, float inputPeak);

    float offsetFor (int targetIndex) const noexcept
    {
        return targetIndex >= 0 && targetIndex < (int) offsets.size() ? offsets[(size_t) targetIndex] : 0.0f;
    }

    // UI access
    std::array<std::atomic<float>, numModSources> sourceValue {};
    std::array<std::atomic<float>, numModulators> modPhase {};
    std::array<std::array<float, numSeqSteps>, numModulators> seqSteps {};   // edited by the UI

private:
    std::array<Modulator, numModulators> mods;
    std::vector<float> offsets;
};
}
