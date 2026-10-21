#include "PresetHelpers.h"
#include <cmath>
#include <map>

namespace liq
{
// ============================================================================
// PresetWriter
// ============================================================================
void PresetWriter::set (const juce::String& paramId, float realValue)
{
    if (auto* p = state.getParameter (paramId))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    else
        jassertfalse;
}

void PresetWriter::route (int slot, int source, const juce::String& targetId, float amount)
{
    const int dst = modTargetIds().indexOf (targetId) + 1;
    jassert (dst > 0);
    setChoice (id::mx (slot, "Src"), source);
    setChoice (id::mx (slot, "Dst"), dst);
    set (id::mx (slot, "Amt"), amount);
}

void PresetWriter::dryWet (float d)
{
    set (id::mixWet, std::min (1.0f, 2.0f * d));
    set (id::mixDry, std::min (1.0f, 2.0f * (1.0f - d)));
}

void PresetWriter::macros (const juce::String& n1, const juce::String& h1, const juce::String& n2, const juce::String& h2)
{
    state.state.setProperty ("macro1Name", n1, nullptr);
    state.state.setProperty ("macro1Hint", h1, nullptr);
    state.state.setProperty ("macro2Name", n2, nullptr);
    state.state.setProperty ("macro2Hint", h2, nullptr);
}

void PresetWriter::freeRateMs (float ms)
{
    setChoice (id::rateMode, rateFree);
    set (id::grainRate, (float) (std::log (ms / 2000.0) / std::log (0.0005)));
}

void PresetWriter::syncedRate (int mode, const char* division)
{
    setChoice (id::rateMode, mode);
    set (id::grainRate, GrainEngine::rateValueForDivision (mode, division));
}

// ============================================================================
// Factory presets
// ============================================================================
using namespace presets;

namespace
{
struct ClassicInfo { const char* source; const char* desc; };

const std::vector<FactoryPreset>& classicList()
{
    static const std::vector<FactoryPreset> list {
        { "Init", "Init", [] (PresetWriter& w) {
            // XY pad does something from the start: X = stretch & spray, Y = pitch & detune
            w.route (1, M1, id::stretch, 0.6f); w.route (2, M1, id::spray, 0.4f);
            w.route (3, M2, id::pitch, semis (7)); w.route (4, M2, id::detune, 0.5f); } },

        // ---------------------------------------------------------- Harmonic
        { "Fifth Choir", "Harmonic", [] (PresetWriter& w) {
            w.freeRateMs (35); w.set (id::sizeRatio, 3.0f);
            w.setChoice (id::chordMode, chordChord); intervals (w, { 0, 4, 7 });
            w.set (id::detune, 0.25f); w.set (id::chordSpread, 0.7f);
            w.set (id::dimension, 0.4f); w.dryWet (0.5f);
            w.route (1, M1, id::detune, 0.6f);   w.route (2, M1, id::dimension, 0.5f);
            w.route (3, M2, id::pitch, semis (7)); w.route (4, M2, id::chordSpread, 0.3f); } },

        { "Triad Halo", "Harmonic", [] (PresetWriter& w) {
            w.freeRateMs (25); w.set (id::sizeRatio, 4.0f); w.set (id::winShape, 0.3f);
            w.setChoice (id::chordMode, chordChord); intervals (w, { 0, 2, 4 });
            w.set (id::spray, 0.15f); w.set (id::blur, 0.3f);
            fx (w, 4, fxReverb, 0.35f, 0.7f, 0.5f, 1.0f, 0.1f);
            w.route (1, M1, id::blur, 0.6f); w.route (2, M1, id::spray, 0.4f);
            w.route (3, M2, id::fx (4, "Mix"), 0.5f); } },

        { "Octave Stack", "Harmonic", [] (PresetWriter& w) {
            w.freeRateMs (30); w.set (id::sizeRatio, 3.0f);
            w.setChoice (id::chordMode, chordChord); intervals (w, { -7, 0, 7 });
            w.set (id::chordSpread, 1.0f); w.dryWet (0.6f);
            w.route (1, M1, id::chordSpread, -0.6f); w.route (2, M2, id::pitch, semis (12)); } },

        // ---------------------------------------------------------- Rhythmic
        { "Arp Rain", "Rhythmic", [] (PresetWriter& w) {
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 1.2f);
            w.setChoice (id::chordMode, chordArp); intervals (w, { 0, 2, 4, 7 });
            w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true);
            w.setChoice (id::dlyDiv, defaultDivisionIndex ("1/8")); w.set (id::dlyTaps, 2); w.set (id::dlyFeedback, 0.35f);
            w.route (1, M1, id::arpRand, 1.0f); w.route (2, M2, id::dlyFeedback, 0.5f); w.route (3, M2, id::dlyMix, 0.4f); } },

