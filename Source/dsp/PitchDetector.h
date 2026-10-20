#pragma once

#include <vector>
#include <cstdint>

namespace liq
{
// Continuous monophonic pitch tracker (YIN on a decimated signal).
// Results are stored per hop and indexed by absolute sample position, so
// each grain can look up the pitch of the audio it is about to play.
class PitchDetector
{
public:
    void prepare (double sampleRate, int64_t historySamples);
    void reset();

    // Feed one mono input sample whose absolute index is 'absIndex'.
    void push (float x, int64_t absIndex) noexcept;

    // Detected pitch (fractional midi note) near an absolute sample index, or -1 if unvoiced.
    float pitchAt (int64_t absIndex) const noexcept;

    // Low-precision mode: analyse less often (cheaper, less accurate on fast material).
    void setLowPrecision (bool b) noexcept { hopMultiplier = b ? 2 : 1; }

    static constexpr int hop = 256;

    // How far behind the write head the newest analysed frame is centred.
    int64_t latency() const noexcept { return (int64_t) ((window + maxTau) * decim) / 2 + hop * 2; }

private:
    void analyse (int64_t absIndex) noexcept;

    double sr = 48000.0, dsr = 12000.0;
    int decim = 4, decimCounter = 0, hopMultiplier = 1;
    int window = 512, minTau = 12, maxTau = 240;

    // decimation lowpass (2x biquad)
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    float process (float x) noexcept { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    Biquad lp1, lp2;

    std::vector<float> ring;      // decimated samples
    int ringPos = 0;
    std::vector<float> frame, diff;
    std::vector<float> table;     // per-hop results
    int64_t lastAnalysis = 0;
};
}
