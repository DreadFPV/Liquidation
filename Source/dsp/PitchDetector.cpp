#include "PitchDetector.h"
#include <cmath>
#include <algorithm>

namespace liq
{
namespace
{
    void designLowpass (float& b0, float& b1, float& b2, float& a1, float& a2, double fc, double fs, double q)
    {
        const double w = 2.0 * 3.14159265358979 * fc / fs;
        const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 - cw) * 0.5 / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
}

void PitchDetector::prepare (double sampleRate, int64_t historySamples)
{
    sr = sampleRate;
    decim = std::max (1, (int) std::lround (sr / 12000.0));
    dsr = sr / decim;
    window = (int) std::lround (dsr * 0.042);              // ~42 ms
    minTau = std::max (2, (int) (dsr / 1100.0));          // up to ~1.1 kHz
    maxTau = (int) (dsr / 50.0);                          // down to 50 Hz

    designLowpass (lp1.b0, lp1.b1, lp1.b2, lp1.a1, lp1.a2, dsr * 0.42, sr, 0.541);
    designLowpass (lp2.b0, lp2.b1, lp2.b2, lp2.a1, lp2.a2, dsr * 0.42, sr, 1.307);

    ring.assign ((size_t) (window + maxTau + 8), 0.0f);
    frame.assign (ring.size(), 0.0f);
    diff.assign ((size_t) maxTau + 2, 0.0f);
    table.assign ((size_t) (historySamples / hop + 4), -1.0f);
    reset();
}

void PitchDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (table.begin(), table.end(), -1.0f);
    ringPos = 0; decimCounter = 0; lastAnalysis = 0;
    lp1.z1 = lp1.z2 = lp2.z1 = lp2.z2 = 0;
}

void PitchDetector::push (float x, int64_t absIndex) noexcept
{
    const float y = lp2.process (lp1.process (x));
    if (++decimCounter >= decim)
    {
        decimCounter = 0;
        ring[(size_t) ringPos] = y;
        if (++ringPos >= (int) ring.size()) ringPos = 0;
    }

    if (absIndex - lastAnalysis >= (int64_t) hop * hopMultiplier)
    {
        lastAnalysis = absIndex;
        analyse (absIndex);
    }
}

void PitchDetector::analyse (int64_t absIndex) noexcept
{
    const int n = (int) ring.size();
    // unroll ring into chronological frame
    for (int i = 0; i < n; ++i)
        frame[(size_t) i] = ring[(size_t) ((ringPos + i) % n)];

    const float* xs = frame.data() + (n - window - maxTau);

    // energy gate
    double energy = 0.0;
    for (int j = 0; j < window; ++j) energy += xs[j] * xs[j];
    const double rms = std::sqrt (energy / window);

    float result = -1.0f;

    if (rms > 0.004)
    {
        // YIN difference + cumulative mean normalised difference
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double d = 0.0;
            for (int j = 0; j < window; ++j)
            {
                const float delta = xs[j] - xs[j + tau];
                d += delta * delta;
            }
            running += d;
            diff[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = minTau; tau < maxTau; ++tau)
        {
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxTau && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }

        if (best > 0)
        {
            // parabolic interpolation
            const float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const float denom = s0 + s2 - 2.0f * s1;
            const float shift = std::abs (denom) > 1.0e-9f ? 0.5f * (s0 - s2) / denom : 0.0f;
            const double period = (best + std::clamp (shift, -1.0f, 1.0f)) / dsr;
            const double hz = 1.0 / period;
            result = (float) (69.0 + 12.0 * std::log2 (hz / 440.0));   // relative to 440; retuned later
        }
    }

    // the analysed frame is centred roughly half a window in the past
    const int64_t centre = absIndex - (int64_t) ((window + maxTau) * decim) / 2;
    if (centre < 0) return;
    const auto slot = (size_t) ((centre / hop) % (int64_t) table.size());
    table[slot] = result;
    // fill the hop before as well when analysing at reduced rate
    if (hopMultiplier > 1)
        table[(slot + table.size() - 1) % table.size()] = result;
}

float PitchDetector::pitchAt (int64_t absIndex) const noexcept
{
    if (absIndex < 0 || table.empty()) return -1.0f;
    return table[(size_t) ((absIndex / hop) % (int64_t) table.size())];
}
}