        { "Gated Sixteenths", "Rhythmic", [] (PresetWriter& w) {
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 0.5f); w.set (id::winShape, 0.7f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fLowpass); w.set (id::fltCutoff, 3000.0f); w.set (id::fltReso, 0.4f);
            w.dryWet (0.8f);
            w.route (1, M1, id::sizeRatio, 0.3f); w.route (2, M2, id::fltCutoff, -0.4f); } },

        { "Triplet Bounce", "Rhythmic", [] (PresetWriter& w) {
            w.syncedRate (rateTriplet, "1/8T"); w.set (id::sizeRatio, 0.9f);
            w.setChoice (id::spreadMode, spAlternate); w.set (id::spread, 1.0f);
            w.setOn (id::dlyOn, true); w.setChoice (id::dlyDiv, defaultDivisionIndex ("1/4T")); w.set (id::dlyTaps, 2);
            w.setChoice (id::dlyPattern, shAlternate); w.set (id::dlyShift, 2.0f);
            w.route (1, M1, id::revProb, 0.6f); w.route (2, M2, id::dlyMix, 0.5f); } },

        { "Dotted Echo Grains", "Rhythmic", [] (PresetWriter& w) {
            w.syncedRate (rateDotted, "1/8D"); w.set (id::sizeRatio, 1.0f);
            w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true); w.setChoice (id::dlyDiv, defaultDivisionIndex ("1/8D"));
            w.set (id::dlyTaps, 4); w.setChoice (id::dlyPattern, shRamp); w.set (id::dlyShift, 1.0f); w.set (id::dlyMix, 0.55f);
            w.route (1, M1, id::dlyShift, 0.17f); w.route (2, M2, id::dlySpace, 0.7f); } },

        // ---------------------------------------------------------- Freeze & Pads
        { "Endless Freeze", "Freeze & Pads", [] (PresetWriter& w) {
            w.set (id::stretch, 1.0f); w.setChoice (id::stretchRetrig, defaultDivisionIndex ("4 bars"));
            w.freeRateMs (18); w.setOn (id::sizeLink, false); w.set (id::grainSize, 300.0f);
            w.set (id::blur, 0.8f); w.set (id::spray, 0.2f); w.set (id::dimension, 0.6f);
            fx (w, 4, fxReverb, 0.4f, 0.8f, 0.4f, 1.0f, 0.0f);
            w.route (1, M1, id::blur, 0.2f); w.route (2, M1, id::spray, 0.4f); w.route (3, M2, id::pitch, semis (12)); } },

        { "Slow Motion 4x", "Freeze & Pads", [] (PresetWriter& w) {
            w.setOn (id::stretchSync, true); w.set (id::stretch, 0.4f);
            w.freeRateMs (40); w.set (id::sizeRatio, 3.0f); w.set (id::blur, 0.4f);
            w.route (1, M1, id::stretch, 0.4f); w.route (2, M2, id::blur, 0.6f); } },

        { "Pad Maker", "Freeze & Pads", [] (PresetWriter& w) {
            w.set (id::stretch, 0.995f); w.set (id::blur, 1.0f); w.freeRateMs (10);
            w.setOn (id::sizeLink, false); w.set (id::grainSize, 400.0f); w.set (id::spray, 0.3f);
            w.setOn (id::fltOn, true); w.set (id::fltCutoff, 3000.0f); w.set (id::fltReso, 0.1f);
            fx (w, 4, fxReverb, 0.5f, 0.85f, 0.6f, 1.0f, 0.2f); w.dryWet (0.7f);
            w.route (1, M1, id::fltCutoff, 0.35f); w.route (2, M2, id::detune, 0.5f); w.route (3, M2, id::dimension, 0.8f); } },

        { "Transient Freeze", "Freeze & Pads", [] (PresetWriter& w) {
            w.setChoice (id::stretchMode, stretchTransient); w.set (id::stretchSens, 0.6f); w.set (id::stretch, 0.9f);
            w.freeRateMs (25); w.set (id::sizeRatio, 3.0f); w.set (id::blur, 0.5f);
            w.route (1, M1, id::stretchSens, 0.4f); w.route (2, M2, id::stretch, 0.1f); } },

        // ---------------------------------------------------------- Shimmer
        { "Crystal Steps", "Shimmer", [] (PresetWriter& w) {
            w.freeRateMs (60); w.set (id::sizeRatio, 2.5f);
            w.setOn (id::dlyOn, true); w.set (id::dlyTaps, 6); w.setChoice (id::dlyPattern, shRamp); w.set (id::dlyShift, 2.0f);
            w.set (id::dlySpace, 0.7f); w.set (id::dlyFeedback, 0.5f); w.set (id::dlyMix, 0.6f); w.set (id::dlyHP, 300.0f);
            w.route (1, M1, id::dlySpace, 0.3f); w.route (2, M2, id::dlyShift, -0.33f); } },

        { "Octave Shimmer", "Shimmer", [] (PresetWriter& w) {
            w.freeRateMs (45); w.set (id::sizeRatio, 3.0f);
            w.setOn (id::dlyOn, true); w.setOn (id::dlySync, false); w.set (id::dlyTime, 180.0f); w.set (id::dlyTaps, 3);
            w.setChoice (id::dlyPattern, shHold); w.set (id::dlyShift, 7.0f);
            w.set (id::dlySpace, 0.85f); w.set (id::dlyFeedback, 0.6f); w.set (id::dlyMix, 0.65f);
            w.route (1, M1, id::dlyFeedback, 0.35f); w.route (2, M2, id::dlyLP, -0.5f); } },

        { "Cathedral Grains", "Shimmer", [] (PresetWriter& w) {
            w.freeRateMs (50); w.set (id::sizeRatio, 4.0f);
            w.setChoice (id::spreadMode, spScatter); w.set (id::spread, 1.0f); w.set (id::dimension, 0.7f);
            fx (w, 4, fxReverb, 0.6f, 0.95f, 0.3f, 1.0f, 0.3f);
            w.route (1, M1, id::fx (4, "Mix"), 0.4f); w.route (2, M2, id::spray, 0.5f); } },

        // ---------------------------------------------------------- Goa / Psy
        { "Goa Acid Grains", "Goa / Psy", [] (PresetWriter& w) {
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 0.9f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fBandpass); w.set (id::fltCutoff, 900.0f);
            w.set (id::fltReso, 0.45f); w.dryWet (0.6f); w.set (id::fltModDepth, 0.5f); w.set (id::fltModRate, 1.0f); w.set (id::fltDrive, 0.3f);
            w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true); w.setChoice (id::dlyDiv, defaultDivisionIndex ("1/8D"));
            w.set (id::dlyTaps, 2); w.set (id::dlyMix, 0.35f);
            w.route (1, M1, id::fltCutoff, 0.4f); w.route (2, M2, id::fltReso, 0.25f); w.route (3, M2, id::dlyMix, 0.4f); } },

        { "Psy Gate Trance", "Goa / Psy", [] (PresetWriter& w) {
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 0.7f); w.set (id::winShape, 0.6f);
            w.setChoice (id::stretchRetrig, defaultDivisionIndex ("1/4")); w.set (id::revProb, 0.2f);
            w.dryWet (1.0f);
            w.route (1, M1, id::revProb, 0.7f); w.route (2, M2, id::sizeRatio, -0.25f); } },

        { "Full-On Roller", "Goa / Psy", [] (PresetWriter& w) {
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 1.0f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fLowpass); w.set (id::fltCutoff, 800.0f);
            w.set (id::fltReso, 0.5f); w.set (id::fltModDepth, 0.6f); w.set (id::fltModRate, 1.0f);
            w.setOn (id::monoLows, true);
            w.route (1, M1, id::fltCutoff, 0.35f); w.route (2, M2, id::fltModDepth, 0.4f); } },

        { "Forest Spores", "Goa / Psy", [] (PresetWriter& w) {
            w.freeRateMs (40); w.setOn (id::sizeLink, false); w.set (id::grainSize, 25.0f);
            w.setChoice (id::spreadMode, spScatter); w.set (id::spread, 0.9f); w.set (id::spray, 0.6f); w.set (id::revProb, 0.5f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fModal); w.set (id::fltMorph, 0.5f); w.set (id::fltReso, 0.6f);
            w.set (id::fltCutoff, 300.0f); w.setOn (id::fltScale, true); w.setOn (id::fltRandom, true); w.set (id::fltModDepth, 0.5f);
            w.set (id::detune, 0.4f); w.dryWet (0.5f);
            w.route (1, M1, id::fltMorph, 0.5f); w.route (2, M2, id::jitter, 0.8f); } },

        { "Hi-Tech Shards", "Goa / Psy", [] (PresetWriter& w) {
            w.syncedRate (rateStraight, "1/32"); w.set (id::sizeRatio, 0.8f); w.set (id::jitter, 0.2f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fReducer); w.set (id::fltCutoff, 6000.0f); w.set (id::fltBits, 6.0f);
            w.setChoice (id::spreadMode, spHelix); w.set (id::spread, 0.8f);
            w.route (1, M1, id::fltBits, -0.3f); w.route (2, M2, id::jitter, 0.6f); } },

        { "Comb Resonator", "Goa / Psy", [] (PresetWriter& w) {
            w.freeRateMs (30); w.set (id::sizeRatio, 2.0f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fComb); w.setOn (id::fltScale, true);
            w.set (id::fltCutoff, 220.0f); w.set (id::fltReso, 0.85f); w.setOn (id::fltRandom, true); w.set (id::fltModDepth, 0.5f);
            w.set (id::fltDamp, 0.4f);
            w.route (1, M1, id::fltReso, 0.15f); w.route (2, M2, id::fltCutoff, 0.3f); } },

        // ---------------------------------------------------------- Transform
        { "Tape Stop Dreams", "Transform", [] (PresetWriter& w) {
            w.setChoice (id::pitchMode, pitchRepitch); w.set (id::pitch, -12.0f); w.set (id::stretch, 0.6f);
            w.freeRateMs (30); w.set (id::sizeRatio, 3.0f);
            w.route (1, M1, id::pitch, semis (12)); w.route (2, M2, id::stretch, 0.3f); } },

        { "Reverse Wash", "Transform", [] (PresetWriter& w) {
            w.setOn (id::reverse, true); w.setOn (id::sizeLink, false); w.set (id::grainSize, 500.0f); w.freeRateMs (60);
            w.setOn (id::dlyOn, true); w.set (id::dlySpace, 0.6f); w.set (id::dlyMix, 0.5f);
            fx (w, 4, fxReverb, 0.4f, 0.8f, 0.5f, 1.0f, 0.0f);
            w.route (1, M1, id::grainSize, 0.2f); w.route (2, M2, id::dlySpace, 0.4f); } },

        { "Vowel Talker", "Transform", [] (PresetWriter& w) {
            w.freeRateMs (20); w.set (id::sizeRatio, 2.0f);
            w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fVowel); w.set (id::fltReso, 0.5f); w.set (id::fltCutoff, 900.0f); w.dryWet (0.55f);
            w.setChoice (id::mod (1, "Type"), modLfo); w.setOn (id::mod (1, "Sync"), true);
            w.setChoice (id::mod (1, "Div"), defaultDivisionIndex ("1 bar")); w.set (id::mod (1, "Shape"), 0.25f);
            w.route (1, Mod1, id::fltMorph, 1.0f); w.route (2, M1, id::fltCutoff, 0.3f); w.route (3, M2, id::fltReso, 0.4f); } },

        { "Granular Oscillator", "Transform", [] (PresetWriter& w) {
            w.setChoice (id::rateMode, rateScale); w.set (id::grainRate, 0.4f); w.set (id::sizeRatio, 2.0f); w.set (id::winShape, 0.3f);
            w.setOn (id::fltOn, true); w.set (id::fltCutoff, 4000.0f);
            w.route (1, M1, id::grainRate, 0.2f); w.route (2, M2, id::winShape, -0.8f); } },

        { "MIDI Played Grains", "Transform", [] (PresetWriter& w) {
            w.setChoice (id::rateMode, rateMidi); w.set (id::grainRate, 0.5f); w.set (id::sizeRatio, 3.0f);
            w.setOn (id::fltOn, true); w.set (id::fltCutoff, 5000.0f); w.dryWet (1.0f);
            w.route (1, M1, id::sizeRatio, 0.2f); w.route (2, M2, id::fltCutoff, -0.3f); } },

        // ---------------------------------------------------------- Glitch / Scrub
        { "Stutter Scrub", "Glitch", [] (PresetWriter& w) {
            w.setChoice (id::playMode, playScrub); w.setChoice (id::scrubMode, 2); w.setChoice (id::loopLength, 2);
            w.syncedRate (rateStraight, "1/32"); w.set (id::sizeRatio, 1.5f);
            w.setChoice (id::mod (3, "Type"), modSequencer); w.setChoice (id::mod (3, "Div"), defaultDivisionIndex ("1 bar"));
            w.set (id::mod (3, "Steps"), 16);
            w.route (1, Mod3, id::scrubPos, 1.0f); w.route (2, M1, id::spray, 0.3f); w.route (3, M2, id::revProb, 0.6f); } },

        { "Scrub Sequencer", "Glitch", [] (PresetWriter& w) {
            w.setChoice (id::playMode, playScrub); w.setChoice (id::scrubMode, 3); w.setChoice (id::loopLength, 3);
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 1.0f);
            w.setChoice (id::mod (3, "Type"), modSequencer); w.setChoice (id::mod (3, "Div"), defaultDivisionIndex ("2 bars"));
            w.set (id::mod (3, "Steps"), 8);
            w.route (1, Mod3, id::scrubPos, 1.0f); w.route (2, M1, id::pitch, semis (12)); w.route (3, M2, id::jitter, 0.5f); } },

        { "Random Slices", "Glitch", [] (PresetWriter& w) {
            w.setChoice (id::playMode, playScrub); w.setChoice (id::scrubMode, 3);
            w.syncedRate (rateStraight, "1/16"); w.set (id::sizeRatio, 0.8f); w.set (id::revProb, 0.3f);
            w.setChoice (id::mod (2, "Type"), modLfo); w.set (id::mod (2, "Shape"), 1.0f);
            w.setChoice (id::mod (2, "Div"), defaultDivisionIndex ("1/8"));
            w.route (1, Mod2, id::scrubPos, 1.0f); w.route (2, M1, id::revProb, 0.7f); w.route (3, M2, id::pitch, semis (-12)); } },

        // ---------------------------------------------------------- Vocal
        { "Vocal Harmonizer", "Vocal", [] (PresetWriter& w) {
            w.setChoice (id::chordMode, chordChord); intervals (w, { 0, 2, 4 }); w.setChoice (id::retune, retuneHard);
            w.setOn (id::sizeLink, false); w.set (id::grainSize, 80.0f); w.freeRateMs (30);
            w.set (id::detune, 0.1f); w.set (id::chordSpread, 0.8f); w.dryWet (0.5f);
            w.route (1, M1, id::chordSpread, 0.2f); w.route (2, M2, id::detune, 0.4f); } },

        { "Hard-Tuned Grains", "Vocal", [] (PresetWriter& w) {
            w.setChoice (id::retune, retuneHard); w.setOn (id::sizeLink, false); w.set (id::grainSize, 60.0f); w.freeRateMs (22);
            w.dryWet (1.0f);
            w.route (1, M1, id::pitch, semis (12)); w.route (2, M2, id::detune, 0.6f); } },

        { "Formant Up", "Vocal", [] (PresetWriter& w) {
            w.setOn (id::formOn, true); w.set (id::formShift, 5.0f);
            w.route (1, M1, id::formShift, 0.3f); } },

        { "Formant Down", "Vocal", [] (PresetWriter& w) {
            w.setOn (id::formOn, true); w.set (id::formShift, -5.0f);
            w.route (1, M1, id::formShift, -0.3f); } },

        { "Wide Ensemble", "Vocal", [] (PresetWriter& w) {
            w.set (id::detune, 0.6f); w.freeRateMs (30); w.set (id::sizeRatio, 3.0f);
            w.set (id::dimension, 0.8f); w.set (id::width, 1.5f); w.setChoice (id::spreadMode, spBloom); w.set (id::spread, 0.7f);
            w.dryWet (0.5f);
            w.route (1, M1, id::detune, 0.4f); w.route (2, M2, id::width, 0.25f); } },

        // ---------------------------------------------------------- Morph
        { "Morph Pad Blend", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlPad);
            w.setChoice (id::morphAlgo, morphBlend); w.set (id::morphX, 0.5f); w.set (id::morphY, 1.0f);
            w.route (1, M1, id::mdlTone, 0.5f); w.route (2, M2, id::morphGamma, 0.3f); } },

        { "Vocoder Pad", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlPad);
            w.setChoice (id::morphAlgo, morphCarve); w.set (id::morphX, 0.0f); w.set (id::morphDetail, 0.75f);
            w.set (id::mdlTone, 0.75f);
            w.route (1, M1, id::morphDetail, -0.5f); w.route (2, M2, id::mdlTone, 0.3f); } },

        { "Vowel Imprint", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlVowel);
            w.setChoice (id::morphAlgo, morphImprint); w.set (id::morphX, 0.75f); w.set (id::mdlMotion, 0.6f);
            w.route (1, M1, id::mdlMotion, 0.4f); w.route (2, M2, id::morphX, 0.25f); } },

        { "Tonal Kick", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlDrone);
            w.set (id::mdlOctave, -1.0f); w.setChoice (id::morphAlgo, morphBlend); w.set (id::morphX, 0.45f);
            w.setOn (id::trOn, true); w.set (id::trLength, 25.0f); w.set (id::trAmount, 1.0f);
            w.route (1, M1, id::morphX, 0.3f); w.route (2, M2, id::trLength, 0.4f); } },

        { "Psy Bass Imprint", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlPsyBass);
            w.set (id::mdlOctave, -1.0f); w.setChoice (id::morphAlgo, morphCarve); w.set (id::morphX, 0.35f);
            w.set (id::morphDetail, 0.3f); w.setOn (id::trOn, true);
            w.route (1, M1, id::mdlTone, 0.5f); w.route (2, M2, id::morphX, 0.4f); } },

        { "Noise Weave", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlNoise);
            w.setChoice (id::morphAlgo, morphWeave); w.set (id::morphX, 0.45f); w.set (id::mdlMotion, 0.7f);
            w.route (1, M1, id::morphX, 0.4f); w.route (2, M2, id::mdlTone, 0.5f); } },

        { "Sidechain Spectral Duck", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcAux);
            w.setChoice (id::morphAlgo, morphDuck); w.set (id::morphX, 0.8f); w.set (id::morphDetail, 0.4f);
            w.route (1, M1, id::morphX, 0.2f); } },

        { "Sidechain Blend", "Morph", [] (PresetWriter& w) {
            w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcAux);
            w.setChoice (id::morphAlgo, morphBlend); w.set (id::morphX, 0.5f);
            w.route (1, M1, id::morphX, 0.5f); w.route (2, M2, id::morphGamma, 0.4f); } },
    };
    return list;
}

