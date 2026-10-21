#include "Dynamics.h"
#include <cmath>
#include <algorithm>

namespace liq
{
// ============================================================================
void TransientKeeper::prepare (double sampleRate, int pathLatency)
{
    sr = sampleRate;
    const int ahead = (int) (0.002 * sr);   // open 2 ms before the attack reaches the aligned signal
    gateDelay.prepare (std::max (0, pathLatency - ahead));
    reset();
}

void TransientKeeper::reset()
{
    gateDelay.reset();
    envFast = envSlow = gate = 0.0f;
    hold = refractory = 0;
}

void TransientKeeper::process (const float* liveL, const float* liveR, const float* dryL, const float* dryR,
                               float* l, float* r, int n, const TransientParams& p) noexcept
{
    const float fa = 1.0f - std::exp (-1.0f / (0.0004f * (float) sr));
    const float fr = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));
    const float sa = 1.0f - std::exp (-1.0f / (0.040f * (float) sr));
    const float srl = 1.0f - std::exp (-1.0f / (0.200f * (float) sr));
    const float ratio = 1.25f + (1.0f - p.sensitivity) * 3.0f;
    const float floorLvl = std::pow (10.0f, (-60.0f + (1.0f - p.sensitivity) * 36.0f) / 20.0f);
    const int holdLen = (int) (std::clamp (p.lengthMs, 2.0f, 500.0f) * 0.001f * (float) sr);
    const float att = 1.0f - std::exp (-1.0f / (0.0005f * (float) sr));
    const float rel = 1.0f - std::exp (-1.0f / (0.025f * (float) sr));
    const float amount = p.on ? std::clamp (p.amount, 0.0f, 1.0f) : 0.0f;

    float shown = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float x = std::max (std::abs (liveL[i]), std::abs (liveR[i]));
        envFast += (x > envFast ? fa : fr) * (x - envFast);
        envSlow += (x > envSlow ? sa : srl) * (x - envSlow);
        if (refractory > 0) --refractory;
        if (refractory == 0 && envFast > envSlow * ratio && envFast > floorLvl)
        {
            hold = holdLen;
            refractory = (int) (0.03 * sr);
        }
        const float target = hold > 0 ? 1.0f : 0.0f;
        if (hold > 0) --hold;
        gate += (target > gate ? att : rel) * (target - gate);

        const float g = gateDelay.process (gate) * amount;
        l[i] = l[i] + (dryL[i] - l[i]) * g;
        r[i] = r[i] + (dryR[i] - r[i]) * g;
        shown = std::max (shown, g);
    }
    gateDisplay.store (shown, std::memory_order_relaxed);
}

// ============================================================================
void Limiter::prepare (double sampleRate)
{
    sr = sampleRate;
    dl.prepare (lookahead);
    dr.prepare (lookahead);
    targets.assign ((size_t) lookahead + 1, 1.0f);
    reset();
}

void Limiter::reset()
{
    dl.reset(); dr.reset();
    std::fill (targets.begin(), targets.end(), 1.0f);
    tpos = 0; gain = 1.0f; displayRed = 0.0f;
}

void Limiter::process (float* l, float* r, int n, bool on, float ceilingDb) noexcept
{
    const float ceiling = std::pow (10.0f, ceilingDb / 20.0f);
    const float attack = 1.0f - std::exp (-1.0f / 14.0f);
    const float release = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
    float maxRed = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
        targets[(size_t) tpos] = peak > ceiling ? ceiling / peak : 1.0f;
        if (++tpos > lookahead) tpos = 0;

        float env = 1.0f;
        for (float t : targets) env = std::min (env, t);

        gain += (env < gain ? attack : release) * (env - gain);

        float yl = dl.process (l[i]), yr = dr.process (r[i]);
        if (on)
        {
            yl = std::clamp (yl * gain, -ceiling, ceiling);
            yr = std::clamp (yr * gain, -ceiling, ceiling);
            maxRed = std::max (maxRed, -20.0f * std::log10 (std::max (gain, 1.0e-6f)));
        }
        l[i] = yl; r[i] = yr;
    }
    displayRed = std::max (maxRed, displayRed * 0.9f);
    reductionDb.store (displayRed, std::memory_order_relaxed);
}
}
