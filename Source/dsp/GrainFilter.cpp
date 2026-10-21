#include "GrainFilter.h"
#include "Scale.h"
#include "../Params.h"
#include <cmath>
#include <algorithm>

namespace liq
{
namespace
{
    constexpr float pi = 3.14159265358979f;

    inline float rand01 (uint32_t& s) noexcept
    {
        s = s * 1664525u + 1013904223u;
        return (float) (s >> 8) * (1.0f / 16777216.0f);
    }

    inline float fastTanh (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    const float vowelF[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                               { 570, 840, 2410 },  { 300, 870, 2240 } };
}

void GrainFilter::start (const GrainFilterParams& params, const Scale& sc, double sampleRate,
                         float* memL, float* memR, uint32_t& rng) noexcept
{
    p = params;
    scale = &sc;
    sr = sampleRate;
    dl[0] = memL; dl[1] = memR;
    dlPos = 0;
    counter = 0;

    baseCutoff = std::clamp (p.cutoff, 20.0f, 20000.0f);
    if (p.scaleLock) baseCutoff = sc.quantiseHz (baseCutoff);

    randomOffset = p.random ? (rand01 (rng) * 2.0f - 1.0f) * p.modDepth * 4.0f : 0.0f;

    for (auto& c : ic1) std::fill (std::begin (c), std::end (c), 0.0f);
    for (auto& c : ic2) std::fill (std::begin (c), std::end (c), 0.0f);
    for (auto& c : apState) std::fill (std::begin (c), std::end (c), 0.0f);
    fbSample[0] = fbSample[1] = 0;
    dampState[0] = dampState[1] = 0;
    holdValue[0] = holdValue[1] = 0;
    smoothState[0] = smoothState[1] = 0;
    holdCounter = 1.0f;

    if ((p.mode == fComb || p.mode == fFlange) && dl[0] != nullptr)
    {
        std::fill (dl[0], dl[0] + maxDelay, 0.0f);
        std::fill (dl[1], dl[1] + maxDelay, 0.0f);
    }

    updateCoefficients (0.0f);
}

void GrainFilter::updateCoefficients (float phase) noexcept
{
    // ---- cutoff with per-grain modulation
    float octaves = randomOffset;
    if (! p.random && p.modDepth != 0.0f)
        octaves = p.modDepth * 4.0f * (0.5f - 0.5f * std::cos (2.0f * pi * p.modRate * phase));

    float fc = baseCutoff * std::exp2 (octaves);
    if (p.random && p.scaleLock) fc = scale->quantiseHz (fc);
    const float nyq = (float) sr * 0.45f;
    fc = std::clamp (fc, 20.0f, nyq);
    currentCutoff = fc;

    auto setSvf = [this, nyq] (Svf& f, float freq, float q)
    {
        freq = std::clamp (freq, 20.0f, nyq);
        f.g = std::tan (pi * freq / (float) sr);
        f.k = 1.0f / std::max (q, 0.3f);
        f.a1 = 1.0f / (1.0f + f.g * (f.g + f.k));
        f.a2 = f.g * f.a1;
        f.a3 = f.g * f.a2;
    };

    const float q = 0.5f + p.reso * p.reso * 18.0f;

    switch (p.mode)
    {
        case fVowel:
        {
            const float pos = std::clamp (p.morph, 0.0f, 1.0f) * 4.0f;
            const int i0 = std::min (3, (int) pos);
            const float t = pos - (float) i0;
            const float shift = std::clamp (fc / 1000.0f, 0.4f, 2.5f);
            for (int b = 0; b < 3; ++b)
            {
                const float f = (vowelF[i0][b] * (1 - t) + vowelF[i0 + 1][b] * t) * shift;
                setSvf (svf[b], f, 4.0f + p.reso * 16.0f);
            }
            bandGain[0] = 1.0f; bandGain[1] = 0.6f; bandGain[2] = 0.35f;
            break;
        }
        case fModal:
        {
            const float m = std::clamp (p.morph, 0.0f, 1.0f);
            const float harm[4] { 1, 2, 3, 4 }, odd[4] { 1, 3, 5, 7 };
            for (int b = 0; b < 4; ++b)
            {
                const float inharm = 1.0f + std::sin (pi * m) * 0.13f * (float) b;
                bandFreqMul[b] = (harm[b] * (1 - m) + odd[b] * m) * inharm;
                setSvf (svf[b], fc * bandFreqMul[b], 8.0f + p.reso * p.reso * 300.0f);
                bandGain[b] = 1.0f / (1.0f + 0.5f * (float) b);
            }
            break;
        }
        case fPhaser:
        case fAllpass:
        {
            const int stages = 8;
            const float spreadOct = p.mode == fPhaser ? 0.3f + p.morph * 3.0f : p.morph * 5.0f;
            for (int s = 0; s < stages; ++s)
            {
                const float rel = stages > 1 ? (float) s / (float) (stages - 1) - 0.5f : 0.0f;
                const float f = std::clamp (fc * std::exp2 (rel * spreadOct), 20.0f, nyq);
                const float t = std::tan (pi * f / (float) sr);
                apCoef[s] = (t - 1.0f) / (t + 1.0f);
            }
            break;
        }
        case fComb:
        case fFlange:
        {
            float period = (float) sr / fc;
            if (p.mode == fFlange)
                period *= 0.5f + 0.45f * (0.5f + 0.5f * std::sin (2.0f * pi * (0.5f + p.morph * 3.0f) * phase));
            dlTime = std::clamp (period, 2.0f, (float) maxDelay - 4.0f);
            break;
        }
        case fReducer:
            break;
        case fMorph:
        default:
            setSvf (svf[0], fc, q);
            break;
    }
}

void GrainFilter::process (float& l, float& r, float phase) noexcept
{
    if ((counter++ & 15) == 0 && counter > 1)
        updateCoefficients (phase);

    float in[2] { l, r };
    float out[2] { l, r };

    const float driveGain = 1.0f + p.drive * p.drive * 12.0f;
    const bool driven = p.drive > 0.001f;

    for (int ch = 0; ch < 2; ++ch)
    {
        float x = in[ch];
        if (driven && p.mode <= fNotch) x = fastTanh (x * driveGain) / std::sqrt (driveGain);

        float y = x;
        switch (p.mode)
        {
            case fLowpass: case fHighpass: case fBandpass: case fNotch: case fMorph:
            {
                auto& f = svf[0];
                float& s1 = ic1[ch][0]; float& s2 = ic2[ch][0];
                const float v3 = x - s2;
                const float v1 = f.a1 * s1 + f.a2 * v3;
                const float v2 = s2 + f.a2 * s1 + f.a3 * v3;
                s1 = 2.0f * v1 - s1;
                s2 = 2.0f * v2 - s2;
                const float lp = v2, bp = v1, hp = x - f.k * v1 - v2, nt = lp + hp;
                if      (p.mode == fLowpass)  y = lp;
                else if (p.mode == fHighpass) y = hp;
                else if (p.mode == fBandpass) y = bp * f.k * 1.5f;
                else if (p.mode == fNotch)    y = nt;
                else
                {
                    const float m = std::clamp (p.morph, 0.0f, 1.0f) * 3.0f;
                    const float bpN = bp * f.k * 1.5f;
                    if (m < 1.0f)      y = lp + (bpN - lp) * m;
                    else if (m < 2.0f) y = bpN + (hp - bpN) * (m - 1.0f);
                    else               y = hp + (nt - hp) * (m - 2.0f);
                    if (driven) y = fastTanh (y * driveGain) / std::sqrt (driveGain);
                }
                break;
            }
            case fVowel:
            case fModal:
            {
                const int bands = p.mode == fVowel ? 3 : 4;
                float sum = 0.0f;
                for (int b = 0; b < bands; ++b)
                {
                    auto& f = svf[b];
                    float& s1 = ic1[ch][b]; float& s2 = ic2[ch][b];
                    const float v3 = x - s2;
                    const float v1 = f.a1 * s1 + f.a2 * v3;
                    const float v2 = s2 + f.a2 * s1 + f.a3 * v3;
                    s1 = 2.0f * v1 - s1;
                    s2 = 2.0f * v2 - s2;
                    sum += v1 * f.k * bandGain[b];
                }
                y = p.mode == fVowel ? sum * 4.0f : sum * 5.0f;   // make-up for the narrow resonant bands
                if (driven) y = fastTanh (y * driveGain) / std::sqrt (driveGain);
                break;
            }
            case fPhaser:
            case fAllpass:
            {
                float v = x + fbSample[ch] * p.reso * 0.85f;
                for (int s = 0; s < 8; ++s)
                {
                    const float a = apCoef[s];
                    const float o = a * v + apState[ch][s];
                    apState[ch][s] = v - a * o;
                    v = o;
                }
                fbSample[ch] = v;
                y = p.mode == fPhaser ? 0.5f * (x + v) : v;
                if (driven) y = fastTanh (y * driveGain) / std::sqrt (driveGain);
                break;
            }
            case fComb:
            case fFlange:
            {
                if (dl[ch] == nullptr) break;
                float rp = (float) dlPos - dlTime;
                while (rp < 0) rp += (float) maxDelay;
                const int i0 = (int) rp;
                const float fr = rp - (float) i0;
                const float d0 = dl[ch][i0 % maxDelay], d1 = dl[ch][(i0 + 1) % maxDelay];
                float delayed = d0 + (d1 - d0) * fr;
                // damping in the loop
                const float dampCoef = p.damp * 0.95f;
                dampState[ch] = delayed * (1.0f - dampCoef) + dampState[ch] * dampCoef;
                delayed = dampState[ch];
                const float fb = (p.mode == fComb ? 0.5f + p.reso * 0.49f : p.reso * 0.9f);
                float w = x + delayed * fb;
                if (driven) w = fastTanh (w * driveGain) / std::sqrt (driveGain);
                dl[ch][dlPos] = w;
                y = p.mode == fComb ? delayed * (1.0f - fb * 0.6f) + x * 0.3f : 0.5f * (x + delayed);
                break;
            }
            case fReducer:
            {
                y = holdValue[ch];
                break;
            }
            default: break;
        }
        out[ch] = y;
    }

    if (p.mode == fComb || p.mode == fFlange)
        if (++dlPos >= maxDelay) dlPos = 0;

    if (p.mode == fReducer)
    {
        // sample-rate reduction: cutoff sets the target rate
        holdCounter += std::clamp (currentCutoff * 2.0f / (float) sr, 0.0005f, 1.0f);
        if (holdCounter >= 1.0f)
        {
            holdCounter -= 1.0f;
            const float levels = std::exp2 (std::clamp (p.bits, 1.0f, 16.0f)) * 0.5f;
            for (int ch = 0; ch < 2; ++ch)
            {
                float v = in[ch];
                if (p.bits < 15.9f) v = std::round (v * levels) / levels;
                holdValue[ch] = v;
            }
        }
        const float sc = p.smooth * 0.97f;
        for (int ch = 0; ch < 2; ++ch)
        {
            smoothState[ch] = holdValue[ch] * (1.0f - sc) + smoothState[ch] * sc;
            out[ch] = smoothState[ch];
        }
    }

    l = in[0] + (out[0] - in[0]) * p.mix;
    r = in[1] + (out[1] - in[1]) * p.mix;
}
}