// source tag + one-line description for the sound-design presets
ClassicInfo classicInfo (const juce::String& n)
{
    static const std::map<juce::String, ClassicInfo> m {
        { "Init", { "Alle", "Neutraler Start: klingt wie das Original. Pad links/rechts = Stretch, oben/unten = Tonhöhe." } },
        { "Fifth Choir", { "Melodie", "Jede Note wird zum Dur-Akkord (Grundton, Terz, Quinte) - breiter Chor." } },
        { "Triad Halo", { "Synth", "Weiche Akkord-Wolke mit Hall, Grains verschwimmen." } },
        { "Octave Stack", { "Melodie", "Oktave tiefer und Quinte darüber, weit im Stereo." } },
        { "Arp Rain", { "Synth", "Macht aus jedem Ton ein 1/16-Arpeggio in deiner Tonart." } },
        { "Gated Sixteenths", { "Synth", "Zerhackt den Klang im 1/16-Raster, Filter macht ihn rund." } },
        { "Triplet Bounce", { "Drums", "Triolische Grains springen links/rechts, Delay mit Tonhöhen-Wechsel." } },
        { "Dotted Echo Grains", { "Synth", "Punktierte Echos, jede Wiederholung einen Ton höher." } },
        { "Endless Freeze", { "Pad/FX", "Friert den Klang zu einer endlosen Fläche ein." } },
        { "Slow Motion 4x", { "Pad/FX", "Spielt alles viermal langsamer ab, ohne Tonhöhe zu ändern." } },
        { "Pad Maker", { "Pad/FX", "Macht aus jedem Sound ein weiches, gefiltertes Pad." } },
        { "Transient Freeze", { "Drums", "Friert bei jedem Schlag neu ein - rhythmische Flächen." } },
        { "Crystal Steps", { "Synth", "Glitzernde Echos, die stufenweise ansteigen." } },
        { "Octave Shimmer", { "Gitarre", "Shimmer-Hall: Echos eine Quinte höher, sehr weit." } },
        { "Cathedral Grains", { "Pad/FX", "Grains verstreut im Raum, grosser Hall." } },
        { "Goa Acid Grains", { "Synth", "1/16-Grains mit Acid-Bandpass und Ping-Pong-Delay." } },
        { "Psy Gate Trance", { "Synth", "Hartes Trance-Gate, einzelne Grains rückwärts." } },
        { "Full-On Roller", { "Bass", "Rollender Filter pro Grain - für Bass und Leads." } },
        { "Forest Spores", { "Pad/FX", "Kleine metallische Grains im Raum - Forest-Atmosphäre." } },
        { "Hi-Tech Shards", { "Drums", "1/32-Splitter mit Bit-Crusher." } },
        { "Comb Resonator", { "Drums", "Kammfilter in deiner Tonart: Drums werden zu Tönen." } },
        { "Tape Stop Dreams", { "Pad/FX", "Oktave tiefer und langsam wie ein bremsendes Band." } },
        { "Reverse Wash", { "Gitarre", "Lange rückwärts gespielte Grains mit Hall." } },
        { "Vowel Talker", { "Synth", "Vokalfilter bewegt sich im Takt - der Sound spricht." } },
        { "Granular Oscillator", { "Synth", "Grains so schnell, dass ein neuer Ton in deiner Tonart entsteht." } },
        { "MIDI Played Grains", { "Alle", "Spiele den Sound mit MIDI-Noten (MIDI-Spur auf Liquidation routen)." } },
        { "Stutter Scrub", { "Drums", "Ein Sequencer springt durch den Loop - Stutter-Effekt." } },
        { "Scrub Sequencer", { "Drums", "Baut aus dem Loop eine neue Abfolge im Takt." } },
        { "Random Slices", { "Drums", "Zufällige Stücke aus dem Loop, teils rückwärts." } },
        { "Vocal Harmonizer", { "Vocal", "Zweite und dritte Stimme in deiner Tonart." } },
        { "Hard-Tuned Grains", { "Vocal", "Harte Tonhöhenkorrektur auf jedes Grain." } },
        { "Formant Up", { "Vocal", "Hellere, kleinere Stimme bei gleicher Tonhöhe." } },
        { "Formant Down", { "Vocal", "Dunklere, grössere Stimme bei gleicher Tonhöhe." } },
        { "Wide Ensemble", { "Gitarre", "Verstimmte Kopien im Stereo - breiter Ensemble-Klang." } },
        { "Morph Pad Blend", { "Alle", "Verschmilzt deine Spur mit einer Akkordfläche." } },
        { "Vocoder Pad", { "Drums", "Vocoder: die Fläche spricht im Rhythmus deiner Spur." } },
        { "Vowel Imprint", { "Drums", "Deine Spur bekommt eine Vokal-Klangfarbe." } },
        { "Tonal Kick", { "Kick", "Die Kick bekommt einen Ton in deiner Tonart, der Anschlag bleibt." } },
        { "Psy Bass Imprint", { "Kick", "Kick formt einen Psy-Bass (Vocoder-Prinzip)." } },
        { "Noise Weave", { "Drums", "Rauschen und deine Spur wechseln sich pro Frequenz ab." } },
        { "Sidechain Spectral Duck", { "Bass", "Bass macht Platz, wo die Sidechain-Kick spielt - nur in deren Frequenzen." } },
        { "Sidechain Blend", { "Alle", "Mischt deine Spur mit dem Sidechain-Signal im Spektrum." } },
    };
    auto it = m.find (n);
    return it != m.end() ? it->second : ClassicInfo { "Alle", "" };
}
}

