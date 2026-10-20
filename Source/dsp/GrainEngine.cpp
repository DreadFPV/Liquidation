#include "GrainEngine.h"
#include <cmath>
#include <algorithm>

namespace liq
{
namespace
{
    constexpr double ringSeconds = 34.0;
    constexpr float halfPi = 1.5707963f;

    // Division indices usable by a synced rate mode, ordered slow -> fast.
    std::vector<int> divisionsForMode (int mode)
    {
        std::vector<int> idx;
        const auto& d = divisions();
        for (int i = (int) d.size() - 1; i >= 0; --i)
        {
            const int kind = d[(size_t) i].kind;
            if (mode == rateSynced || (mode == rateStraight && kind == 0)
                || (mode == rateTriplet && kind == 1) || (mode == rateDotted && kind == 2))
                idx.push_back (i);
        }
        return idx;
    }

    double divisionBeatsForRate (int mode, float v)
    {
        static const std::vector<int> lists[5] { {}, divisionsForMode (rateSynced), divisionsForMode (rateStraight),
                                                 divisionsForMode (rateTriplet), divisionsForMode (rateDotted) };
        const auto& l = lists[std::clamp (mode, 1, 4)];
        const int k = std::clamp ((int) std::lround (v * (float) (l.size() - 1)), 0, (int) l.size() - 1);
        return divisions()[(size_t) l[(size_t) k]].beats;
    }

    const char* divisionNameForRate (int mode, float v)
    {
        const double beats = divisionBeatsForRate (mode, v);
        for (auto& d : divisions()) if (std::abs (d.beats - beats) < 1e-9) return d.name;
        return "";
    }

    float freeRateMs (float v) { return 2000.0f * std::pow (0.0005f, std::clamp (v, 0.0f, 1.0f)); }
}

// ============================================================================
// static helpers
// ============================================================================
float GrainEngine::stretchFactor (float v, bool sync) noexcept
{
    // returns playback speed (1 = original, 0 = frozen)
    if (sync)
    {
        static const float f[] { 1, 1.5f, 2, 3, 4, 6, 8, 12, 16, 32, 0 };
        const int k = std::clamp ((int) std::lround (v * 10.0f), 0, 10);
        return f[k] == 0 ? 0.0f : 1.0f / f[k];
    }
    if (v >= 0.995f) return 0.0f;
    const float s = 1.0f - v;
    return s * s;
}

double GrainEngine::rateHz (const EngineParams& p, double midiHz, bool& valid) noexcept
{
    valid = true;
    switch (p.rateMode)
    {
        case rateFree:  return 1000.0 / freeRateMs (p.grainRate);
        case rateScale: return p.scale.midiToHz ((float) p.scale.noteFromNormalised (p.grainRate, 12, 108));
        case rateMidi:
        {
            if (midiHz <= 0.0) { valid = false; return 1.0; }
            const float offset = std::round ((p.grainRate - 0.5f) * 48.0f);
            return midiHz * std::pow (2.0, offset / 12.0);
        }
        default:
            return p.bpm / 60.0 / divisionBeatsForRate (p.rateMode, p.grainRate);
    }
}

juce::String GrainEngine::rateText (int mode, float v, const Scale& scale, double)
{
    switch (mode)
    {
        case rateFree:
        {
            const float ms = freeRateMs (v);
            return ms >= 100.0f ? juce::String ((int) std::lround (ms)) + " ms" : juce::String (ms, 1) + " ms";
        }
        case rateScale:
        {
            const int note = scale.noteFromNormalised (v, 12, 108);
            return noteNames()[note % 12] + juce::String (note / 12 - 1);
        }
        case rateMidi:
        {
            const int off = (int) std::lround ((v - 0.5f) * 48.0f);
            return (off > 0 ? "+" : "") + juce::String (off) + " st";
        }
        default: return divisionNameForRate (mode, v);
    }
}

float GrainEngine::rateValueForDivision (int mode, const char* name)
{
    const auto l = divisionsForMode (std::clamp (mode, (int) rateSynced, (int) rateDotted));
    for (size_t k = 0; k < l.size(); ++k)
        if (juce::String (divisions()[(size_t) l[k]].name) == name)
            return l.size() > 1 ? (float) k / (float) (l.size() - 1) : 0.0f;
    return 0.5f;
}

// ============================================================================
GrainEngine::GrainEngine()
{
    heldNotes.fill (-1);
}

void GrainEngine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    maxBlockSize = maxBlock;
    ringSize = (int64_t) (sr * ringSeconds);
    ringL.assign ((size_t) ringSize, 0.0f);
    ringR.assign ((size_t) ringSize, 0.0f);
    pitchDetector.prepare (sr, ringSize);

