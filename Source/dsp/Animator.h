#pragma once

#include <atomic>
#include <cstdint>

namespace liq
{
struct AnimatorParams
{
    float x = 0.0f, y = 0.0f;   // macro handle (0..1)
    float depth = 0.0f, drift = 0.0f, warp = 0.3f;
    int   mode = 0;             // 0 orbit, 1 weave
    int   cycles = 1;
    float rateHz = 0.25f;
    bool  sync = false;
    double divBeats = 4.0;
};

// Traces an orbit (hypotrochoid) or weave (lissajous) figure around the pad position
// and returns the two animated macro values.
class Animator
{
public:
    void prepare (double sampleRate) { sr = sampleRate; t = 0.0; }
    void advance (const AnimatorParams& p, int n, double ppq, double bpm, bool playing) noexcept;

    float macro1() const noexcept { return m1; }
    float macro2() const noexcept { return m2; }

    // Shape point (-1..1) for a parameter t in 0..1 — also used by the UI to draw the path.
    static void shapePoint (int mode, int cycles, float warp, double t01, float& x, float& y) noexcept;

    std::atomic<float> cursorX { 0 }, cursorY { 0 };

private:
    double sr = 48000.0, t = 0.0;
    float m1 = 0.0f, m2 = 0.0f;
    float driftX = 0, driftY = 0, driftTX = 0, driftTY = 0;
    int driftCounter = 0;
    uint32_t rng = 0x5eedu;
    float rnd() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f) * 2.0f - 1.0f; }
};
}