const std::vector<FactoryPreset>& PresetManager::factory()
{
    static const std::vector<FactoryPreset> all = []
    {
        std::vector<FactoryPreset> v;
        for (auto p : classicList())
        {
            const auto info = classicInfo (p.name);
            p.source = juce::String::fromUTF8 (info.source);
            p.description = juce::String::fromUTF8 (info.desc);
            p.genre = p.name == "Init" ? juce::String ("Alle") : juce::String ("Sound Design");
            if (p.category != "Init") p.category = "Sound Design / " + p.category;
            v.push_back (p);
        }
        appendKickMelodyPresets (v);
        appendGenrePresets (v);
        return v;
    }();
    return all;
}

// ============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

juce::File PresetManager::userFolder() const
{
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("JustCreateit").getChildFile ("Liquidation").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

juce::Array<juce::File> PresetManager::userPresets() const
{
    if (! userCacheValid) const_cast<PresetManager*> (this)->rescanUserPresets();
    return userCache;
}

void PresetManager::rescanUserPresets()
{
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.liqpreset");
    files.sort();
    userCache = files;
    userCacheValid = true;
}

juce::StringArray PresetManager::allNames() const
{
    juce::StringArray names;
    for (auto& p : factory()) names.add (p.name);
    for (auto& f : userPresets()) names.add (f.getFileNameWithoutExtension());
    return names;
}

juce::StringArray PresetManager::allCategories() const
{
    juce::StringArray cats;
    for (auto& p : factory()) cats.add (p.category);
    for (int i = 0; i < userPresets().size(); ++i) cats.add ("Eigene");
    return cats;
}

int PresetManager::numPresets() const { return (int) factory().size() + userPresets().size(); }

juce::String PresetManager::currentName() const
{
    if (currentNameOverride.isNotEmpty()) return currentNameOverride;
    const auto names = allNames();
    return juce::isPositiveAndBelow (current, names.size()) ? names[current] : juce::String ("Init");
}

void PresetManager::resetToDefaultsKeepingKey()
{
    static const juce::StringArray keep { id::scaleRoot, id::scaleType, id::globalTune };
    for (auto* p : state.processor.getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto pid = rp->getParameterID();
            if (keep.contains (pid) || pid.startsWith ("scaleNote")) continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    }
}

void PresetManager::load (int index)
{
    const auto& f = factory();
    if (index < 0) return;
    current = index;
    currentNameOverride = {};

    if (index < (int) f.size())
    {
        if (onBeforeFactoryLoad) onBeforeFactoryLoad();
        resetToDefaultsKeepingKey();
        for (auto* k : { "macro1Name", "macro1Hint", "macro2Name", "macro2Hint" }) state.state.removeProperty (k, nullptr);
        PresetWriter w (state);
        f[(size_t) index].apply (w);
        deriveMacroNames();
        return;
    }

    const auto users = userPresets();
    const int u = index - (int) f.size();
    if (u < users.size()) loadFile (users[u]);
}

void PresetManager::deriveMacroNames()
{
    // presets without explicit names: name the macro after what it drives
    const auto names = modTargetNames();
    for (int m = 1; m <= 2; ++m)
    {
        const juce::String key = m == 1 ? "macro1Name" : "macro2Name";
        if (state.state.getProperty (key).toString().isNotEmpty()) continue;
        juce::StringArray t;
        for (int s = 1; s <= numMatrixSlots; ++s)
        {
            const int src = (int) std::lround (state.getRawParameterValue (id::mx (s, "Src"))->load());
            const int dst = (int) std::lround (state.getRawParameterValue (id::mx (s, "Dst"))->load());
            if (src == m && dst > 0 && dst < names.size()) t.addIfNotAlreadyThere (names[dst]);
        }
        if (! t.isEmpty())
        {
            state.state.setProperty (key, t[0] + (t.size() > 1 ? " +" : ""), nullptr);
            state.state.setProperty (m == 1 ? "macro1Hint" : "macro2Hint", "Steuert: " + t.joinIntoString (", "), nullptr);
        }
    }
}

juce::String PresetManager::descriptionOf (int i) const
{
    const auto& f = factory();
    return juce::isPositiveAndBelow (i, (int) f.size()) ? f[(size_t) i].description : juce::String ("Eigenes Preset");
}

juce::String PresetManager::genreOf (int i) const
{
    const auto& f = factory();
    return juce::isPositiveAndBelow (i, (int) f.size()) ? f[(size_t) i].genre : juce::String ("Eigene");
}

juce::String PresetManager::sourceOf (int i) const
{
    const auto& f = factory();
    return juce::isPositiveAndBelow (i, (int) f.size()) ? f[(size_t) i].source : juce::String ("Alle");
}

void PresetManager::loadFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        const auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid() && onLoadState)
        {
            onLoadState (tree);
            currentNameOverride = file.getFileNameWithoutExtension();
            const auto names = allNames();
            const int i = names.indexOf (currentNameOverride);
            if (i >= 0) current = i;
        }
    }
}

bool PresetManager::saveUser (const juce::String& name, const juce::ValueTree& fullState)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto file = userFolder().getChildFile (clean + ".liqpreset");
    if (auto xml = fullState.createXml())
    {
        const bool ok = xml->writeTo (file);
        rescanUserPresets();
        if (ok)
        {
            currentNameOverride = clean;
            const int i = allNames().indexOf (clean);
            if (i >= 0) current = i;
        }
        return ok;
    }
    return false;
}
}