    grains.assign ((size_t) maxGrains, Grain {});
    for (int i = 0; i < maxGrains; ++i) grains[(size_t) i].poolIndex = i;
    filterPool.assign ((size_t) maxGrains * 2 * GrainFilter::maxDelay, 0.0f);

    windowTable.assign (windowSize + 2, 0.0f);
    winShapeCached = winSkewCached = 99.0f;
    rebuildWindow (0.0f, 0.0f);

    mainBus.setSize (2, maxBlock, false, true, true);
    tapBus.setSize (2, maxBlock, false, true, true);

    grainDelay.prepare (sr, maxBlock);
    imager.prepare (sr);
    reset();
}

void GrainEngine::reset()
{
    std::fill (ringL.begin(), ringL.end(), 0.0f);
    std::fill (ringR.begin(), ringR.end(), 0.0f);
    writeAbs = 0;
    playAbs = 0.0;
    playDir = 1;
    pitchDetector.reset();
    for (auto& g : grains) g.active = false;
    samplesToNextGrain = 0.0;
    lastSyncIndex = lastGridIndex = lastLoopIndex = -1.0;
    lastLoopBoundary = -1;
    envFast = envSlow = 0.0f;
    transientHold = 0;
    arpStep = 0; arpCount = 0;
    grainDelay.reset();
    imager.reset();
}

void GrainEngine::noteOn (int note) noexcept
{
    noteOff (note);
    if (numHeld < (int) heldNotes.size())
        heldNotes[(size_t) numHeld++] = note;
}

void GrainEngine::noteOff (int note) noexcept
{
    for (int i = 0; i < numHeld; ++i)
        if (heldNotes[(size_t) i] == note)
        {
            for (int k = i; k < numHeld - 1; ++k) heldNotes[(size_t) k] = heldNotes[(size_t) k + 1];
            --numHeld;
            return;
        }
}

void GrainEngine::rebuildWindow (float shape, float skew)
{
    if (std::abs (shape - winShapeCached) < 0.005f && std::abs (skew - winSkewCached) < 0.005f) return;
    winShapeCached = shape; winSkewCached = skew;

    const float a = std::clamp (0.5f + 0.45f * skew, 0.05f, 0.95f);   // peak position
    for (int i = 0; i <= windowSize; ++i)
    {
        const float t = (float) i / (float) windowSize;
        float x = t < a ? t / a : (1.0f - t) / (1.0f - a);              // 0 at edges, 1 at peak
        x = std::clamp (x, 0.0f, 1.0f);
        float y;
        if (shape >= 0.0f)
        {
            const float stretched = std::min (1.0f, x * (1.0f + shape * 7.0f));
            const float s = std::sin (halfPi * stretched);
            y = s * s;
        }
        else
        {
            const float hann = std::sin (halfPi * x);
            const float tri = std::pow (x, 1.0f + (-shape) * 2.0f);
            y = hann * hann * (1.0f + shape) + tri * (-shape);
        }
        windowTable[(size_t) i] = y;
    }
    windowTable[(size_t) windowSize + 1] = 0.0f;
}

