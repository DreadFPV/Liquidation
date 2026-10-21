#include "Melody.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace liq
{
namespace
{
    struct Rng
    {
        uint32_t s;
        explicit Rng (uint32_t seed) : s (seed * 2654435761u + 0x9e3779b9u) { for (int i = 0; i < 4; ++i) next(); }
        uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
        int range (int lo, int hi) noexcept { return lo + (int) (uni() * (float) (hi - lo + 1)) % (hi - lo + 1); }
        bool chance (float p) noexcept { return uni() < p; }
        int pick (std::initializer_list<int> a) noexcept { return *(a.begin() + range (0, (int) a.size() - 1)); }
    };

    MelodyStep note (int deg, float vel = 0.8f, int len = 1, int oct = 0)
    {
        MelodyStep s;
        s.on = true; s.degree = deg; s.velocity = vel; s.length = len; s.octave = oct;
        return s;
    }

    // keep a degree inside the allowed range (in octaves), folding it back
    void fold (MelodyStep& st, int range)
    {
        const int maxOct = std::max (0, range - 1);
        while (st.degree > 7) { st.degree -= 7; ++st.octave; }
        while (st.degree < -3) { st.degree += 7; --st.octave; }
        st.octave = std::clamp (st.octave, -1, maxOct);
    }

    // chord progressions as scale degrees (one chord per bar)
    const int progMinor[][4] { { 0, 5, 3, 4 }, { 0, 5, 2, 6 }, { 0, 3, 5, 4 }, { 0, 0, 5, 6 }, { 0, 6, 5, 6 }, { 0, 2, 5, 4 } };
}

int stepNote (const MelodyStep& st, const Scale& scale, int root)
{
    return scale.shiftDegrees (root, st.degree) + 12 * st.octave;
}

MelodyPhrase generatePhrase (const MelodySettings& set)
{
    MelodyPhrase ph;
    const int len = std::clamp (set.steps, 16, MelodyPhrase::maxSteps);
    ph.length = len;
    const int bars = len / 16;
    const float dens = std::clamp (set.density, 0.0f, 1.0f);
    const int range = std::clamp (set.range, 1, 3);
    Rng rng ((uint32_t) (set.style * 1000 + std::clamp (set.variation, 1, 99)));
    const auto& prog = progMinor[(size_t) rng.range (0, 5)];
    auto& st = ph.steps;

    auto rest = [&] (float extra = 0.0f) { return rng.chance ((1.0f - dens) * 0.55f + extra); };

    switch (set.style)
    {
        case mlGoaLead:
        {
            // one bar motif: stepwise lines with leaps, call/response across bars
            MelodyStep motif[16];
            int d = rng.pick ({ 0, 4, 2 });
            for (int i = 0; i < 16; ++i)
            {
                const int stepMove = rng.pick ({ 1, 1, -1, -1, 2, -2, 3, 0 });
                d = std::clamp (d + stepMove, -2, 7 * range - 1);
                motif[i] = note (d, (i % 4 == 0) ? 1.0f : 0.75f);
                if (i % 4 != 0 && rest()) motif[i].on = false;
                if (rng.chance (0.12f)) motif[i].slide = true;
            }
            motif[0].on = true;
            for (int b = 0; b < bars; ++b)
                for (int i = 0; i < 16; ++i)
                {
                    auto s = motif[i];
                    if (b % 2 == 1) s.degree += (b % 4 == 3 ? -2 : 2);          // answer a third away
                    if (b % 4 == 3 && i >= 12) { s = note (i == 15 ? 0 : 4 - (i - 12), 0.9f); }   // cadence
                    fold (s, range);
                    st[(size_t) (b * 16 + i)] = s;
                }
            break;
        }
        case mlRollingArp:
        {
            const int shape = rng.range (0, 3);
            static const int up[4] { 0, 2, 4, 7 }, down[4] { 7, 4, 2, 0 }, updn[4] { 0, 4, 2, 7 }, wide[4] { 0, 4, 7, 9 };
            const int* pat = shape == 0 ? up : (shape == 1 ? down : (shape == 2 ? updn : wide));
            for (int b = 0; b < bars; ++b)
            {
                const int chordRoot = prog[b % 4];
                for (int i = 0; i < 16; ++i)
                {
                    auto s = note (chordRoot + pat[i % 4], (i % 4 == 0) ? 1.0f : 0.7f);
                    if (i >= 8 && rng.chance (0.3f)) s.octave += 1;
                    if (i % 4 == 3 && rest (-0.2f)) s.on = false;
                    fold (s, range);
                    st[(size_t) (b * 16 + i)] = s;
                }
            }
            break;
        }
        case mlAcid:
        {
            MelodyStep motif[32];

            for (int i = 0; i < 32; ++i)
            {
                auto s = note (rng.pick ({ 0, 0, 0, 0, 2, 3, 4, 7, -1 }), rng.chance (0.3f) ? 1.0f : 0.6f);
                if (rng.chance (0.18f)) s.octave = 1;
                s.slide = rng.chance (0.22f);
                if (i % 4 != 0 && rest (-0.1f)) s.on = false;
                fold (s, range);
                motif[i] = s;
            }
            motif[0] = note (0, 1.0f);
            for (int i = 0; i < len; ++i) st[(size_t) i] = motif[i % 32];
            break;
        }
        case mlStabs:
        {
            for (int b = 0; b < bars; ++b)
                for (int i = 0; i < 16; ++i)
                {
                    MelodyStep s;
                    if (i % 4 == 2) { s = note (prog[b % 4], 0.9f); s.chord = true; }
                    else if (i % 8 == 7 && dens > 0.5f && rng.chance (dens - 0.3f)) { s = note (prog[b % 4], 0.6f); s.chord = true; }
                    fold (s, range);
                    st[(size_t) (b * 16 + i)] = s;
                }
            break;
        }
        case mlDarkPulse:
        {
            for (int i = 0; i < len; ++i)
            {
                auto s = note (0, (i % 4 == 0) ? 1.0f : (i % 2 == 0 ? 0.75f : 0.55f));
                const float r = rng.uni();
                if (r < 0.10f) s.degree = 1;                 // the flat second in phrygian
                else if (r < 0.16f) s.octave = 1;
                else if (r < 0.20f) s.degree = -1;
                s.slide = rng.chance (0.08f);
                if (i % 4 != 0 && rest (-0.25f)) s.on = false;
                fold (s, range);
                st[(size_t) i] = s;
            }
            break;
        }
        case mlHook:
        {
            // 2-bar call + answer on an 8th grid with held notes
            MelodyStep call[32];
            int d = rng.pick ({ 0, 2, 4 });
            int i = 0;
            while (i < 32)
            {
                const int l = rng.pick ({ 2, 2, 2, 4, 3, 6 });
                auto s = note (d, (i % 8 == 0) ? 1.0f : 0.8f, std::min (l, 32 - i));
                if (i > 0 && rest (-0.15f)) s.on = false;
                call[i] = s;
                for (int k = 1; k < s.length && i + k < 32; ++k) call[i + k] = MelodyStep {};
                i += std::max (1, s.length);
                const int mv = i < 16 ? rng.pick ({ 1, 2, 1, -1, 3 }) : rng.pick ({ -1, -2, -1, 1, -3 });
                d = std::clamp (d + mv, -2, 7 * range - 1);
            }
            call[0].on = true;
            for (int k = 0; k < len; ++k)
            {
                auto s = call[k % 32];
                if ((k / 32) % 2 == 1 && k % 32 >= 24 && s.on) s.degree = (k % 32 >= 28) ? 0 : 2;   // second answer resolves
                fold (s, range);
                st[(size_t) k] = s;
            }
            break;
        }
        case mlReese:
        {
            int i = 0, b = 0;
            while (i < len)
            {
                const int l = rng.pick ({ 4, 6, 8, 8, 12 });
                const int deg = (i % 16 == 0) ? prog[(i / 16) % 4] : rng.pick ({ 0, 0, 2, 3, 4, 5, 6 });
                auto s = note (deg, 0.9f, std::min (l, len - i), -1);
                s.slide = rng.chance (0.5f);
                fold (s, range);
                st[(size_t) i] = s;
                i += s.length;
                ++b;
            }
            break;
        }
        case mlChords:
        {
            const int l = dens > 0.6f ? 8 : 16;
            for (int i = 0; i < len; i += l)
            {
                auto s = note (prog[(i / 16) % 4] + (l == 8 && (i % 16) == 8 ? 2 : 0), 0.8f, l);
                s.chord = true;
                fold (s, range);
                st[(size_t) i] = s;
            }
            break;
        }
        case mlRollingBass:
        default:
        {
            // psytrance roll: three 16ths after each kick, root note, fills every 4th bar
            const bool gallop = dens < 0.45f;
            for (int i = 0; i < len; ++i)
            {
                MelodyStep s;
                const int pos = i % 4;
                if (pos != 0 && ! (gallop && pos == 1))
                {
                    s = note (0, pos == 1 ? 0.95f : 0.8f, 1, -1);
                    const int bar = i / 16;
                    if (bar % 4 == 3 && i % 16 >= 12) s.degree = rng.pick ({ 5, 6, 4, 2 });
                    else if (set.variation % 3 == 2 && pos == 3) s.octave = 0;
                }
                fold (s, range);
                st[(size_t) i] = s;
            }
            break;
        }
    }

    // never an empty phrase
    bool any = false;
    for (int i = 0; i < len; ++i) any = any || st[(size_t) i].on;
    if (! any) st[0] = note (0, 1.0f);
    return ph;
}
}
