#pragma once

#include "Scale.h"
#include "PitchDetector.h"
#include "GrainFilter.h"
#include "GrainDelay.h"
#include "Imager.h"
#include "../Params.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <vector>

namespace liq
{
// All values the engine needs for one sub-block, already modulated.
struct EngineParams
{
    // transport
    double bpm = 120.0;
    double ppq = 0.0;            // at start of the sub-block
    bool   playing = false;

    // grain
    int   rateMode = rateFree;
    float grainRate = 0.45f;     // 0..1, interpreted per mode
    float sizeMs = 120.0f;
    float sizeRatio = 2.0f;
    bool  sizeLink = true;
    float winShape = 0.0f, winSkew = 0.0f, jitter = 0.0f;

    // playback
    int   playMode = playStretch, stretchMode = stretchGrid, stretchRetrig = 16;
    float stretch = 0.0f;
    bool  stretchSync = false;
    float stretchSens = 0.5f;
    bool  stretchActive = true;
    float scrubPos = 0.0f;
    int   scrubMode = 0, loopLength = 2;
    bool  recHold = false, reverse = false;
    float revProb = 0.0f, spray = 0.05f, blur = 0.0f, levelDb = 0.0f;

    // pitch
    float pitch = 0.0f;
    int   pitchMode = pitchScale;
    float detune = 0.0f;
    int   retune = retuneSmooth, chordMode = chordOff;
    float arpRand = 0.0f, chordSpread = 0.5f;
    std::array<int, numIntervals>  intervals { 0, 2, 4, 7, 9, 11 };
    std::array<bool, numIntervals> intervalOn { true, true, true, false, false, false };

    // per-grain filter
    bool fltOn = false;
    GrainFilterParams filter;

    // imager (per grain part)
    bool  imgOn = true;
    int   spreadMode = spAlternate;
    bool  smoothSpread = false;
    float spread = 0.3f, pan = 0.0f;
    ImagerParams imager;

    // grain delay
    bool  dlyOn = false, dlyPingPong = false, dlySwap = false, dlySync = true;
    float dlyTimeMs = 250.0f;
    int   dlyDiv = 8, dlyTaps = 3, dlyPattern = shHold;
    float dlyShift = 0.0f;
    GrainDelayParams delay;

    Scale scale;
};

// Snapshot published to the editor.
struct EngineDisplay
{
    static constexpr int maxGrains = 64;
    std::atomic<int64_t> regionStart { 0 }, regionLength { 1 };
    std::atomic<float> playhead { 0 };          // 0..1 within region
    std::atomic<float> sprayLen { 0 };          // 0..1 of region
    std::atomic<int> numGrains { 0 };
    std::array<std::atomic<float>, maxGrains> grainPos {};
    std::array<std::atomic<float>, maxGrains> grainTrail {};
    std::array<std::atomic<float>, maxGrains> grainAmp {};
    std::array<std::atomic<int8_t>, maxGrains> grainSide {};   // -1 left, 1 right
    static constexpr int maxMarkers = 16;
    std::array<std::atomic<int64_t>, maxMarkers> markers {};
    std::atomic<int> numMarkers { 0 };
    std::atomic<bool> recording { true };
    std::atomic<float> inputLevel { 0 };
    std::atomic<int> lastReverse { 0 };          // 1 if last grain was reversed
    std::atomic<float> stretchedBpm { 120 };
    std::atomic<float> detectedPitch { -1 };
    std::atomic<bool> isScrub { false };
};

class GrainEngine
{
public:
    GrainEngine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    // Process a block: input is the (gain-adjusted) dry signal, output receives the wet grains.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  const EngineParams& p);

    void noteOn (int note)  noexcept;
    void noteOff (int note) noexcept;
    void triggerManual()    noexcept { manualTrigger = true; }

    // Access for the waveform view (read-only, racy by design).
    const std::vector<float>& bufferL() const noexcept { return ringL; }
    const std::vector<float>& bufferR() const noexcept { return ringR; }
    int64_t bufferSize() const noexcept { return ringSize; }
    int64_t writeHead() const noexcept { return writeAbs; }

    EngineDisplay display;

    // helpers shared with the UI
    static float stretchFactor (float v, bool sync) noexcept;              // returns 0 for freeze speed
    static double rateHz (const EngineParams& p, double lastMidiHz, bool& valid) noexcept;
    static juce::String rateText (int rateMode, float v, const Scale& scale, double bpm);
    static float rateValueForDivision (int rateMode, const char* divisionName);   // inverse for synced modes

private:
    struct Grain
    {
        bool active = false;
        int  delay = 0;              // samples before it starts
        double pos = 0.0;            // absolute read position (fractional)
        double inc = 1.0;            // read increment
        int  length = 0, age = 0;
        float gain = 1.0f;
        float panStart = 0.0f, panEnd = 0.0f;
        bool  isTap = false, swapLR = false, reversed = false;
        bool  wrap = false;
        int64_t wrapStart = 0, wrapLen = 1;
        bool  filtered = false;
        GrainFilter filter;
        int   poolIndex = 0;
        uint32_t serial = 0;
    };

    struct SpawnInfo
    {
        double startPos;
        double ratio;
        bool   reversed;
        float  gain, pan, prevPan;
        int    length;
        int    delay;
        bool   isTap, swapLR;
    };

    void spawnGrains (const EngineParams& p, int offsetInBlock);
    Grain* allocateGrain();
    void startGrain (const SpawnInfo& s, const EngineParams& p);
    float nextPanPattern (const EngineParams& p) noexcept;
    float semitonesFor (const EngineParams& p, double readPos, int intervalDegrees) noexcept;
    float shiftForTap (const EngineParams& p, int tapIndex) noexcept;
    void rebuildWindow (float shape, float skew);
    inline float readRing (const std::vector<float>& buf, double pos) const noexcept;
    float rand01() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f); }
    float randBi() noexcept { return rand01() * 2.0f - 1.0f; }

    double sr = 48000.0;
    int maxBlockSize = 512;

    // capture buffer
    std::vector<float> ringL, ringR;
    int64_t ringSize = 1;
    int64_t writeAbs = 0;

    PitchDetector pitchDetector;

    // playhead
    double playAbs = 0.0;
    int playDir = 1;
    bool lastActive = true, lastReverse = false;
    bool manualTrigger = false;
    double lastGridIndex = -1.0;
    int64_t lastLoopBoundary = -1;
    double lastLoopIndex = -1.0;
    double internalPpq = 0.0;

    // transient detection
    float envFast = 0.0f, envSlow = 0.0f;
    int transientHold = 0;

    // scheduler
    double samplesToNextGrain = 0.0;
    int curOffset = 0;
    double lastSyncIndex = -1.0;
    uint32_t grainCounter = 0;
    float lastPan = 0.0f;
    float driftPan = 0.0f;
    int arpStep = 0;
    std::array<int, numIntervals> arpOrder {};
    int arpCount = 0;

    // midi
    std::array<int, 16> heldNotes {};
    int numHeld = 0;

    // grains
    static constexpr int maxGrains = 512;
    std::vector<Grain> grains;
    std::vector<float> filterPool;
    uint32_t serialCounter = 0;

    // shared window table
    static constexpr int windowSize = 2048;
    std::vector<float> windowTable;
    float winShapeCached = 99.0f, winSkewCached = 99.0f;

    // buses
    juce::AudioBuffer<float> mainBus, tapBus;

    GrainDelay grainDelay;
    Imager imager;

    uint32_t rng = 0x1234567u;
};
}