inline float GrainEngine::readRing (const std::vector<float>& buf, double pos) const noexcept
{
    if (pos < 1.0 || pos > (double) writeAbs - 2.0 || pos < (double) (writeAbs - ringSize + 4)) return 0.0f;
    const int64_t i1 = (int64_t) pos;
    const float t = (float) (pos - (double) i1);
    auto at = [&] (int64_t k) { return buf[(size_t) (((k % ringSize) + ringSize) % ringSize)]; };
    const float y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
    // 4-point Hermite
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

// ============================================================================
// pitch helpers
// ============================================================================
float GrainEngine::semitonesFor (const EngineParams& p, double readPos, int deg) noexcept
{
    const Scale& sc = p.scale;
    if (p.pitchMode != pitchScale)
        return p.pitch + (float) (sc.chromatic ? deg : sc.degreeToSemis (deg));

    const int totalDeg = (int) std::lround (p.pitch) + deg;
    // the newest analysis lags the write head; never look up beyond it
    const double lookup = std::min (readPos, (double) (writeAbs - pitchDetector.latency()));
    float m = pitchDetector.pitchAt ((int64_t) lookup);
    if (m < 0.0f)
        return (float) (sc.chromatic ? totalDeg : sc.degreeToSemis (totalDeg));   // fall back to root

    m -= 12.0f * std::log2 (sc.a4 / 440.0f);   // detector assumes A4 = 440
    display.detectedPitch.store (m, std::memory_order_relaxed);
    const int sn = sc.nearest (m);
    const int target = sc.shiftDegrees (sn, totalDeg);
    switch (p.retune)
    {
        case retuneHard:   return (float) target - m;
        case retuneSmooth: return (float) (target - sn) + ((float) sn - m) * 0.65f;
        default:           return (float) (target - sn);
    }
}

float GrainEngine::shiftForTap (const EngineParams& p, int k) noexcept
{
    const bool degrees = p.pitchMode == pitchScale && ! p.scale.chromatic;
    const float s = p.dlyShift;
    float v = 0.0f;
    switch (p.dlyPattern)
    {
        case shHold:      v = s; break;
        case shAlternate: v = (k % 2 == 1) ? s : -s; break;
        case shRamp:      v = s * (float) k; break;
        case shPulse:     v = (k % 2 == 1) ? s * (float) ((k + 1) / 2) : 0.0f; break;
        case shTriangle:  { static const int tri[8] { 1, 2, 1, 0, -1, -2, -1, 0 }; v = s * (float) tri[(k - 1) % 8]; break; }
        case shDiverge:   v = (k % 2 == 1) ? s * (float) ((k + 1) / 2) : -s * (float) (k / 2); break;
        case shRandom:    v = s * rand01(); break;
        case shChaos:     v = randBi() * (degrees ? 14.0f : 24.0f); break;
        default: break;
    }
    if (degrees) return (float) p.scale.degreeToSemis ((int) std::lround (v));
    return v;
}

float GrainEngine::nextPanPattern (const EngineParams& p) noexcept
{
    const uint32_t c = grainCounter;
    switch (p.spreadMode)
    {
        case spAlternate: return (c & 1) ? 1.0f : -1.0f;
        case spPendulum:
        {
            const float ph = (float) (c % 16) / 16.0f;
            return ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;
        }
        case spHelix:
        {
            const float s = std::sin ((float) (c / 2) * 0.55f);
            return (c & 1) ? s : -s;
        }
        case spBloom:
        {
            const double g = std::fmod ((double) c * 0.6180339887, 1.0);
            return (float) g * 2.0f - 1.0f;
        }
        case spScatter: return randBi();
        case spDrift:
        default:
        {
            driftPan += randBi() * 0.25f;
            if (driftPan > 1.0f)  driftPan = 2.0f - driftPan;
            if (driftPan < -1.0f) driftPan = -2.0f - driftPan;
            return driftPan;
        }
    }
}

// ============================================================================
// grains
// ============================================================================
GrainEngine::Grain* GrainEngine::allocateGrain()
{
    Grain* oldest = nullptr;
    for (auto& g : grains)
    {
        if (! g.active) return &g;
        if (oldest == nullptr || g.serial < oldest->serial) oldest = &g;
    }
    return oldest;   // steal
}

void GrainEngine::startGrain (const SpawnInfo& s, const EngineParams& p)
{
    Grain* g = allocateGrain();
    if (g == nullptr) return;

    g->active = true;
    g->delay = s.delay;
    g->length = std::max (16, s.length);
    g->age = 0;
    g->gain = s.gain;
    g->panStart = s.prevPan;
    g->panEnd = s.pan;
    g->isTap = s.isTap;
    g->swapLR = s.swapLR;
    g->reversed = s.reversed;
    g->serial = ++serialCounter;

    const double span = (double) g->length * s.ratio;
    const bool scrub = p.playMode == playScrub;

    if (scrub && lastLoopBoundary >= 0)
    {
        g->wrap = true;
        const double L = loopLengthBeats (p.loopLength) * 60.0 / p.bpm * sr;
        g->wrapLen = std::max<int64_t> (64, (int64_t) std::min (L, (double) ringSize * 0.9));
        g->wrapStart = lastLoopBoundary - g->wrapLen;
        if (g->wrapStart < 0) g->wrapStart = 0;
        g->pos = s.reversed ? s.startPos + span : s.startPos;
    }
    else
    {
        g->wrap = false;
        // never read ahead of the write head (account for the start delay)
        const double headAtStart = (double) writeAbs + (double) (s.delay - curOffset);
        double start = s.startPos;
        if (s.reversed) start = std::min (start, headAtStart - span - 4.0);
        else            start = std::min (start, headAtStart - std::max (0.0, span - (double) g->length) - 4.0);
        g->pos = s.reversed ? start + span : start;
    }
    g->inc = s.reversed ? -s.ratio : s.ratio;

    g->filtered = p.fltOn;
    if (g->filtered)
    {
        float* mem = filterPool.data() + (size_t) g->poolIndex * 2 * GrainFilter::maxDelay;
        g->filter.start (p.filter, p.scale, sr, mem, mem + GrainFilter::maxDelay, rng);
    }
}

void GrainEngine::spawnGrains (const EngineParams& p, int offset)
{
    ++grainCounter;

    // ---- read position
    const bool scrub = p.playMode == playScrub;
    double base = playAbs;
    const double regionLen = scrub ? loopLengthBeats (p.loopLength) * 60.0 / p.bpm * sr : sr * 2.0;
    const double sprayRange = scrub ? p.spray * regionLen * 0.5 : (double) (p.spray * p.spray) * sr;

    if (sprayRange > 1.0)
        base += scrub ? randBi() * sprayRange : -rand01() * sprayRange;

    // ---- size
    bool rateValid = true;
    const double midiHz = numHeld > 0 ? p.scale.midiToHz ((float) heldNotes[(size_t) numHeld - 1]) : 0.0;
    const double hz = rateHz (p, midiHz, rateValid);
    const double period = sr / std::max (0.05, hz);
    double len = p.sizeLink ? p.sizeRatio * period : p.sizeMs * 0.001 * sr;
    len = std::clamp (len, 16.0, sr * 4.0);

    if (p.blur > 0.001f)
    {
        base += randBi() * p.blur * len * 0.5;
        len *= 1.0 + randBi() * p.blur * 0.25;
    }

    // ---- voices (chord / arp)
    int voices[numIntervals]; int nv = 0;
    int active[numIntervals]; int na = 0;
    for (int i = 0; i < numIntervals; ++i) if (p.intervalOn[(size_t) i]) active[na++] = i;

    if (p.chordMode == chordOff || na == 0)
    {
        voices[nv++] = -1;
    }
    else if (p.chordMode == chordArp)
    {
        if (arpStep >= arpCount || arpCount != na)
        {
            arpCount = na;
            for (int i = 0; i < na; ++i) arpOrder[(size_t) i] = active[i];
            for (int i = 0; i < na; ++i)
                if (rand01() < p.arpRand)
                    std::swap (arpOrder[(size_t) i], arpOrder[(size_t) std::min (na - 1, (int) (rand01() * (float) na))]);
            arpStep = 0;
        }
        voices[nv++] = arpOrder[(size_t) arpStep++];
    }
    else
    {
        for (int i = 0; i < na; ++i) voices[nv++] = active[i];
    }

    // ---- pan
    float pan = 0.0f;
    if (p.imgOn) pan = std::clamp (p.pan + p.spread * nextPanPattern (p), -1.0f, 1.0f);
    const float prevPan = p.smoothSpread ? lastPan : pan;
    lastPan = pan;

    // overlap compensation: Hann windows at overlap o sum to o/2
    const double overlap = len / period;
    const float overlapGain = overlap > 2.0 ? (float) std::pow (2.0 / overlap, 0.75) : 1.0f;
    const float level = juce::Decibels::decibelsToGain (p.levelDb) / std::sqrt ((float) nv) * overlapGain;

    // tap spacing
    const double spb = 60.0 / p.bpm * sr;
    const double tapSpacing = p.dlySync ? divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, p.dlyDiv)].beats * spb
                                        : p.dlyTimeMs * 0.001 * sr;

    for (int v = 0; v < nv; ++v)
    {
        const int deg = voices[v] < 0 ? 0 : p.intervals[(size_t) voices[v]];
        float semis = semitonesFor (p, base + len * 0.5, deg);
        semis += randBi() * p.detune;

        const bool reversed = p.reverse ? true : (rand01() < p.revProb);
        if (v == 0) display.lastReverse.store (reversed ? 1 : 0, std::memory_order_relaxed);

        // chord stereo spread: alternate voices L/R, first voice centred for odd counts
        float voicePan = pan;
        if (nv > 1)
        {
            const int k = (nv % 2 == 1) ? v : v + 1;
            if (k > 0)
            {
                const float side = (k % 2 == 1) ? -1.0f : 1.0f;
                const float amount = (float) ((k + 1) / 2) / (float) std::max (1, nv / 2);
                voicePan = std::clamp (pan + side * amount * p.chordSpread, -1.0f, 1.0f);
            }
        }

        SpawnInfo s;
        s.startPos = base;
        s.ratio = std::pow (2.0, (double) semis / 12.0);
        s.reversed = reversed;
        s.gain = level;
        s.pan = voicePan;
        s.prevPan = (nv > 1) ? voicePan : prevPan;
        s.length = (int) len;
        s.delay = offset;
        s.isTap = false;
        s.swapLR = false;
        startGrain (s, p);

        if (p.dlyOn)
        {
            for (int k = 1; k <= p.dlyTaps; ++k)
            {
                SpawnInfo t = s;
                t.isTap = true;
                t.delay = offset + (int) std::lround (tapSpacing * k);
                t.ratio = s.ratio * std::pow (2.0, (double) shiftForTap (p, k) / 12.0);
                t.swapLR = p.dlySwap && (k % 2 == 1);
                if (p.dlyPingPong) { t.pan = (k % 2 == 1) ? -1.0f : 1.0f; t.prevPan = t.pan; }
                // taps replay the same audio the original grain read, later in time
                startGrain (t, p);
            }
        }
    }
}

