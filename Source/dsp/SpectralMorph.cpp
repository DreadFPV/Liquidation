#include "SpectralMorph.h"
#include <cmath>
#include <algorithm>

namespace liq
{
namespace
{
    constexpr float twoPi = 6.283185307179586f;
    inline float dbNorm (float mag, float ref)
    {
        const float db = 20.0f * std::log10 (mag / ref + 1.0e-9f);
        return std::clamp ((db + 84.0f) / 84.0f, 0.0f, 1.0f);
    }
}

void SpectralMorph::prepare (double sampleRate)
{
    sr = sampleRate;
    fft = std::make_unique<juce::dsp::FFT> (order);

    window.resize ((size_t) fftSize);
    for (int j = 0; j < fftSize; ++j)
        window[(size_t) j] = std::sqrt (0.5f - 0.5f * std::cos (twoPi * (float) j / (float) fftSize));

    for (auto& c : chans)
    {
        c.inA.assign ((size_t) fftSize, 0.0f);
        c.inB.assign ((size_t) fftSize, 0.0f);
        c.acc.assign ((size_t) fftSize, 0.0f);
        c.duckGain.assign ((size_t) numBins, 1.0f);
        c.bypass.prepare (fftSize);
    }
    bufA.assign ((size_t) fftSize * 2, 0.0f);
    bufB.assign ((size_t) fftSize * 2, 0.0f);
    for (auto* v : { &magA, &magB, &envA, &envB, &magS, &envS })
        v->assign ((size_t) numBins, 0.0f);
    prefix.assign ((size_t) numBins + 1, 0.0f);
    specA.assign ((size_t) numBins, {});
    specB.assign ((size_t) numBins, {});
    specO.assign ((size_t) numBins, {});

    // calibrate the round-trip gain of this FFT engine (differs between platforms)
    {
        std::vector<float> x ((size_t) fftSize * 2, 0.0f), y;
        juce::Random r (1);
        for (int j = 0; j < fftSize; ++j) x[(size_t) j] = r.nextFloat() - 0.5f;
        y = x;
        fft->performRealOnlyForwardTransform (y.data(), true);
        fft->performRealOnlyInverseTransform (y.data());
        double num = 0, den = 0;
        for (int j = 0; j < fftSize; ++j) { num += (double) y[(size_t) j] * x[(size_t) j]; den += (double) x[(size_t) j] * x[(size_t) j]; }
        const double c = num / den;
        // sqrt-Hann analysis * synthesis = Hann; Hann at 75 % overlap sums to 2
        roundTripScale = (float) (1.0 / (c * 2.0));
    }
    reset();
}

void SpectralMorph::reset()
{
    for (auto& c : chans)
    {
        std::fill (c.inA.begin(), c.inA.end(), 0.0f);
        std::fill (c.inB.begin(), c.inB.end(), 0.0f);
        std::fill (c.acc.begin(), c.acc.end(), 0.0f);
        std::fill (c.duckGain.begin(), c.duckGain.end(), 1.0f);
        c.bypass.reset();
        c.presence = 0.0f;
    }
    pos = 0; hopCounter = 0;
    activeGain = 0.0f; warmup = 0;
    for (auto& d : dispA) d.store (0.0f);
    for (auto& d : dispB) d.store (0.0f);
    for (auto& d : dispOut) d.store (0.0f);
}

void SpectralMorph::smoothEnvelope (const float* mag, float* env, float q) noexcept
{
    prefix[0] = 0.0f;
    for (int k = 0; k < numBins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + mag[k];
    for (int k = 0; k < numBins; ++k)
    {
        const int w = std::max (1, (int) ((float) k * q));
        const int lo = std::max (0, k - w), hi = std::min (numBins - 1, k + w);
        env[k] = (prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (float) (hi - lo + 1) + 1.0e-9f;
    }
}

void SpectralMorph::processFrame (int ch, const MorphParams& p) noexcept
{
    auto& c = chans[(size_t) ch];

    for (int j = 0; j < fftSize; ++j)
    {
        const int idx = (pos + j) & (fftSize - 1);
        bufA[(size_t) j] = c.inA[(size_t) idx] * window[(size_t) j];
        bufB[(size_t) j] = c.inB[(size_t) idx] * window[(size_t) j];
    }
    std::fill (bufA.begin() + fftSize, bufA.end(), 0.0f);
    std::fill (bufB.begin() + fftSize, bufB.end(), 0.0f);
    fft->performRealOnlyForwardTransform (bufA.data(), true);
    if (p.morphOn) fft->performRealOnlyForwardTransform (bufB.data(), true);

    float peakA = 1.0e-9f, peakB = 1.0e-9f;
    for (int k = 0; k < numBins; ++k)
    {
        specA[(size_t) k] = { bufA[(size_t) (2 * k)], bufA[(size_t) (2 * k + 1)] };
        specB[(size_t) k] = p.morphOn ? std::complex<float> { bufB[(size_t) (2 * k)], bufB[(size_t) (2 * k + 1)] }
                                      : std::complex<float> {};
        magA[(size_t) k] = std::abs (specA[(size_t) k]);
        magB[(size_t) k] = std::abs (specB[(size_t) k]);
        peakA = std::max (peakA, magA[(size_t) k]);
        peakB = std::max (peakB, magB[(size_t) k]);
    }

    const float q = 0.25f + (0.02f - 0.25f) * std::clamp (p.detail, 0.0f, 1.0f);

    // without a B signal (e.g. sidechain not connected) the morph fades to A instead of going silent
    {
        const float bDb = 20.0f * std::log10 (peakB / ((float) fftSize * 0.25f) + 1.0e-12f);
        const float presence = std::clamp ((bDb + 80.0f) / 30.0f, 0.0f, 1.0f);
        c.presence += (presence > c.presence ? 0.5f : 0.15f) * (presence - c.presence);
    }
    const float presenceB = c.presence;

    if (! p.morphOn || presenceB < 0.001f)
    {
        for (int k = 0; k < numBins; ++k) specO[(size_t) k] = specA[(size_t) k];
    }
    else
    {
        const float x = std::clamp (p.x, 0.0f, 1.0f) * presenceB;
        const float y = std::clamp (p.y, 0.0f, 1.0f) * presenceB;
        const float gA = std::cos (x * 1.5707963f), gB = std::sin (x * 1.5707963f);
        const bool needEnv = p.algo == morphImprint || p.algo == morphCarve || p.algo == morphDuck;
        if (needEnv)
        {
            smoothEnvelope (magA.data(), envA.data(), q);
            smoothEnvelope (magB.data(), envB.data(), q);
        }
        const float floorA = peakA * 1.0e-3f, floorB = peakB * 1.0e-3f;
        const float duckRelease = (float) hop / (float) (sr * 0.09);   // gain recovery per frame

        // ---- spectral algorithm -> magS (magnitude) and specO (temporarily holds the algorithm result)
        double e1 = 0.0;
        for (int k = 0; k < numBins; ++k)
        {
            const auto A = specA[(size_t) k], B = specB[(size_t) k];
            const float ma = magA[(size_t) k], mb = magB[(size_t) k];
            std::complex<float> S;
            switch (p.algo)
            {
                case morphImprint:
                {
                    const float g = std::pow (envB[(size_t) k] / envA[(size_t) k], x);
                    S = A * std::min (g, 32.0f);
                    break;
                }
                case morphCarve:
                {
                    const float g = std::pow (envA[(size_t) k] / envB[(size_t) k], 1.0f - x);
                    S = B * std::min (g, 32.0f);
                    break;
                }
                case morphWeave:
                {
                    const float wa = std::pow (ma * (1.0f - x) + 1.0e-12f, 4.0f);
                    const float wb = std::pow (mb * x + 1.0e-12f, 4.0f);
                    S = (A * wa + B * wb) / (wa + wb);
                    break;
                }
                case morphDuck:
                {
                    const float r = envB[(size_t) k] / (envA[(size_t) k] + floorA);
                    const float g = 1.0f / (1.0f + 4.0f * x * r * r);
                    float& dg = c.duckGain[(size_t) k];
                    dg = std::min (g, dg + duckRelease);
                    S = A * dg;
                    break;
                }
                case morphBlend:
                default:
                {
                    const float m = std::exp ((1.0f - x) * std::log (ma + floorA) + x * std::log (mb + floorB));
                    const auto lin = A * gA + B * gB;
                    const float ml = std::abs (lin);
                    S = ml > 1.0e-12f ? lin * (m / ml) : (ma >= mb ? A : B) * (m / std::max (std::max (ma, mb), 1.0e-12f));
                    break;
                }
            }
            specO[(size_t) k] = S;
            magS[(size_t) k] = std::abs (S);
            e1 += (double) magS[(size_t) k] * magS[(size_t) k];
        }

        // ---- gamma: spectral contrast, energy preserving
        if (std::abs (p.gamma - 1.0f) > 0.01f && e1 > 1.0e-20)
        {
            const float peak = *std::max_element (magS.begin(), magS.end()) + 1.0e-12f;
            double e2 = 0.0;
            for (int k = 0; k < numBins; ++k)
            {
                const float m = magS[(size_t) k];
                const float m2 = peak * std::pow (m / peak, p.gamma);
                envS[(size_t) k] = m > 1.0e-12f ? m2 / m : 0.0f;
                e2 += (double) m2 * m2;
            }
            const float norm = e2 > 1.0e-20 ? (float) std::sqrt (e1 / e2) : 1.0f;
            for (int k = 0; k < numBins; ++k) specO[(size_t) k] *= envS[(size_t) k] * norm;
        }

        // ---- combine: Y = 0 plain crossfade, Y = 1 full algorithm
        for (int k = 0; k < numBins; ++k)
        {
            const auto lin = specA[(size_t) k] * gA + specB[(size_t) k] * gB;
            specO[(size_t) k] = lin * (1.0f - y) + specO[(size_t) k] * y;
        }
    }

    // ---- formant shift on the result
    if (p.formantOn && std::abs (p.formantShift) > 0.01f)
    {
        for (int k = 0; k < numBins; ++k) magS[(size_t) k] = std::abs (specO[(size_t) k]);
        smoothEnvelope (magS.data(), envS.data(), std::max (q, 0.06f));
        const float f = std::pow (2.0f, p.formantShift / 12.0f);
        for (int k = 1; k < numBins; ++k)
        {
            const float src = (float) k / f;
            const int i0 = std::min ((int) src, numBins - 2);
            const float t = std::clamp (src - (float) i0, 0.0f, 1.0f);
            const float shifted = src >= (float) (numBins - 1) ? envS[(size_t) numBins - 1]
                                                               : envS[(size_t) i0] + (envS[(size_t) i0 + 1] - envS[(size_t) i0]) * t;
            specO[(size_t) k] *= std::clamp (shifted / envS[(size_t) k], 0.0f, 8.0f);
        }
    }

    // ---- display (left channel only)
    if (ch == 0)
    {
        const float ref = (float) fftSize * 0.25f;
        for (int d = 0; d < displayBins; ++d)
        {
            const float hz = 30.0f * std::pow (600.0f, (float) d / (float) (displayBins - 1));
            const int k = std::clamp ((int) (hz / (float) sr * (float) fftSize), 1, numBins - 1);
            auto upd = [] (std::atomic<float>& a, float v) { a.store (a.load (std::memory_order_relaxed) * 0.7f + v * 0.3f, std::memory_order_relaxed); };
            upd (dispA[(size_t) d], dbNorm (magA[(size_t) k], ref));
            upd (dispB[(size_t) d], dbNorm (magB[(size_t) k], ref));
            upd (dispOut[(size_t) d], dbNorm (std::abs (specO[(size_t) k]), ref));
        }
    }

    // ---- inverse
    for (int k = 0; k < numBins; ++k)
    {
        bufA[(size_t) (2 * k)] = specO[(size_t) k].real();
        bufA[(size_t) (2 * k + 1)] = (k == 0 || k == numBins - 1) ? 0.0f : specO[(size_t) k].imag();
    }
    std::fill (bufA.begin() + 2 * numBins, bufA.end(), 0.0f);
    fft->performRealOnlyInverseTransform (bufA.data());

    for (int j = 0; j < fftSize; ++j)
    {
        const int idx = (pos + j) & (fftSize - 1);
        c.acc[(size_t) idx] += bufA[(size_t) j] * window[(size_t) j] * roundTripScale;
    }
}

void SpectralMorph::process (const float* aL, const float* aR, const float* bL, const float* bR,
                             float* outL, float* outR, int n, const MorphParams& p) noexcept
{
    const bool active = p.morphOn || (p.formantOn && std::abs (p.formantShift) > 0.01f);
    const float step = 1.0f / (float) (0.03 * sr);
    const float* a[2] { aL, aR };
    const float* b[2] { bL, bR };
    float* o[2] { outL, outR };

    for (int i = 0; i < n; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& c = chans[(size_t) ch];
            c.inA[(size_t) pos] = a[ch][i];
            c.inB[(size_t) pos] = b[ch][i];
            const float stft = c.acc[(size_t) pos];
            c.acc[(size_t) pos] = 0.0f;
            const float dry = c.bypass.process (a[ch][i]);
            o[ch][i] = dry + (stft - dry) * activeGain;
        }
        pos = (pos + 1) & (fftSize - 1);

        // frames run while active or fading out; the fade-in waits until the overlap-add is complete
        const bool running = active || activeGain > 0.0f;
        if (running)
        {
            if (active && warmup < fftSize) ++warmup;
            if (++hopCounter >= hop)
            {
                hopCounter = 0;
                processFrame (0, p);
                processFrame (1, p);
            }
        }
        else
        {
            warmup = 0;
        }

        const float target = (active && warmup >= fftSize) ? 1.0f : 0.0f;
        if (activeGain < target) activeGain = std::min (target, activeGain + step);
        else if (activeGain > target) activeGain = std::max (target, activeGain - step);
    }
}
}
