#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DelayLine.h"
#include <array>
#include <vector>
#include <complex>
#include <atomic>

namespace liq
{
enum MorphAlgo { morphBlend = 0, morphImprint, morphCarve, morphWeave, morphDuck, morphNumAlgos };

struct MorphParams
{
    bool  morphOn = false;
    float x = 0.5f;          // X-Fade: 0 = A, 1 = B
    float y = 1.0f;          // Morph depth: 0 = plain crossfade, 1 = full algorithm
    int   algo = morphBlend;
    float gamma = 1.0f;      // spectral contrast (0.25 .. 4)
    float detail = 0.5f;     // spectral envelope resolution (0 coarse .. 1 fine)
    bool  formantOn = false;
    float formantShift = 0;  // semitones
};

// STFT processor that morphs signal A with signal B and optionally shifts formants.
// Latency is constant (fftSize) whether or not processing is active, so parallel paths stay aligned.
class SpectralMorph
{
public:
    void prepare (double sampleRate);
    void reset();
    int latency() const noexcept { return fftSize; }

    void process (const float* aL, const float* aR, const float* bL, const float* bR,
                  float* outL, float* outR, int n, const MorphParams& p) noexcept;

    bool hasSignalB() const noexcept { return chans[0].presence > 0.5f || chans[1].presence > 0.5f; }

    // UI: smoothed magnitude spectra (0..1 scaled, log frequency) of A, B and output.
    static constexpr int displayBins = 96;
    std::array<std::atomic<float>, displayBins> dispA {}, dispB {}, dispOut {};

private:
    void processFrame (int ch, const MorphParams& p) noexcept;
    void smoothEnvelope (const float* mag, float* env, float q) noexcept;

    static constexpr int order = 11;
    static constexpr int fftSize = 1 << order;   // 2048
    static constexpr int hop = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;

    double sr = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;

    struct Channel
    {
        std::vector<float> inA, inB, acc;       // ring buffers (fftSize)
        std::vector<float> duckGain;            // per bin, for Duck smoothing
        FixedDelay bypass;                      // A delayed by fftSize when inactive
        float presence = 0.0f;                  // 0 = no B signal, 1 = B present
    };
    std::array<Channel, 2> chans;
    int pos = 0, hopCounter = 0;

    // scratch
    std::vector<float> bufA, bufB, magA, magB, envA, envB, magS, envS, prefix;
    std::vector<std::complex<float>> specA, specB, specO;

    float activeGain = 0.0f;   // crossfade between bypass delay and STFT output
    int   warmup = 0;          // samples of complete overlap-add since activation
    float roundTripScale = 1.0f;
};
}
