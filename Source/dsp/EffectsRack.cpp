#include "EffectsRack.h"
#include <cmath>
#include <algorithm>

namespace liq
{
namespace
{
    constexpr double twoPi = 6.283185307179586;

    inline float softClip (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    // simple fractional delay line
    struct DelayLine
    {
        std::vector<float> buf; int pos = 0;
        void prepare (int len) { buf.assign ((size_t) std::max (4, len), 0.0f); pos = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
        void push (float x) noexcept { buf[(size_t) pos] = x; if (++pos >= (int) buf.size()) pos = 0; }
        float read (float delay) const noexcept
        {
            const int L = (int) buf.size();
            float rp = (float) pos - 1.0f - std::clamp (delay, 0.0f, (float) L - 3.0f);
            while (rp < 0) rp += (float) L;
            const int i0 = (int) rp; const float fr = rp - (float) i0;
            const float a = buf[(size_t) (i0 % L)], b = buf[(size_t) ((i0 + 1) % L)];
            return a + (b - a) * fr;
        }
    };

    const char* fxDelayDivNames[] { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2" };
    const double fxDelayDivBeats[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 2.0 };
    int delayDivIndex (float a) { return std::clamp ((int) std::lround (a * 9.0f), 0, 9); }

    // ======================================================================
    class Chorus : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (sr * 0.05)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); phase = 0; fb[0] = fb[1] = 0; }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            const double rate = 0.05 * std::pow (100.0, (double) p.a);           // 0.05..5 Hz
            const float depth = 0.0005f + p.b * 0.0075f;                         // seconds
            const float feedback = (p.c - 0.5f) * 1.4f;
            const float stereo = p.d * 0.5f;
            for (int i = 0; i < n; ++i)
            {
                phase += rate / sr; if (phase >= 1.0) phase -= 1.0;
                const float mL = 0.5f + 0.5f * (float) std::sin (twoPi * phase);
                const float mR = 0.5f + 0.5f * (float) std::sin (twoPi * (phase + stereo));
                const float base = 0.007f * (float) sr;
                const float dL = dl[0].read (base + mL * depth * (float) sr);
                const float dR = dl[1].read (base + mR * depth * (float) sr);
                dl[0].push (l[i] + dL * feedback);
                dl[1].push (r[i] + dR * feedback);
                l[i] = dL; r[i] = dR;
            }
        }
    private:
        double sr = 48000, phase = 0; DelayLine dl[2]; float fb[2] {};
    };

