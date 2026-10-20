#include "Modulation.h"
#include <cmath>
#include <algorithm>

namespace liq
{
float Modulator::lfoShape (float shape, double ph, float randomHeld, float randomNext) noexcept
{
    const float p = (float) ph;
    const float sine = 0.5f - 0.5f * std::cos (6.2831853f * p);
    const float tri  = p < 0.5f ? p * 2.0f : 2.0f - p * 2.0f;
    const float saw  = p;
    const float sq   = p < 0.5f ? 1.0f : 0.0f;
    juce::ignoreUnused (randomNext);
    const float rnd  = randomHeld;   // sample & hold
    const float s = std::clamp (shape, 0.0f, 1.0f) * 4.0f;
    const int i = std::min (3, (int) s);
    const float t = s - (float) i;
    const float v[5] { sine, tri, saw, sq, rnd };
    return v[i] + (v[i + 1] - v[i]) * t;
}

float Modulator::advance (const ModulatorParams& p, int n, double ppq, bool playing, float peak,
                          const std::array<float, numSeqSteps>& steps) noexcept
{
    if (p.type == modEnvelope)
    {
        const float target = std::min (1.0f, peak * std::pow (10.0f, p.gainDb / 20.0f));
        const float ms = target > env ? p.attackMs : p.releaseMs;
        const float a = 1.0f - std::exp (-(float) n / (std::max (0.05f, ms) * 0.001f * (float) sr));
        env += a * (target - env);
        value = std::clamp (env, 0.0f, 1.0f);
        return value;
    }

    // ---- phase (tempo-locked when synced and the host is playing)
    double cycles;
    if (p.sync && playing)
    {
        cycles = ppq / std::max (1.0e-6, p.divBeats);
        phase = cycles - std::floor (cycles);
    }
    else
    {
        const double hz = p.sync ? p.bpm / 60.0 / std::max (1.0e-6, p.divBeats) : (double) p.rateHz;
        phase += hz * (double) n / sr;
        cycles = (double) lastCycle + phase;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            cycles = (double) lastCycle + 1.0 + phase;
        }
    }

    double ph = phase + p.phase;
    ph -= std::floor (ph);

    const long long cyc = (long long) std::floor (cycles);
    if (cyc != lastCycle)
    {
        lastCycle = cyc;
        held = nextHeld;
        nextHeld = rnd();
    }

    if (p.type == modLfo)
    {
        value = lfoShape (p.shape, ph, held, nextHeld);
    }
    else
    {
        const int count = std::clamp (p.steps, 2, numSeqSteps);
        const int step = std::min (count - 1, (int) (ph * count));
        const float target = std::clamp (steps[(size_t) step], 0.0f, 1.0f);
        if (p.glide <= 0.001f) seqValue = target;
        else
        {
            const double stepSeconds = (p.sync ? p.divBeats * 60.0 / p.bpm : 1.0 / std::max (0.01f, p.rateHz)) / count;
            const float tau = (float) (stepSeconds * p.glide * 0.5);
            const float a = 1.0f - std::exp (-(float) n / (std::max (1.0e-4f, tau) * (float) sr));
            seqValue += a * (target - seqValue);
        }
        value = seqValue;
    }
    return value;
}

void ModSystem::prepare (double sampleRate)
{
    for (auto& m : mods) m.prepare (sampleRate);
    offsets.assign ((size_t) modTargetIds().size(), 0.0f);
    for (auto& s : seqSteps)
        for (int i = 0; i < numSeqSteps; ++i)
            s[(size_t) i] = (i % 4 == 0) ? 1.0f : ((i % 2 == 0) ? 0.5f : 0.15f);
}

void ModSystem::reset()
{
    for (auto& m : mods) m.reset();
    std::fill (offsets.begin(), offsets.end(), 0.0f);
}

void ModSystem::process (const std::array<ModulatorParams, numModulators>& mp,
                         const std::array<MatrixSlot, numMatrixSlots>& matrix,
                         float macro1, float macro2, int n, double ppq, bool playing, float inputPeak)
{
    std::array<float, numModSources> src {};
    src[0] = 0.0f;
    src[1] = macro1;
    src[2] = macro2;
    for (int m = 0; m < numModulators; ++m)
    {
        src[(size_t) m + 3] = mods[(size_t) m].advance (mp[(size_t) m], n, ppq, playing, inputPeak, seqSteps[(size_t) m]);
        modPhase[(size_t) m].store ((float) mods[(size_t) m].currentPhase(), std::memory_order_relaxed);
    }
    for (int s = 0; s < numModSources; ++s)
        sourceValue[(size_t) s].store (src[(size_t) s], std::memory_order_relaxed);

    std::fill (offsets.begin(), offsets.end(), 0.0f);
    for (const auto& slot : matrix)
    {
        if (slot.src <= 0 || slot.dst <= 0 || slot.src >= numModSources) continue;
        const int t = slot.dst - 1;
        if (t >= (int) offsets.size()) continue;
        offsets[(size_t) t] += slot.amount * src[(size_t) slot.src];
    }
}
}
