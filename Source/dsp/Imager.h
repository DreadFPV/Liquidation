#pragma once

#include <vector>

namespace liq
{
struct ImagerParams
{
    bool  on = true;
    float dimension = 0.0f;     // 0..1
    float dimSize = 0.5f;       // 0..1
    float grainSizeMs = 100.0f; // used to adapt the dimension delays
    float grainRateHz = 10.0f;
    bool  monoLows = false;
    float monoFreq = 120.0f;
    float width = 1.0f;         // 0..2
    bool  safeWidth = true;
};

// Post-processing part of the Grain Imager: Dimension, Mono Lows and Stereo Width.
class Imager
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, const ImagerParams& p);

private:
    struct Lr4   // Linkwitz-Riley 4th order low/high split
    {
        float lpz[2][2] {}, hpz[2][2] {};
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;          // lowpass biquad
        float hb0 = 0, hb1 = 0, hb2 = 0;                      // highpass numerator
        void design (float fc, double sr);
        float low (float x, int stage) noexcept;
        float high (float x, int stage) noexcept;
    };

    double sr = 48000.0;
    std::vector<float> dimBuf;
    int dimPos = 0;
    double lfo = 0.0;
    float smoothedDelay = 10.0f;

    Lr4 xoverL, xoverR;
    float lastMonoFreq = -1.0f;
    float sideLp = 0.0f;
};
}