    // ======================================================================
    class Compressor : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { env = 0; }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            const float threshDb = -48.0f + p.a * 48.0f;
            const float ratio = 1.0f + p.b * p.b * 19.0f;
            const float atkMs = 0.1f * std::pow (1000.0f, p.c);                   // 0.1..100 ms
            const float relMs = 10.0f * std::pow (100.0f, p.d);                   // 10..1000 ms
            const float aA = std::exp (-1.0f / (atkMs * 0.001f * (float) sr));
            const float aR = std::exp (-1.0f / (relMs * 0.001f * (float) sr));
            const float makeup = juce::Decibels::decibelsToGain (-threshDb * (1.0f - 1.0f / ratio) * 0.5f);
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (l[i]), std::abs (r[i]));
                const float xDb = juce::Decibels::gainToDecibels (x + 1.0e-9f, -120.0f);
                const float over = xDb - threshDb;
                const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                env = gr > env ? aA * env + (1 - aA) * gr : aR * env + (1 - aR) * gr;
                const float g = juce::Decibels::decibelsToGain (-env) * makeup;
                l[i] *= g; r[i] *= g;
            }
        }
    private:
        double sr = 48000; float env = 0;
    };

    // ======================================================================
    class Delay : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (sr * 4.2)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); lp[0] = lp[1] = 0; smoothed = -1; }
        void process (float* l, float* r, int n, const FxSlotParams& p, double bpm) override
        {
            const float target = (float) (fxDelayDivBeats[delayDivIndex (p.a)] * 60.0 / std::max (20.0, bpm) * sr);
            if (smoothed < 0) smoothed = target;
            const float fbk = p.b * 0.95f;
            const float pp = p.c;
            const float cutoff = 500.0f * std::pow (40.0f, p.d);
            const float a = 1.0f - std::exp (-6.2831853f * cutoff / (float) sr);
            for (int i = 0; i < n; ++i)
            {
                smoothed += 0.0005f * (target - smoothed);
                const float dL = dl[0].read (smoothed), dR = dl[1].read (smoothed);
                lp[0] += a * (dL - lp[0]); lp[1] += a * (dR - lp[1]);
                const float inMono = 0.5f * (l[i] + r[i]);
                const float inL = lerp (l[i], pp > 0.5f ? inMono : l[i], pp);
                const float inR = lerp (r[i], 0.0f, pp);
                const float fL = lerp (lp[0], lp[1], pp), fR = lerp (lp[1], lp[0], pp);
                dl[0].push (softClip (inL + fL * fbk));
                dl[1].push (softClip (inR + fR * fbk));
                l[i] = dL; r[i] = dR;
            }
        }
    private:
        double sr = 48000; DelayLine dl[2]; float lp[2] {}; float smoothed = -1;
    };

    // ======================================================================
    class Distortion : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { tone[0] = tone[1] = 0; }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            const float drive = juce::Decibels::decibelsToGain (p.a * 40.0f);
            const int type = std::clamp ((int) (p.b * 4.0f), 0, 3);
            const float cutoff = 1000.0f * std::pow (20.0f, p.c);
            const float a = 1.0f - std::exp (-6.2831853f * std::min (cutoff, (float) sr * 0.45f) / (float) sr);
            const float out = juce::Decibels::decibelsToGain (-24.0f + p.d * 30.0f) / std::sqrt (drive) * 2.0f;
            float* ch[2] { l, r };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float x = ch[c][i] * drive, y;
                    switch (type)
                    {
                        case 0:  y = softClip (x); break;
                        case 1:  y = std::clamp (x, -1.0f, 1.0f); break;
                        case 2:  y = std::sin (x); break;                                 // fold
                        default: y = softClip (std::abs (x) * 1.2f - 0.3f); break;        // rectify
                    }
                    tone[c] += a * (y - tone[c]);
                    ch[c][i] = tone[c] * out;
                }
        }
    private:
        double sr = 48000; float tone[2] {};
    };

    // ======================================================================
    class Equalizer : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { for (auto& b : bands) for (auto& f : b) { f.z1 = f.z2 = 0; } last[0] = -99; }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            if (p.a != last[0] || p.b != last[1] || p.c != last[2] || p.d != last[3])
            {
                last[0] = p.a; last[1] = p.b; last[2] = p.c; last[3] = p.d;
                for (int c = 0; c < 2; ++c)
                {
                    bands[c][0].lowShelf (sr, 150.0, (p.a - 0.5) * 30.0);
                    bands[c][1].peak (sr, 200.0 * std::pow (40.0, (double) p.c), 0.9, (p.b - 0.5) * 30.0);
                    bands[c][2].highShelf (sr, 6000.0, (p.d - 0.5) * 30.0);
                }
            }
            for (int i = 0; i < n; ++i)
            {
                float x = l[i], y = r[i];
                for (int b = 0; b < 3; ++b) { x = bands[0][b].process (x); y = bands[1][b].process (y); }
                l[i] = x; r[i] = y;
            }
        }
    private:
        double sr = 48000; EffectsRack::Biquad bands[2][3]; float last[4] { -99, -99, -99, -99 };
    };

    // ======================================================================
    class Filter : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { for (auto& c : s1) c = 0; for (auto& c : s2) c = 0; }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            const float fc = std::min (20.0f * std::pow (1000.0f, p.a), (float) sr * 0.45f);
            const float g = std::tan (3.14159265f * fc / (float) sr);
            const float k = 2.0f - p.b * 1.9f;
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float morph = p.c * 2.0f;
            const float drive = 1.0f + p.d * p.d * 10.0f;
            float* ch[2] { l, r };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float x = softClip (ch[c][i] * drive) / std::sqrt (drive);
                    const float v3 = x - s2[c];
                    const float v1 = a1 * s1[c] + a2 * v3;
                    const float v2 = s2[c] + a2 * s1[c] + a3 * v3;
                    s1[c] = 2 * v1 - s1[c]; s2[c] = 2 * v2 - s2[c];
                    const float lp = v2, bp = v1 * k, hp = x - k * v1 - v2;
                    ch[c][i] = morph < 1.0f ? lerp (lp, bp, morph) : lerp (bp, hp, morph - 1.0f);
                }
        }
    private:
        double sr = 48000; float s1[2] {}, s2[2] {};
    };

    // ======================================================================
    class Reverb : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int maxBlock) override { rev.prepare (s); wl.assign ((size_t) maxBlock, 0); wr.assign ((size_t) maxBlock, 0); }
        void reset() override { rev.reset(); }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            rev.setParams (p.a, p.b, p.c, p.d * 200.0f);
            rev.process (l, r, wl.data(), wr.data(), n);
            for (int i = 0; i < n; ++i) { l[i] = wl[(size_t) i]; r[i] = wr[(size_t) i]; }
        }
    private:
        FdnReverb rev; std::vector<float> wl, wr;
    };

    // ======================================================================
    class Shifter : public EffectsRack::Effect
    {
    public:
        void prepare (double s, int) override
        {
            sr = s;
            for (auto& d : dl) d.prepare ((int) (sr * 0.12));
            reset();
        }
        void reset() override
        {
            for (auto& d : dl) d.reset();
            ph[0] = 0; ph[1] = 0.5; osc = 0; fbS[0] = fbS[1] = 0;
            for (auto& c : hs) for (auto& v : c) v = 0;
        }
        void process (float* l, float* r, int n, const FxSlotParams& p, double) override
        {
            const float semis = std::round ((p.a - 0.5f) * 24.0f);
            const float freqShift = (p.b - 0.5f) * 1000.0f;
            const float fbk = p.c * 0.9f;
            const float spread = p.d * 0.3f;   // semitones of stereo detune
            const float win = (float) (0.05 * sr);
            float* ch[2] { l, r };

            for (int i = 0; i < n; ++i)
            {
                osc += freqShift / sr; if (osc >= 1.0) osc -= 1.0; if (osc < 0.0) osc += 1.0;
                const float co = (float) std::cos (twoPi * osc), so = (float) std::sin (twoPi * osc);

                for (int c = 0; c < 2; ++c)
                {
                    const float x = ch[c][i] + fbS[c] * fbk;
                    dl[c].push (x);

                    // granular pitch shift with two crossfaded taps
                    const float ratio = std::pow (2.0f, (semis + (c == 0 ? -spread : spread)) / 12.0f);
                    ph[c] += (1.0 - ratio) / win;
                    if (ph[c] >= 1.0) ph[c] -= 1.0;
                    if (ph[c] < 0.0) ph[c] += 1.0;
                    const float p1 = (float) ph[c], p2 = (float) std::fmod (ph[c] + 0.5, 1.0);
                    const float g1 = std::sin (3.14159265f * p1), g2 = std::sin (3.14159265f * p2);
                    float y = dl[c].read (p1 * win + 2.0f) * g1 + dl[c].read (p2 * win + 2.0f) * g2;

                    // frequency shift (Hilbert pair)
                    if (std::abs (freqShift) > 0.5f)
                    {
                        float re, im;
                        hilbert (c, y, re, im);
                        y = re * co - im * so;
                    }
                    fbS[c] = softClip (y);
                    ch[c][i] = y;
                }
            }
        }
    private:
        void hilbert (int c, float x, float& re, float& im) noexcept
        {
            static const float ca[4] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static const float cb[4] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            float a = x, b = x;
            for (int s = 0; s < 4; ++s)
            {
                float* st = hs[c] + s * 4;
                const float ka = ca[s] * ca[s];
                const float ya = ka * (a + st[1]) - st[0];
                st[0] = st[2]; st[2] = a; st[1] = st[3]; st[3] = ya; a = ya;
                // keep proper 2-sample history: st[0]=x[n-2], st[2]=x[n-1]; st[1]=y[n-2], st[3]=y[n-1]
            }
            for (int s = 0; s < 4; ++s)
            {
                float* st = hs[c] + 16 + s * 4;
                const float kb = cb[s] * cb[s];
                const float yb = kb * (b + st[1]) - st[0];
                st[0] = st[2]; st[2] = b; st[1] = st[3]; st[3] = yb; b = yb;
            }
            re = a;
            im = hDelay[c];
            hDelay[c] = b;
        }

        double sr = 48000; DelayLine dl[2]; double ph[2] { 0, 0.5 }; double osc = 0; float fbS[2] {};
        float hs[2][32] {}; float hDelay[2] {};
    };
}

