#include "Animator.h"
#include <cmath>
#include <algorithm>

namespace liq
{
void Animator::shapePoint (int mode, int cycles, float warp, double t01, float& x, float& y) noexcept
{
    const double t = t01 * 6.283185307179586;
    const int c = std::clamp (cycles, 1, 8);
    if (mode == 0)
    {
        // hypotrochoid: circle at warp 0, petals / rosettes as warp rises
        const double R = 1.0, r = 1.0 / (c + 1.0);
        const double d = (double) warp * r * 1.6;
        const double k = (R - r) / r;
        const double xx = (R - r) * std::cos (t) + d * std::cos (k * t);
        const double yy = (R - r) * std::sin (t) - d * std::sin (k * t);
        const double norm = (R - r) + d;
        x = (float) (xx / norm);
        y = (float) (yy / norm);
    }
    else
    {
        // lissajous weave: warp sets the phase offset between the axes
        x = (float) std::sin (c * t);
        y = (float) std::sin ((c + 1) * t + (double) warp * 3.14159265358979);
    }
}

void Animator::advance (const AnimatorParams& p, int n, double ppq, double bpm, bool playing) noexcept
{
    if (p.sync && playing)
    {
        const double cyc = ppq / std::max (1.0e-6, p.divBeats);
        t = cyc - std::floor (cyc);
    }
    else
    {
        const double hz = p.sync ? bpm / 60.0 / std::max (1.0e-6, p.divBeats) : (double) p.rateHz;
        t += hz * n / sr;
        t -= std::floor (t);
    }

    float sx, sy;
    shapePoint (p.mode, p.cycles, p.warp, t, sx, sy);

    // smooth random drift around the path
    if (--driftCounter <= 0)
    {
        driftCounter = (int) (sr * 0.25 / std::max (1, n));
        driftTX = rnd(); driftTY = rnd();
    }
    driftX += 0.02f * (driftTX - driftX);
    driftY += 0.02f * (driftTY - driftY);

    const float amp = p.depth * 0.5f;
    const float dx = (sx + driftX * p.drift * 0.6f) * amp;
    const float dy = (sy + driftY * p.drift * 0.6f) * amp;

    m1 = std::clamp (p.x + dx, 0.0f, 1.0f);
    m2 = std::clamp (p.y + dy, 0.0f, 1.0f);
    cursorX.store (m1, std::memory_order_relaxed);
    cursorY.store (m2, std::memory_order_relaxed);
}
}
