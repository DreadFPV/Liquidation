#include "Modeler.h"
#include <cmath>
#include <algorithm>

namespace liq
{
namespace
{
    constexpr float pi = 3.14159265358979f;

    inline float polyBlep (double t, double dt) noexcept
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline float svfG (float hz, double sr) noexcept
    {
        return std::tan (pi * std::clamp (hz, 20.0f, (float) sr * 0.45f) / (float) sr);
    }

    const float vowels[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                               { 570, 840, 2410 },  { 300, 870, 2240 } };
}

float Modeler::Osc::next (double inc) noexcept
{
    phase += inc;
    if (phase >= 1.0) phase -= 1.0;
    return (float) (2.0 * phase - 1.0) - polyBlep (phase, inc);
}

void Modeler::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Modeler::reset()
{
    for (size_t i = 0; i < osc.size(); ++i) osc[i].phase = (double) i * 0.137;
    for (auto& f : filt) f = {};
    for (auto& ch : formant) for (auto& f : ch) f = {};
    lfo = 0.0; lastStep = -1; arpIndex = 0; env = 0.0f; meter = 0.0f;
}

float Modeler::noteHz (int midi) const noexcept
{
    return a4 * std::pow (2.0f, ((float) midi - 69.0f) / 12.0f);
}

void Modeler::process (float* L, float* R, int n, const ModelerParams& p, const Scale& scale,
                       double bpm, double ppq, float followMidi) noexcept
{
    a4 = scale.a4;

    // ---- root note: key root (or followed input pitch) in the chosen octave
    int root = 48 + scale.root + 12 * p.octave;           // octave 0 = root in the 3rd octave
    if (p.follow && followMidi > 0.0f)
        root = scale.nearest (followMidi) + 12 * p.octave;
    root = std::clamp (root, 12, 108);
    smoothRoot += 0.2f * ((float) root - smoothRoot);
    const int r = (int) std::lround (smoothRoot);

    const float tone = std::clamp (p.tone, 0.0f, 1.0f);
    const float motion = std::clamp (p.motion, 0.0f, 1.0f);
    const double beatsPerSample = bpm / 60.0 / sr;
    const double rate = std::max (1.0 / 64.0, p.rateBeats);
    const double lfoInc = (0.07 + motion * 0.5) / sr;

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        lfo += lfoInc; if (lfo >= 1.0) lfo -= 1.0;
        const float lfoS = std::sin (2.0f * pi * (float) lfo);
        const double q = ppq + (double) i * beatsPerSample;
        const long long step = (long long) std::floor (q / rate);
        const bool newStep = step != lastStep;
        lastStep = step;

        float l = 0.0f, rr = 0.0f;

        switch (p.mode)
        {
            case mdlPad:
            {
                const int notes[4] { r, scale.shiftDegrees (r, 2), scale.shiftDegrees (r, 4), r + 12 };
                const float det = 0.0015f + motion * 0.006f;
                for (int v = 0; v < 4; ++v)
                {
                    const double hz = noteHz (notes[v]);
                    l  += osc[(size_t) v * 2].next (hz * (1.0 - det) / sr);
                    rr += osc[(size_t) v * 2 + 1].next (hz * (1.0 + det) / sr);
                }
                l *= 0.22f; rr *= 0.22f;
                const float cut = 180.0f * std::pow (2.0f, tone * 6.5f + motion * 1.2f * lfoS);
                float lp, bp, hp;
                filt[0].process (l, svfG (cut, sr), 1.4f, lp, bp, hp); l = lp;
                filt[1].process (rr, svfG (cut * 1.02f, sr), 1.4f, lp, bp, hp); rr = lp;
                break;
            }
            case mdlArp:
            case mdlPsyBass:
            {
                const bool bass = p.mode == mdlPsyBass;
                if (newStep)
                {
                    bool trigger = true;
                    if (bass)
                    {
                        // rolling bass: rests on the beat (where the kick sits)
                        const int perBeat = std::max (1, (int) std::lround (1.0 / rate));
                        trigger = perBeat < 2 || (step % perBeat) != 0;
                        stepNote = r - 12;
                    }
                    else
                    {
                        static const int degs[6] { 0, 2, 4, 7, 4, 2 };
                        stepNote = scale.shiftDegrees (r, degs[arpIndex % 6]) + 12;
                        ++arpIndex;
                    }
                    if (trigger) env = 1.0f;
                    currentNote.store (stepNote, std::memory_order_relaxed);
                }
                const double hz = noteHz (stepNote);
                const float s1 = osc[0].next (hz / sr), s2 = osc[1].next (hz * (bass ? 1.0 : 1.004) / sr);
                const float decay = bass ? 0.06f + tone * 0.12f : 0.08f + tone * 0.35f;
                env *= std::exp (-1.0f / (decay * (float) sr));
                const float cut = (bass ? 120.0f : 300.0f) * std::pow (2.0f, (bass ? 4.5f : 5.5f) * env * (0.4f + tone * 0.8f))
                                  * std::pow (2.0f, motion * 0.8f * lfoS);
                float lp, bp, hp;
                filt[0].process ((s1 + s2) * 0.5f, svfG (cut, sr), bass ? 0.5f : 0.9f, lp, bp, hp);
                const float v = lp * env * (bass ? 0.9f : 0.6f);
                const float panL = bass ? 1.0f : 1.0f - 0.3f * (float) (arpIndex % 2);
                l = v * panL; rr = v * (bass ? 1.0f : 1.3f - panL);
                break;
            }
            case mdlDrone:
            {
                const double h0 = noteHz (r - 12), h1 = noteHz (scale.shiftDegrees (r, 4) - 12), h2 = noteHz (r);
                const float beat = 1.0f + motion * 0.004f * lfoS;
                osc[0].phase += h0 / sr;
                osc[0].phase -= std::floor (osc[0].phase);
                const float s0 = std::sin (2.0f * pi * (float) osc[0].phase);
                const float s1 = osc[1].next (h1 * beat / sr) * 0.5f;
                const float s2 = osc[2].next (h2 / beat / sr) * 0.4f;
                const float cut = 150.0f * std::pow (2.0f, tone * 5.5f + motion * lfoS);
                float lp, bp, hp, lp2;
                filt[0].process (s1 + s2, svfG (cut, sr), 1.0f, lp, bp, hp);
                filt[1].process (s1 - s2 * 0.5f, svfG (cut * 1.1f, sr), 1.0f, lp2, bp, hp);
                l = s0 * 0.45f + lp * 0.6f;
                rr = s0 * 0.45f + lp2 * 0.6f;
                break;
            }
            case mdlVowel:
            {
                const double h = noteHz (r), h5 = noteHz (scale.shiftDegrees (r, 4));
                const float src = osc[0].next (h / sr) * 0.6f + osc[1].next (h5 / sr) * 0.35f;
                const float pos = std::clamp (0.5f + 0.5f * lfoS * (0.2f + motion * 0.8f), 0.0f, 1.0f) * 4.0f;
                const int v0 = std::min (3, (int) pos);
                const float t = pos - (float) v0;
                const float shift = 0.8f + tone * 0.5f;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float sum = 0.0f;
                    for (int b = 0; b < 3; ++b)
                    {
                        const float f = (vowels[v0][b] * (1 - t) + vowels[v0 + 1][b] * t) * shift * (ch ? 1.015f : 1.0f);
                        float lp, bp, hp;
                        formant[(size_t) ch][(size_t) b].process (src, svfG (f, sr), 0.08f, lp, bp, hp);
                        sum += bp * 0.08f * (b == 0 ? 1.0f : (b == 1 ? 0.7f : 0.4f));
                    }
                    (ch ? rr : l) = sum;
                }
                break;
            }
            case mdlNoise:
            default:
            {
                const float cut = 200.0f * std::pow (2.0f, tone * 6.0f + motion * 2.0f * lfoS);
                float lp, bp, hp;
                filt[0].process (rnd(), svfG (cut, sr), 1.6f, lp, bp, hp); l = lp * 0.5f + bp * 0.4f;
                filt[1].process (rnd(), svfG (cut, sr), 1.6f, lp, bp, hp); rr = lp * 0.5f + bp * 0.4f;
                break;
            }
        }

        L[i] = l;
        R[i] = rr;
        peak = std::max (peak, std::max (std::abs (l), std::abs (rr)));
    }
    meter = std::max (peak, meter * 0.9f);
    level.store (meter, std::memory_order_relaxed);
    if (p.mode != mdlArp && p.mode != mdlPsyBass) currentNote.store (r, std::memory_order_relaxed);
}
}