// ============================================================================
// main process
// ============================================================================
void GrainEngine::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                           const EngineParams& p)
{
    jassert (n <= maxBlockSize);
    rebuildWindow (p.winShape, p.winSkew);

    const double spb = 60.0 / std::max (20.0, p.bpm) * sr;   // samples per beat
    const double beatsPerSample = 1.0 / spb;
    const bool scrub = p.playMode == playScrub;
    const bool writing = ! (scrub && p.recHold);

    display.recording.store (writing, std::memory_order_relaxed);
    display.isScrub.store (scrub, std::memory_order_relaxed);

    // rate for free-running modes
    bool rateValid = true;
    const double midiHz = numHeld > 0 ? p.scale.midiToHz ((float) heldNotes[(size_t) numHeld - 1]) : 0.0;
    const double hz = rateHz (p, midiHz, rateValid);
    const double period = sr / std::max (0.05, hz);
    const bool syncedRate = p.rateMode >= rateSynced && p.rateMode <= rateDotted;
    const double rateBeats = syncedRate ? divisionBeatsForRate (p.rateMode, p.grainRate) : 0.0;
    if (samplesToNextGrain > period * 1.5) samplesToNextGrain = period;

    // stretch speed
    float speed = stretchFactor (p.stretch, p.stretchSync);
    if (p.pitchMode == pitchRepitch) speed *= std::pow (2.0f, p.pitch / 12.0f);
    display.stretchedBpm.store ((float) (p.bpm * stretchFactor (p.stretch, p.stretchSync)), std::memory_order_relaxed);

    // stretch-mode trigger edges (manual)
    bool trigger = false;
    if (p.stretchActive && ! lastActive) trigger = true;
    if (manualTrigger) { trigger = true; manualTrigger = false; }
    if (p.reverse != lastReverse && p.stretchMode == stretchManual && p.reverse) trigger = true;
    lastActive = p.stretchActive;
    lastReverse = p.reverse;
    playDir = p.reverse ? -1 : 1;

    const double retrigBeats = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, p.stretchRetrig)].beats;
    const double loopBeats = loopLengthBeats (p.loopLength);
    const double loopSamples = std::min (loopBeats * spb, (double) ringSize * 0.9);

    const float fastA = 1.0f - std::exp (-1.0f / (0.0005f * (float) sr));
    const float fastR = 1.0f - std::exp (-1.0f / (0.015f * (float) sr));
    const float slowA = 1.0f - std::exp (-1.0f / (0.030f * (float) sr));
    const float slowR = 1.0f - std::exp (-1.0f / (0.150f * (float) sr));
    const float onsetRatio = 1.3f + (1.0f - p.stretchSens) * 3.5f;
    const float onsetFloor = juce::Decibels::decibelsToGain (-55.0f + (1.0f - p.stretchSens) * 35.0f);

    float peak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const double ppq = p.ppq + i * beatsPerSample;
        const float mono = 0.5f * (inL[i] + inR[i]);
        peak = std::max (peak, std::abs (mono));

        // ---- record
        if (writing)
        {
            const size_t w = (size_t) (writeAbs % ringSize);
            ringL[w] = inL[i];
            ringR[w] = inR[i];
            pitchDetector.push (mono, writeAbs);
            ++writeAbs;
        }

        // ---- transient detector
        {
            const float a = std::abs (mono);
            envFast += (a > envFast ? fastA : fastR) * (a - envFast);
            envSlow += (a > envSlow ? slowA : slowR) * (a - envSlow);
        }
        bool onset = false;
        if (transientHold > 0) --transientHold;
        else if (envFast > envSlow * onsetRatio && envFast > onsetFloor)
        {
            onset = true;
            transientHold = (int) (0.06 * sr);
        }

        // ---- playhead
        if (! scrub)
        {
            bool retrig = (i == 0 && trigger);
            if (p.stretchMode == stretchGrid)
            {
                const double gi = std::floor (ppq / retrigBeats);
                if (gi != lastGridIndex) { retrig = true; lastGridIndex = gi; }
            }
            else if (p.stretchMode == stretchTransient && onset)
                retrig = true;

            if (! p.stretchActive)
            {
                playAbs = (double) writeAbs;
            }
            else
            {
                if (retrig)
                {
                    playAbs = (double) writeAbs;
                    // remember marker for the display
                    const int nm = display.numMarkers.load (std::memory_order_relaxed);
                    display.markers[(size_t) (nm % EngineDisplay::maxMarkers)].store (writeAbs, std::memory_order_relaxed);
                    display.numMarkers.store (nm + 1, std::memory_order_relaxed);
                }
                else
                {
                    playAbs += playDir * speed;
                }
                playAbs = std::clamp (playAbs, (double) (writeAbs - ringSize + (int64_t) (sr * 6.0)), (double) writeAbs);
            }
        }
        else
        {
            const double li = std::floor (ppq / loopBeats);
            if (writing && (li != lastLoopIndex || lastLoopBoundary < 0))
            {
                lastLoopIndex = li;
                // align boundary with the musical loop start
                const double intoLoop = (ppq - li * loopBeats) * spb;
                lastLoopBoundary = writeAbs - (int64_t) intoLoop;
            }
            float pos = std::clamp (p.scrubPos, 0.0f, 1.0f);
            const double gridBeats = scrubModeBeats (p.scrubMode);
            if (gridBeats > 0.0)
            {
                const double steps = std::max (1.0, loopBeats / gridBeats);
                pos = (float) (std::floor (pos * steps * 0.9999) / steps);
            }
            playAbs = (double) (lastLoopBoundary - (int64_t) loopSamples) + pos * loopSamples;
        }

        // ---- scheduler
        curOffset = i;
        if (rateValid)
        {
            if (syncedRate)
            {
                const double si = std::floor (ppq / rateBeats);
                if (si != lastSyncIndex)
                {
                    lastSyncIndex = si;
                    const int jitterDelay = p.jitter > 0.0f ? (int) (rand01() * p.jitter * 0.5f * rateBeats * spb) : 0;
                    spawnGrains (p, i + jitterDelay);
                }
            }
            else
            {
                samplesToNextGrain -= 1.0;
                if (samplesToNextGrain <= 0.0)
                {
                    spawnGrains (p, i);
                    samplesToNextGrain += period * (1.0 + p.jitter * randBi() * 0.9);
                    if (samplesToNextGrain < 1.0) samplesToNextGrain = 1.0;
                }
            }
        }
    }

    display.inputLevel.store (peak, std::memory_order_relaxed);

    // ---- render grains
    mainBus.clear (0, n);
    tapBus.clear (0, n);
    float* mL = mainBus.getWritePointer (0); float* mR = mainBus.getWritePointer (1);
    float* tL = tapBus.getWritePointer (0);  float* tR = tapBus.getWritePointer (1);

    int shown = 0;
    const int64_t dispLen = (int64_t) loopSamples;
    const int64_t dispStart = scrub ? (lastLoopBoundary - (int64_t) loopSamples) : (writeAbs - dispLen);

    for (auto& g : grains)
    {
        if (! g.active) continue;
        float* bl = g.isTap ? tL : mL;
        float* br = g.isTap ? tR : mR;

        if (g.delay >= n) { g.delay -= n; continue; }

        for (int i = 0; i < n; ++i)
        {
            if (g.delay > 0) { --g.delay; continue; }

            const float phase = (float) g.age / (float) g.length;
            const float wpos = phase * (float) windowSize;
            const int wi = std::min ((int) wpos, windowSize);
            const float wf = wpos - (float) wi;
            const float w = windowTable[(size_t) wi] + (windowTable[(size_t) wi + 1] - windowTable[(size_t) wi]) * wf;

            double rp = g.pos;
            if (g.wrap)
            {
                double rel = std::fmod (rp - (double) g.wrapStart, (double) g.wrapLen);
                if (rel < 0) rel += (double) g.wrapLen;
                rp = (double) g.wrapStart + rel;
            }
            float sl = readRing (ringL, rp), sr_ = readRing (ringR, rp);
            if (g.swapLR) std::swap (sl, sr_);
            if (g.filtered) g.filter.process (sl, sr_, phase);

            // pan: narrow the grain towards mono as it moves sideways, then equal-power pan
            const float pp = g.panStart + (g.panEnd - g.panStart) * phase;
            const float mid = 0.5f * (sl + sr_);
            const float side = 0.5f * (sl - sr_) * (1.0f - std::abs (pp));
            const float l = mid + side, r = mid - side;
            const float angle = (pp + 1.0f) * 0.25f * 3.14159265f;
            const float gl = std::cos (angle) * 1.41421356f, gr = std::sin (angle) * 1.41421356f;

            const float amp = w * g.gain;
            bl[i] += l * gl * amp;
            br[i] += r * gr * amp;

            g.pos += g.inc;
            if (++g.age >= g.length) { g.active = false; break; }
        }

        // publish for the display
        if (g.active && g.delay == 0 && ! g.isTap && shown < EngineDisplay::maxGrains && dispLen > 0)
        {
            double rp = g.pos;
            if (g.wrap)
            {
                double rel = std::fmod (rp - (double) g.wrapStart, (double) g.wrapLen);
                if (rel < 0) rel += (double) g.wrapLen;
                rp = (double) g.wrapStart + rel;
            }
            const float x = (float) ((rp - (double) dispStart) / (double) dispLen);
            if (x >= 0.0f && x <= 1.0f)
            {
                const float phase = (float) g.age / (float) g.length;
                display.grainPos[(size_t) shown].store (x, std::memory_order_relaxed);
                display.grainTrail[(size_t) shown].store ((float) (-g.inc * g.age / (double) dispLen), std::memory_order_relaxed);
                display.grainAmp[(size_t) shown].store (std::sin (phase * 3.14159f), std::memory_order_relaxed);
                display.grainSide[(size_t) shown].store ((int8_t) (g.panEnd < 0 ? -1 : 1), std::memory_order_relaxed);
                ++shown;
            }
        }
    }
    display.numGrains.store (shown, std::memory_order_relaxed);
    display.regionStart.store (dispStart, std::memory_order_relaxed);
    display.regionLength.store (std::max<int64_t> (1, dispLen), std::memory_order_relaxed);
    display.playhead.store ((float) ((playAbs - (double) dispStart) / (double) std::max<int64_t> (1, dispLen)),
                            std::memory_order_relaxed);
    {
        const double regionLen = scrub ? loopSamples : sr * 2.0;
        const double sprayRange = scrub ? p.spray * regionLen * 0.5 : (double) (p.spray * p.spray) * sr;
        display.sprayLen.store ((float) (sprayRange / (double) std::max<int64_t> (1, dispLen)), std::memory_order_relaxed);
    }

    // ---- grain delay (feedback, space, mix)
    GrainDelayParams dp = p.delay;
    dp.on = p.dlyOn;
    dp.pingPong = p.dlyPingPong;
    {
        const double tapSpacing = p.dlySync ? divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, p.dlyDiv)].beats * spb
                                            : p.dlyTimeMs * 0.001 * sr;
        dp.loopSamples = tapSpacing * (double) std::max (1, p.dlyTaps);
    }
    grainDelay.process (mL, mR, tL, tR, outL, outR, n, dp);

    // ---- imager post stage
    ImagerParams ip = p.imager;
    ip.on = p.imgOn;
    ip.grainSizeMs = (float) ((p.sizeLink ? p.sizeRatio * period : p.sizeMs * 0.001 * sr) / sr * 1000.0);
    ip.grainRateHz = (float) hz;
    imager.process (outL, outR, n, ip);

    // safety
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (outL[i])) outL[i] = 0.0f;
        if (! std::isfinite (outR[i])) outR[i] = 0.0f;
        outL[i] = std::clamp (outL[i], -8.0f, 8.0f);
        outR[i] = std::clamp (outR[i], -8.0f, 8.0f);
    }
}
}
