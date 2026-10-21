#pragma once

#include "Scale.h"
#include "Melody.h"
#include <array>
#include <cstdint>
#include <atomic>

namespace liq
{
enum ModelerMode { mdlPad = 0, mdlArp, mdlDrone, mdlPsyBass, mdlVowel, mdlNoise, mdlMelody, mdlNumModes };

struct ModelerParams
{
    int    mode = mdlPad;
    int    octave = 0;        // -3 .. +2
    double rateBeats = 0.25;  // step length for Arp / Psy Bass
    float  tone = 0.5f;       // brightness
    float  motion = 0.3f;     // movement (LFO depth / detune / sweep)
    bool   follow = false;    // follow the pitch of the main input

    // melody generator (mode Melody)
    MelodySettings melody;
    int    trigger = trigGrid;
    float  glide = 0.2f;      // 0..1
};

// Internal sound source that can be morphed with the main input (no sidechain needed).
// Everything is generated in the current key and locked to the host tempo.
class Modeler
{
public:
    void prepare (double sampleRate);
    void reset();

    // followMidi: detected main-input pitch (fractional midi) or < 0 when unknown
    // onsets: 1 where a hit of the main input was detected; inEnv: main input envelope (0..1)
    void process (float* L, float* R, int n, const ModelerParams& p, const Scale& scale,
                  double bpm, double ppq, float followMidi,
                  const uint8_t* onsets = nullptr, const float* inEnv = nullptr) noexcept;

    std::atomic<int> melodyStep { -1 };   // UI: playing step of the phrase
    static int styleOctaveOffset (int style) noexcept;

    std::atomic<float> level { 0.0f };    // UI meter
    std::atomic<int> currentNote { -1 };

private:
    struct Osc
    {
        double phase = 0.0;
        float next (double inc) noexcept;   // polyBLEP saw, -1..1
    };
    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        // returns lowpass, writes band & high
        void process (float x, float g, float k, float& lp, float& bp, float& hp) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };

    float noteHz (int midi) const noexcept;
    float noteHzF (float midi) const noexcept;
    void renderMelody (float* L, float* R, int n, const ModelerParams& p, const Scale& scale, int root,
                       double bpm, double ppq, const uint8_t* onsets, const float* inEnv) noexcept;
    void triggerMelodyStep (const MelodyStep& st, const Scale& scale, int root, double stepSamples, float glide, int style) noexcept;
    float rnd() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f) * 2.0f - 1.0f; }

    double sr = 48000.0;
    std::array<Osc, 16> osc;
    std::array<Svf, 2> filt;
    std::array<std::array<Svf, 3>, 2> formant;
    double lfo = 0.0;
    long long lastStep = -1;
    int arpIndex = 0;
    float env = 0.0f;
    int arpNote = 60;
    float smoothRoot = 60.0f;
    float a4 = 440.0f;
    uint32_t rng = 0x2468aceu;
    float meter = 0.0f;

    // melody state
    MelodyPhrase phrase;
    MelodySettings lastSettings { -1, -1, -1.0f, -1, -1 };
    long long lastMelStep = -1;
    int kickIndex = 0;
    float mNote = 60.0f, mTarget = 60.0f, glideCoef = 1.0f;
    int   chordNotes = 1;
    std::array<float, 3> chordOffsets {};
    float ampEnv = 0.0f, filtEnv = 0.0f, accent = 0.8f;
    double gateLeft = 0.0;
    bool  gateOpen = false, attacking = false;
    float duck = 0.0f, envFollow = 0.0f;
    std::array<Osc, 8> mel;
    Svf melFilt[2];
};
}
