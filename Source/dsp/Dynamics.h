#pragma once

#include "DelayLine.h"
#include <vector>
#include <atomic>

namespace liq
{
struct TransientParams
{
    bool  on = false;
    float sensitivity = 0.5f;  // 0..1
    float lengthMs = 40.0f;    // how long the original attack is kept
    float amount = 1.0f;       // 0..1
};

// Keeps the attack of the original signal: detects onsets on the live input and, using the
// plugin latency as look-ahead, crossfades to the time-aligned dry signal during each attack.
class TransientKeeper
{
public:
    void prepare (double sampleRate, int pathLatency);
    void reset();
    void process (const float* liveL, const float* liveR, const float* dryL, const float* dryR,
                  float* l, float* r, int n, const TransientParams& p) noexcept;
    std::atomic<float> gateDisplay { 0.0f };

private:
    double sr = 48000.0;
    FixedDelay gateDelay;
    float envFast = 0, envSlow = 0, gate = 0;
    int hold = 0, refractory = 0;
};

// Look-ahead peak limiter with constant latency (also when switched off).
class Limiter
{
public:
    static constexpr int lookahead = 64;
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, bool on, float ceilingDb = -0.3f) noexcept;
    std::atomic<float> reductionDb { 0.0f };

private:
    double sr = 48000.0;
    FixedDelay dl, dr;
    std::vector<float> targets;
    int tpos = 0;
    float gain = 1.0f;
    float displayRed = 0.0f;
};
}
