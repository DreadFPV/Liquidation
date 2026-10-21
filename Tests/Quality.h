#pragma once
// Objective transparency / artefact measurements, shared by the test runner.
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <vector>
#include <cmath>

namespace liqtest
{
struct QualityResult
{
    double residualDb = 0;   // error after best alignment, relative to input (lower = cleaner)
    double ampRippleDb = 0;  // peak-to-peak variation of the short-term RMS on a steady tone
    int lag = 0;
    double levelDb = 0;
};

// Runs 'seconds' of mono signal through proc (stereo in/out), returns the left output.
inline std::vector<float> render (juce::AudioProcessor& proc, const std::function<float (int64_t)>& sig, double seconds,
                                  std::vector<float>* inputOut = nullptr, int channel = 0)
{
    const int block = 512;
    juce::AudioBuffer<float> b (2, block);
    juce::MidiBuffer m;
    std::vector<float> out;
    const int64_t total = (int64_t) (seconds * 48000.0);
    for (int64_t pos = 0; pos < total; pos += block)
    {
        for (int k = 0; k < block; ++k)
        {
            const float x = sig (pos + k);
            b.setSample (0, k, x); b.setSample (1, k, x);
            if (inputOut) inputOut->push_back (x);
        }
        proc.processBlock (b, m);
        for (int k = 0; k < block; ++k) out.push_back (b.getSample (channel, k));
    }
    return out;
}

inline QualityResult measure (const std::vector<float>& in, const std::vector<float>& out, int maxLag = 6000)
{
    QualityResult r;
    const size_t start = in.size() / 2, end = in.size() - (size_t) maxLag - 10;
    double best = 1e30; int bestLag = 0;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double e = 0;
        for (size_t i = start; i < end; i += 7) { const double d = out[i + (size_t) lag] - in[i]; e += d * d; }
        if (e < best) { best = e; bestLag = lag; }
    }
    double err = 0, ref = 0, o = 0;
    for (size_t i = start; i < end; ++i)
    {
        const double d = out[i + (size_t) bestLag] - in[i];
        err += d * d; ref += (double) in[i] * in[i]; o += (double) out[i + (size_t) bestLag] * out[i + (size_t) bestLag];
    }
    r.lag = bestLag;
    r.residualDb = 10.0 * std::log10 (err / ref + 1e-12);
    r.levelDb = 10.0 * std::log10 (o / ref + 1e-12);

    // short-term RMS ripple (50 ms windows, long enough to hold many periods)
    const size_t w = 2400;
    double mn = 1e9, mx = 0;
    for (size_t i = start; i + w < end; i += w)
    {
        double s = 0;
        for (size_t k = 0; k < w; ++k) s += (double) out[i + k + (size_t) bestLag] * out[i + k + (size_t) bestLag];
        const double rms = std::sqrt (s / w);
        mn = std::min (mn, rms); mx = std::max (mx, rms);
    }
    r.ampRippleDb = 20.0 * std::log10 ((mx + 1e-9) / (mn + 1e-9));
    return r;
}
}