// ============================================================================
juce::StringArray fxKnobLabels (int type)
{
    switch (type)
    {
        case fxChorus:     return { "Rate", "Depth", "Feedback", "Stereo" };
        case fxCompressor: return { "Threshold", "Ratio", "Attack", "Release" };
        case fxDelay:      return { "Time", "Feedback", "Ping Pong", "Tone" };
        case fxDistortion: return { "Drive", "Type", "Tone", "Output" };
        case fxEqualizer:  return { "Low", "Mid", "Mid Freq", "High" };
        case fxFilter:     return { "Cutoff", "Resonance", "LP-BP-HP", "Drive" };
        case fxReverb:     return { "Size", "Damping", "Width", "Pre-Delay" };
        case fxShifter:    return { "Pitch", "Freq Shift", "Feedback", "Spread" };
        default:           return { "A", "B", "C", "D" };
    }
}

juce::String fxKnobText (int type, int k, float v)
{
    auto pct = [] (float x) { return juce::String ((int) std::lround (x * 100.0f)) + "%"; };
    auto db = [] (float x) { return juce::String (x, 1) + " dB"; };
    auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz"; };
    switch (type)
    {
        case fxChorus:     return k == 0 ? juce::String (0.05 * std::pow (100.0, (double) v), 2) + " Hz" : pct (v);
        case fxCompressor:
            if (k == 0) return db (-48.0f + v * 48.0f);
            if (k == 1) return juce::String (1.0f + v * v * 19.0f, 1) + ":1";
            if (k == 2) return juce::String (0.1f * std::pow (1000.0f, v), 1) + " ms";
            return juce::String ((int) (10.0f * std::pow (100.0f, v))) + " ms";
        case fxDelay:      return k == 0 ? juce::String (fxDelayDivNames[delayDivIndex (v)]) : (k == 3 ? hz (500.0f * std::pow (40.0f, v)) : pct (v));
        case fxDistortion:
            if (k == 0) return db (v * 40.0f);
            if (k == 1) { static const char* t[] { "Soft", "Hard", "Fold", "Rectify" }; return t[std::clamp ((int) (v * 4.0f), 0, 3)]; }
            if (k == 2) return hz (1000.0f * std::pow (20.0f, v));
            return db (-24.0f + v * 30.0f);
        case fxEqualizer:  return k == 2 ? hz (200.0f * std::pow (40.0f, v)) : db ((v - 0.5f) * 30.0f);
        case fxFilter:     return k == 0 ? hz (20.0f * std::pow (1000.0f, v)) : pct (v);
        case fxReverb:     return k == 3 ? juce::String ((int) (v * 200.0f)) + " ms" : pct (v);
        case fxShifter:
            if (k == 0) { const int s = (int) std::lround ((v - 0.5f) * 24.0f); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }
            if (k == 1) return juce::String ((int) ((v - 0.5f) * 1000.0f)) + " Hz";
            return pct (v);
        default: return pct (v);
    }
}

