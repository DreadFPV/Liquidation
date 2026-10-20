#pragma once

#include "FdnReverb.h"
#include <vector>

namespace liq
{
struct GrainDelayParams
{
    bool  on = false;
    bool  pingPong = false;
    float feedback = 0.3f;       // 0..0.98
    float space = 0.0f;          // 0..1
    float lowpass = 20000.0f;    // Hz
    float highpass = 20.0f;      // Hz
    float mix = 0.4f;            // 0..1
    double loopSamples = 12000;  // time until the tap pattern repeats
};

// Handles the parts of the grain delay that are not grains themselves:
// feedback (with filtering, not re-pitched), the Space reverb and the mix.
// The taps are spawned as grains by the engine and arrive on the tap bus.
class GrainDelay
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    // main = original grains, taps = delayed grains; result written to outL/outR.
    void process (const float* mainL, const float* mainR, const float* tapL, const float* tapR,
                  float* outL, float* outR, int n, const GrainDelayParams& p);

private:
    struct OnePole
    {
        float z = 0.0f;
        float lp (float x, float a) noexcept { z += a * (x - z); return z; }
    };

    double sr = 48000.0;
    std::vector<float> fbL, fbR;
    int fbPos = 0;
    OnePole lpL, lpR, hpL, hpR, lpOutL, lpOutR, hpOutL, hpOutR;
    FdnReverb space;
    std::vector<float> busL, busR, revL, revR;
    float smoothedLoop = 12000.0f;
};
}
