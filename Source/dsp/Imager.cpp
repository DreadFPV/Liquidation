#include "Imager.h"
#include <cmath>
#include <algorithm>

namespace liq
{
void Imager::Lr4::design (float fc, double sr)
{
    const double w = 2.0 * 3.14159265358979 * fc / sr;
    const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * 0.70710678);
    const double a0 = 1.0 + alpha;
    b0 = (float) ((1.0 - cw) * 0.5 / a0);
    b1 = (float) ((1.0 - cw) / a0);
    b2 = b0;
    hb0 = (float) ((1.0 + cw) * 0.5 / a0);
    hb1 = (float) (-(1.0 + cw) / a0);
    hb2 = hb0;
    a1 = (float) (-2.0 * cw / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

float Imager::Lr4::low (float x, int s) noexcept
{
    const float y = b0 * x + lpz[s][0];
    lpz[s][0] = b1 * x - a1 * y + lpz[s][1];
    lpz[s][1] = b2 * x - a2 * y;
    return y;
}

float Imager::Lr4::high (float x, int s) noexcept
{
    const float y = hb0 * x + hpz[s][0];
    hpz[s][0] = hb1 * x - a1 * y + hpz[s][1];
    hpz[s][1] = hb2 * x - a2 * y;
    return y;
}

void Imager::prepare (double sampleRate)
{
    sr = sampleRate;
    dimBuf.assign ((size_t) (sr * 0.1) + 8, 0.0f);
    reset();
}

void Imager::reset()
{
    std::fill (dimBuf.begin(), dimBuf.end(), 0.0f);
    dimPos = 0;
    lfo = 0.0;
    xoverL = {}; xoverR = {};
    lastMonoFreq = -1.0f;
    sideLp = 0.0f;
}

void Imager::process (float* l, float* r, int n, const ImagerParams& p)
{
    if (! p.on) return;

    const int len = (int) dimBuf.size();

    // ---- Dimension: two modulated delays of the mid signal, added as side content
    if (p.dimension > 0.001f)
    {
        const float base = std::clamp (p.grainSizeMs * 0.15f, 3.0f, 30.0f) * (0.4f + p.dimSize * 1.6f);
        const float lfoRate = std::clamp (p.grainRateHz * 0.05f, 0.1f, 2.0f);
        const double lfoInc = lfoRate / sr;
        for (int i = 0; i < n; ++i)
        {
            smoothedDelay += 0.001f * (base - smoothedDelay);
            const float mid = 0.5f * (l[i] + r[i]);
            dimBuf[(size_t) dimPos] = mid;

            lfo += lfoInc; if (lfo >= 1.0) lfo -= 1.0;
            const float m = (float) std::sin (lfo * 6.283185307);
            auto tap = [&] (float ms)
            {
                float rp = (float) dimPos - ms * 0.001f * (float) sr;
                while (rp < 0) rp += (float) len;
                const int i0 = (int) rp; const float fr = rp - (float) i0;
                const float a = dimBuf[(size_t) (i0 % len)], b = dimBuf[(size_t) ((i0 + 1) % len)];
                return a + (b - a) * fr;
            };
            const float d1 = tap (smoothedDelay * (1.0f + 0.12f * m));
            const float d2 = tap (smoothedDelay * 1.37f * (1.0f - 0.12f * m));
            const float side = (d1 - d2) * p.dimension * 0.9f;
            l[i] += side;
            r[i] -= side;
            if (++dimPos >= len) dimPos = 0;
        }
    }

    // ---- Mono lows
    if (p.monoLows)
    {
        if (std::abs (p.monoFreq - lastMonoFreq) > 0.5f)
        {
            xoverL.design (p.monoFreq, sr);
            xoverR.design (p.monoFreq, sr);
            lastMonoFreq = p.monoFreq;
        }
        for (int i = 0; i < n; ++i)
        {
            const float ll = xoverL.low (xoverL.low (l[i], 0), 1);
            const float lh = xoverL.high (xoverL.high (l[i], 0), 1);
            const float rl = xoverR.low (xoverR.low (r[i], 0), 1);
            const float rh = xoverR.high (xoverR.high (r[i], 0), 1);
            const float mono = 0.5f * (ll + rl);
            l[i] = mono + lh;
            r[i] = mono + rh;
        }
    }

    // ---- Stereo width
    const float w = std::clamp (p.width, 0.0f, 2.0f);
    if (std::abs (w - 1.0f) > 0.001f)
    {
        const float aSide = 1.0f - std::exp (-6.2831853f * 250.0f / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (l[i] + r[i]);
            float side = 0.5f * (l[i] - r[i]);
            if (w <= 1.0f || ! p.safeWidth)
            {
                side *= w;
            }
            else
            {
                // safe width: only widen above ~250 Hz and keep side below 1.4x mid energy
                sideLp += aSide * (side - sideLp);
                const float hf = side - sideLp;
                float extra = hf * (w - 1.0f);
                const float limit = std::abs (mid) * 1.4f + 0.02f;
                extra = std::clamp (extra, -limit, limit);
                side += extra;
            }
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }
}
}