// ============================================================================
void EffectsRack::Biquad::lowShelf (double fs, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = twoPi * f / fs, cw = std::cos (w), sw = std::sin (w);
    const double alpha = sw / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1) + (A - 1) * cw + sA;
    b0 = (float) (A * ((A + 1) - (A - 1) * cw + sA) / a0);
    b1 = (float) (2 * A * ((A - 1) - (A + 1) * cw) / a0);
    b2 = (float) (A * ((A + 1) - (A - 1) * cw - sA) / a0);
    a1 = (float) (-2 * ((A - 1) + (A + 1) * cw) / a0);
    a2 = (float) (((A + 1) + (A - 1) * cw - sA) / a0);
}

void EffectsRack::Biquad::highShelf (double fs, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = twoPi * f / fs, cw = std::cos (w), sw = std::sin (w);
    const double alpha = sw / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1) - (A - 1) * cw + sA;
    b0 = (float) (A * ((A + 1) + (A - 1) * cw + sA) / a0);
    b1 = (float) (-2 * A * ((A - 1) + (A + 1) * cw) / a0);
    b2 = (float) (A * ((A + 1) + (A - 1) * cw - sA) / a0);
    a1 = (float) (2 * ((A - 1) - (A + 1) * cw) / a0);
    a2 = (float) (((A + 1) - (A - 1) * cw - sA) / a0);
}

