#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace liq
{
// Compact 8-line feedback-delay-network reverb with input diffusion.
// process() returns the wet signal only.
class FdnReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double scale = sr / 48000.0;
        for (int i = 0; i < N; ++i)
        {
            maxLen[(size_t) i] = (int) (baseLen[i] * scale * 2.2) + 8;
            lines[(size_t) i].assign ((size_t) maxLen[(size_t) i], 0.0f);
        }
        for (int i = 0; i < 4; ++i)
        {
            apLen[(size_t) i] = std::max (1, (int) (diffLen[i] * scale));
            for (int c = 0; c < 2; ++c)
                ap[(size_t) c][(size_t) i].assign ((size_t) apLen[(size_t) i] + 1, 0.0f);
        }
        preDelay.assign ((size_t) (sr * 0.25) + 4, 0.0f);
        preDelayR.assign (preDelay.size(), 0.0f);
        reset();
        setParams (0.5f, 0.5f, 1.0f, 0.0f);
    }

    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& c : ap) for (auto& a : c) std::fill (a.begin(), a.end(), 0.0f);
        std::fill (preDelay.begin(), preDelay.end(), 0.0f);
        std::fill (preDelayR.begin(), preDelayR.end(), 0.0f);
        pos.fill (0); apPos.fill (0); damp.fill (0.0f);
        prePos = 0; lfoPhase = 0.0;
    }

    // size 0..1, damping 0..1, width 0..1, preDelayMs 0..250
    void setParams (float size, float damping, float width, float preDelayMs)
    {
        const float sz = std::clamp (size, 0.0f, 1.0f);
        const float lenScale = 0.35f + sz * 1.6f;
        for (int i = 0; i < N; ++i)
            len[(size_t) i] = std::clamp ((int) (baseLen[i] * (sr / 48000.0) * lenScale), 16, maxLen[(size_t) i] - 4);
        // RT60 between ~0.4 s and ~14 s
        const float rt60 = 0.4f * std::pow (35.0f, sz);
        for (int i = 0; i < N; ++i)
            gain[(size_t) i] = std::pow (10.0f, -3.0f * (float) len[(size_t) i] / (float) (rt60 * sr));
        dampCoef = std::clamp (damping, 0.0f, 1.0f) * 0.85f;
        wid = std::clamp (width, 0.0f, 1.0f);
        preSamples = std::clamp ((int) (preDelayMs * 0.001f * (float) sr), 0, (int) preDelay.size() - 2);
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const double lfoInc = 0.37 / sr;
        for (int s = 0; s < n; ++s)
        {
            // pre-delay
            preDelay[(size_t) prePos] = inL[s];
            preDelayR[(size_t) prePos] = inR[s];
            int rp = prePos - preSamples; if (rp < 0) rp += (int) preDelay.size();
            float xl = preDelay[(size_t) rp], xr = preDelayR[(size_t) rp];
            if (++prePos >= (int) preDelay.size()) prePos = 0;

            // input diffusion
            for (int i = 0; i < 4; ++i)
            {
                xl = allpass (0, i, xl);
                xr = allpass (1, i, xr);
            }

            // read lines
            std::array<float, N> o {};
            lfoPhase += lfoInc; if (lfoPhase > 1.0) lfoPhase -= 1.0;
            const float mod = (float) std::sin (lfoPhase * 6.283185307) * 6.0f;
            for (int i = 0; i < N; ++i)
            {
                float rpos = (float) pos[(size_t) i] - (float) len[(size_t) i] + (i < 2 ? mod * (i == 0 ? 1.0f : -1.0f) : 0.0f);
                const int L = maxLen[(size_t) i];
                while (rpos < 0) rpos += (float) L;
                const int i0 = (int) rpos; const float fr = rpos - (float) i0;
                const auto& line = lines[(size_t) i];
                const float a = line[(size_t) (i0 % L)], b = line[(size_t) ((i0 + 1) % L)];
                float v = a + (b - a) * fr;
                damp[(size_t) i] = v * (1.0f - dampCoef) + damp[(size_t) i] * dampCoef;
                o[(size_t) i] = damp[(size_t) i] * gain[(size_t) i];
            }

            // Householder feedback matrix
            float sum = 0.0f;
            for (float v : o) sum += v;
            sum *= 2.0f / (float) N;

            for (int i = 0; i < N; ++i)
            {
                const float inj = (i & 1) ? xr : xl;
                lines[(size_t) i][(size_t) pos[(size_t) i]] = o[(size_t) i] - sum + inj * 0.35f;
                if (++pos[(size_t) i] >= maxLen[(size_t) i]) pos[(size_t) i] = 0;
            }

            const float wl = o[0] + o[2] + o[4] + o[6];
            const float wr = o[1] + o[3] + o[5] + o[7];
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * wid;
            outL[s] = (mid + side) * 0.6f;
            outR[s] = (mid - side) * 0.6f;
        }
    }

private:
    static constexpr int N = 8;
    static constexpr int baseLen[N] { 1433, 1601, 1867, 2053, 2251, 2399, 2687, 2903 };
    static constexpr int diffLen[4] { 142, 107, 379, 277 };

    float allpass (int ch, int i, float x)
    {
        auto& buf = ap[(size_t) ch][(size_t) i];
        int& p = apPos[(size_t) (ch * 4 + i)];
        const float d = buf[(size_t) p];
        const float g = 0.6f;
        const float y = -g * x + d;
        buf[(size_t) p] = x + g * y;
        if (++p >= apLen[(size_t) i]) p = 0;
        return y;
    }

    double sr = 48000.0;
    std::array<std::vector<float>, N> lines;
    std::array<int, N> maxLen {}, len {}, pos {};
    std::array<float, N> gain {}, damp {};
    std::array<std::array<std::vector<float>, 4>, 2> ap;
    std::array<int, 4> apLen {};
    std::array<int, 8> apPos {};
    std::vector<float> preDelay, preDelayR;
    int prePos = 0, preSamples = 0;
    float dampCoef = 0.3f, wid = 1.0f;
    double lfoPhase = 0.0;
};
}
