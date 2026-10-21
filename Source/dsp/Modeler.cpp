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
    lastMelStep = -1; kickIndex = 0; ampEnv = filtEnv = 0.0f; gateOpen = false; gateLeft = 0.0;
    duck = envFollow = 0.0f; attacking = false; lastSettings = { -1, -1, -1.0f, -1, -1 };
    for (auto& f : melFilt) f = {};
}

float Modeler::noteHz (int midi) const noexcept
{
    return a4 * std::pow (2.0f, ((float) midi - 69.0f) / 12.0f);
}

float Modeler::noteHzF (float midi) const noexcept
{
    return a4 * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

int Modeler::styleOctaveOffset (int style) noexcept
{
    switch (style)
    {
        case mlGoaLead: case mlRollingArp: case mlHook: case mlStabs: case mlChords: return 12;
        default: return 0;
    }
}

void Modeler::triggerMelodyStep (const MelodyStep& st, const Scale& scale, int root, double stepSamples, float glide, int style) noexcept
{
    const int base = liq::stepNote (st, scale, root);
    mTarget = (float) base;
    const bool doGlide = (st.slide || style == mlReese) && gateOpen;
    const float glideMs = 15.0f + glide * 180.0f;
    glideCoef = doGlide ? 1.0f - std::exp (-1.0f / (glideMs * 0.001f * (float) sr)) : 1.0f;
    if (! doGlide) mNote = mTarget;
    chordNotes = st.chord ? 3 : 1;
    if (st.chord)
    {
        chordOffsets[0] = 0.0f;
        chordOffsets[1] = (float) (scale.shiftDegrees (base, 2) - base);
        chordOffsets[2] = (float) (scale.shiftDegrees (base, 4) - base);
    }
    accent = st.velocity;
    if (! doGlide) { attacking = true; filtEnv = 1.0f; }
    gateLeft = std::max (1.0, stepSamples * (double) st.length * 0.92);
    gateOpen = true;
    currentNote.store (base, std::memory_order_relaxed);
}

void Modeler::renderMelody (float* L, float* R, int n, const ModelerParams& p, const Scale& scale, int root,
                            double bpm, double ppq, const uint8_t* onsets, const float* inEnv) noexcept
{
    const auto& ms = p.melody;
    if (ms.style != lastSettings.style || ms.variation != lastSettings.variation || ms.range != lastSettings.range
        || ms.steps != lastSettings.steps || std::abs (ms.density - lastSettings.density) > 0.02f)
    {
        phrase = generatePhrase (ms);
        lastSettings = ms;
        lastSettings.density = ms.density;
    }

    const int style = ms.style;
    const bool bass = style == mlReese || style == mlRollingBass || style == mlAcid || style == mlDarkPulse;
    const bool pluck = ! (style == mlReese || style == mlChords || style == mlHook);
    const float tone = std::clamp (p.tone, 0.0f, 1.0f);
    const float motion = std::clamp (p.motion, 0.0f, 1.0f);
    const double rate = std::max (1.0 / 64.0, p.rateBeats);
    const double beatsPerSample = bpm / 60.0 / sr;
    const double stepSamples = rate / beatsPerSample;
    const int r = root + styleOctaveOffset (style);

    const float atk = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    const float rel = 1.0f - std::exp (-1.0f / (0.035f * (float) sr));
    const float decayS = pluck ? (0.07f + tone * 0.35f) : 2.5f;
    const float dec = std::exp (-1.0f / (decayS * (float) sr));
    const float fdec = std::exp (-1.0f / ((0.05f + tone * 0.25f) * (float) sr));
    const float envF = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));
    const float duckRel = std::exp (-1.0f / (0.11f * (float) sr));
    const float baseCut = (bass ? 90.0f : 220.0f) * std::pow (2.0f, tone * 4.5f);
    const float envAmt = (style == mlAcid ? 5.0f : 3.5f) * (0.4f + 0.6f * accent);
    const float reso = style == mlAcid ? 0.25f + (1.0f - tone) * 0.15f : (style == mlReese ? 0.9f : 1.1f);
    const float det = 0.002f + motion * 0.008f + (style == mlReese ? 0.006f : 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const double q = ppq + (double) i * beatsPerSample;
        const long long step = (long long) std::floor (q / rate);
        const bool onset = onsets != nullptr && onsets[i] != 0;
        if (onset) duck = 1.0f;

        if (p.trigger == trigKickStep)
        {
            if (onset)
            {
                for (int k = 0; k < phrase.length; ++k)
                {
                    kickIndex = (kickIndex + 1) % phrase.length;
                    if (phrase.steps[(size_t) kickIndex].on) break;
                }
                auto st = phrase.steps[(size_t) kickIndex];
                st.length = std::max (st.length, 2);
                triggerMelodyStep (st, scale, r, stepSamples * 2.0, p.glide, style);
                melodyStep.store (kickIndex, std::memory_order_relaxed);
            }
        }
        else if (step != lastMelStep)
        {
            lastMelStep = step;
            const int idx = (int) (((step % phrase.length) + phrase.length) % phrase.length);
            const auto& st = phrase.steps[(size_t) idx];
            melodyStep.store (idx, std::memory_order_relaxed);
            const bool blocked = p.trigger == trigBetweenKicks && duck > 0.6f;
            if (st.on && ! blocked) triggerMelodyStep (st, scale, r, stepSamples, p.glide, style);
        }

        // envelopes
        if (gateOpen)
        {
            gateLeft -= 1.0;
            if (gateLeft <= 0.0) gateOpen = false;
        }
        if (gateOpen)
        {
            const float target = pluck ? 0.0f : 0.85f;
            if (attacking)
            {
                ampEnv += atk * (1.05f - ampEnv);
                if (ampEnv >= 1.0f) { ampEnv = 1.0f; attacking = false; }
            }
            else
            {
                ampEnv = target + (ampEnv - target) * dec;
            }
        }
        else
        {
            ampEnv -= rel * ampEnv;
        }
        filtEnv *= fdec;
        mNote += glideCoef * (mTarget - mNote);
        duck *= duckRel;
        if (inEnv != nullptr) envFollow += envF * (std::min (1.0f, inEnv[i] * 2.5f) - envFollow);

        lfo += (0.15 + motion * 5.0) / sr; if (lfo >= 1.0) lfo -= 1.0;
        const float vib = motion * 0.12f * std::sin (2.0f * pi * (float) lfo);

        float l = 0.0f, rr = 0.0f;
        for (int v = 0; v < chordNotes; ++v)
        {
            const double hz = noteHzF (mNote + (v > 0 ? chordOffsets[(size_t) v] : 0.0f) + vib);
            l  += mel[(size_t) v * 2].next (hz * (1.0 - det) / sr);
            rr += mel[(size_t) v * 2 + 1].next (hz * (1.0 + det) / sr);
        }
        if (bass)
        {
            const double sub = noteHzF (mNote - 12.0f);
            const float s = mel[6].next (sub / sr);
            l += s * 0.5f; rr += s * 0.5f;
        }
        const float norm = chordNotes > 1 ? 0.28f : 0.45f;
        const float cut = baseCut * std::pow (2.0f, envAmt * filtEnv);
        float lp, bp, hp;
        melFilt[0].process (l * norm, svfG (cut, sr), reso, lp, bp, hp); l = lp;
        melFilt[1].process (rr * norm, svfG (cut * 1.01f, sr), reso, lp, bp, hp); rr = lp;

        float g = ampEnv * (0.55f + 0.45f * accent);
        if (p.trigger == trigKickGate) g *= envFollow;
        if (p.trigger == trigBetweenKicks) g *= 1.0f - duck;
        L[i] = std::tanh (l * g * 1.6f);
        R[i] = std::tanh (rr * g * 1.6f);
    }
}

void Modeler::process (float* L, float* R, int n, const ModelerParams& p, const Scale& scale,
                       double bpm, double ppq, float followMidi, const uint8_t* onsets, const float* inEnv) noexcept
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
    if (p.mode == mdlMelody)
    {
        renderMelody (L, R, n, p, scale, r, bpm, ppq, onsets, inEnv);
        for (int i = 0; i < n; ++i) peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
        meter = std::max (peak, meter * 0.9f);
        level.store (meter, std::memory_order_relaxed);
        return;
    }
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
                        arpNote = r - 12;
                    }
                    else
                    {
                        static const int degs[6] { 0, 2, 4, 7, 4, 2 };
                        arpNote = scale.shiftDegrees (r, degs[arpIndex % 6]) + 12;
                        ++arpIndex;
                    }
                    if (trigger) env = 1.0f;
                    currentNote.store (arpNote, std::memory_order_relaxed);
                }
                const double hz = noteHz (arpNote);
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
