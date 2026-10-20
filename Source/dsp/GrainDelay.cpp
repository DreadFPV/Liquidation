#include "GrainDelay.h"
#include <cmath>
#include <algorithm>

namespace liq
{
void GrainDelay::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    const size_t len = (size_t) (sr * 34.0) + 8;   // up to 8 taps x 4 bars at slow tempi
    fbL.assign (len, 0.0f);
    fbR.assign (len, 0.0f);
    busL.assign ((size_t) maxBlock, 0.0f); busR.assign ((size_t) maxBlock, 0.0f);
    revL.assign ((size_t) maxBlock, 0.0f); revR.assign ((size_t) maxBlock, 0.0f);
    space.prepare (sr);
    space.setParams (0.75f, 0.45f, 1.0f, 0.0f);
    reset();
}

void GrainDelay::reset()
{
    std::fill (fbL.begin(), fbL.end(), 0.0f);
    std::fill (fbR.begin(), fbR.end(), 0.0f);
    fbPos = 0;
    lpL = lpR = hpL = hpR = lpOutL = lpOutR = hpOutL = hpOutR = {};
    space.reset();
}

void GrainDelay::process (const float* mainL, const float* mainR, const float* tapL, const float* tapR,
                          float* outL, float* outR, int n, const GrainDelayParams& p)
{
    if (! p.on)
    {
        for (int i = 0; i < n; ++i) { outL[i] = mainL[i]; outR[i] = mainR[i]; }
        return;
    }

    const int len = (int) fbL.size();
    const float aLp = 1.0f - std::exp (-6.2831853f * std::min (p.lowpass, (float) sr * 0.45f) / (float) sr);
    const float aHp = 1.0f - std::exp (-6.2831853f * std::max (p.highpass, 10.0f) / (float) sr);
    const float target = (float) std::clamp (p.loopSamples, 16.0, (double) len - 4);

    for (int i = 0; i < n; ++i)
    {
        smoothedLoop += 0.0005f * (target - smoothedLoop);

        float rp = (float) fbPos - smoothedLoop;
        while (rp < 0) rp += (float) len;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) % len;
        const float dl = fbL[(size_t) i0] + (fbL[(size_t) i1] - fbL[(size_t) i0]) * fr;
        const float dr = fbR[(size_t) i0] + (fbR[(size_t) i1] - fbR[(size_t) i0]) * fr;

        // filters inside the loop (recursive)
        float fl = lpL.lp (dl, aLp), frr = lpR.lp (dr, aLp);
        fl -= hpL.lp (fl, aHp); frr -= hpR.lp (frr, aHp);

        const float fbGain = p.feedback;
        float wl, wr;
        if (p.pingPong) { wl = tapL[i] + frr * fbGain; wr = tapR[i] + fl * fbGain; }
        else            { wl = tapL[i] + fl * fbGain;  wr = tapR[i] + frr * fbGain; }

        // soft limit the loop
        fbL[(size_t) fbPos] = std::tanh (wl);
        fbR[(size_t) fbPos] = std::tanh (wr);
        if (++fbPos >= len) fbPos = 0;

        // delay bus = taps + echoes (filtered once on the way out)
        float bl = tapL[i] + fl * fbGain, br = tapR[i] + frr * fbGain;
        bl = lpOutL.lp (bl, aLp); br = lpOutR.lp (br, aLp);
        bl -= hpOutL.lp (bl, aHp); br -= hpOutR.lp (br, aHp);
        busL[(size_t) i] = bl; busR[(size_t) i] = br;
    }

    if (p.space > 0.001f)
    {
        space.process (busL.data(), busR.data(), revL.data(), revR.data(), n);
        const float s = p.space;
        const float dry = std::cos (s * 1.5707963f), wet = std::sin (s * 1.5707963f) * 1.4f;
        for (int i = 0; i < n; ++i)
        {
            busL[(size_t) i] = busL[(size_t) i] * dry + revL[(size_t) i] * wet;
            busR[(size_t) i] = busR[(size_t) i] * dry + revR[(size_t) i] * wet;
        }
    }

    const float mainGain = std::min (1.0f, 2.0f * (1.0f - p.mix));
    const float busGain  = std::min (1.0f, 2.0f * p.mix);
    for (int i = 0; i < n; ++i)
    {
        outL[i] = mainL[i] * mainGain + busL[(size_t) i] * busGain;
        outR[i] = mainR[i] * mainGain + busR[(size_t) i] * busGain;
    }
}
}
