#pragma once

#include <cstdint>

namespace liq
{
struct Scale;

// Settings captured by each grain when it is spawned.
struct GrainFilterParams
{
    int   mode = 0;          // FilterMode
    float cutoff = 2000.0f;  // Hz
    bool  scaleLock = false;
    bool  random = false;
    float modDepth = 0.0f;   // -1..1 (up to +-4 octaves)
    float modRate = 1.0f;    // cycles per grain
    float reso = 0.2f;       // 0..1
    float drive = 0.0f;      // 0..1
    float morph = 0.0f;      // 0..1
    float damp = 0.3f;       // 0..1
    float smooth = 0.0f;     // 0..1
    float bits = 16.0f;      // 1..16
    float mix = 1.0f;        // 0..1
};

// Per-grain filter. Holds its own state; delay memory is provided by the engine pool.
class GrainFilter
{
public:
    static constexpr int maxDelay = 2048;   // samples per channel (comb / flange)

    void start (const GrainFilterParams& p, const Scale& scale, double sampleRate,
                float* delayMemoryL, float* delayMemoryR, uint32_t& rng) noexcept;

    // Process one stereo sample. 'phase' is the grain's progress 0..1.
    void process (float& l, float& r, float phase) noexcept;

private:
    void updateCoefficients (float phase) noexcept;

    GrainFilterParams p;
    const Scale* scale = nullptr;
    double sr = 48000.0;
    float baseCutoff = 2000.0f;
    float randomOffset = 0.0f;   // octaves
    int counter = 0;

    // SVF state per channel (several bands for vowel / modal)
    struct Svf { float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0; };
    Svf svf[4];
    float ic1[2][4] {}, ic2[2][4] {};
    float bandFreqMul[4] { 1, 2, 3, 4 };
    float bandGain[4] { 1, 1, 1, 1 };

    // allpass stages (phaser / allpass)
    float apCoef[8] {};
    float apState[2][8] {};
    float fbSample[2] {};

    // comb / flange
    float* dl[2] { nullptr, nullptr };
    int dlPos = 0;
    float dlTime = 100.0f;
    float dampState[2] {};

    // reducer
    float holdValue[2] {};
    float holdCounter = 0.0f;
    float smoothState[2] {};

    float currentCutoff = 2000.0f;
};
}