void EffectsRack::Biquad::peak (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = twoPi * std::min (f, fs * 0.45) / fs;
    const double alpha = std::sin (w) / (2.0 * q), cw = std::cos (w);
    const double a0 = 1 + alpha / A;
    b0 = (float) ((1 + alpha * A) / a0);
    b1 = (float) (-2 * cw / a0);
    b2 = (float) ((1 - alpha * A) / a0);
    a1 = (float) (-2 * cw / a0);
    a2 = (float) ((1 - alpha / A) / a0);
}

void EffectsRack::Biquad::lowpass (double fs, double f, double q)
{
    const double w = twoPi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q), a0 = 1 + alpha;
    b0 = (float) ((1 - cw) * 0.5 / a0); b1 = (float) ((1 - cw) / a0); b2 = b0;
    a1 = (float) (-2 * cw / a0); a2 = (float) ((1 - alpha) / a0);
}

void EffectsRack::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto& s : slots)
    {
        s.fx[fxChorus]     = std::make_unique<Chorus>();
        s.fx[fxCompressor] = std::make_unique<Compressor>();
        s.fx[fxDelay]      = std::make_unique<Delay>();
        s.fx[fxDistortion] = std::make_unique<Distortion>();
        s.fx[fxEqualizer]  = std::make_unique<Equalizer>();
        s.fx[fxFilter]     = std::make_unique<Filter>();
        s.fx[fxReverb]     = std::make_unique<Reverb>();
        s.fx[fxShifter]    = std::make_unique<Shifter>();
        for (auto& f : s.fx) f->prepare (sr, maxBlock);
        s.dryL.assign ((size_t) maxBlock, 0.0f);
        s.dryR.assign ((size_t) maxBlock, 0.0f);
    }
    reset();
}

void EffectsRack::reset()
{
    for (auto& s : slots)
    {
        for (auto& f : s.fx) if (f) f->reset();
        s.smoothedMix = 0.0f;
        s.lastType = -1;
    }
}

void EffectsRack::process (float* l, float* r, int n, const std::array<FxSlotParams, numFxSlots>& params, double bpm)
{
    for (int k = 0; k < numFxSlots; ++k)
    {
        auto& s = slots[(size_t) k];
        const auto& p = params[(size_t) k];
        const int type = std::clamp (p.type, 0, fxNumTypes - 1);

        if (type != s.lastType)
        {
            s.fx[(size_t) type]->reset();
            s.lastType = type;
        }

        const float target = p.on ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f;
        if (target <= 0.0f && s.smoothedMix < 1.0e-4f) { s.smoothedMix = 0.0f; continue; }

        std::copy (l, l + n, s.dryL.begin());
        std::copy (r, r + n, s.dryR.begin());
        s.fx[(size_t) type]->process (l, r, n, p, bpm);

        for (int i = 0; i < n; ++i)
        {
            s.smoothedMix += 0.002f * (target - s.smoothedMix);
            const float m = s.smoothedMix;
            l[i] = s.dryL[(size_t) i] + (l[i] - s.dryL[(size_t) i]) * m;
            r[i] = s.dryR[(size_t) i] + (r[i] - s.dryR[(size_t) i]) * m;
        }
    }
}
}
