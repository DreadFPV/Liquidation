#include "Params.h"

namespace liq
{
// ============================================================================
// Divisions
// ============================================================================
const std::vector<Division>& divisions()
{
    static const std::vector<Division> d {
        { "1/64",   0.0625,        0 },
        { "1/32T",  0.125 * 2 / 3, 1 },
        { "1/32",   0.125,         0 },
        { "1/16T",  0.25 * 2 / 3,  1 },
        { "1/32D",  0.1875,        2 },
        { "1/16",   0.25,          0 },
        { "1/8T",   0.5 * 2 / 3,   1 },
        { "1/16D",  0.375,         2 },
        { "1/8",    0.5,           0 },
        { "1/4T",   2.0 / 3,       1 },
        { "1/8D",   0.75,          2 },
        { "1/4",    1.0,           0 },
        { "1/2T",   4.0 / 3,       1 },
        { "1/4D",   1.5,           2 },
        { "1/2",    2.0,           0 },
        { "1/2D",   3.0,           2 },
        { "1 bar",  4.0,           0 },
        { "2 bars", 8.0,           0 },
        { "4 bars", 16.0,          0 },
    };
    return d;
}

juce::StringArray divisionNames()
{
    juce::StringArray s;
    for (auto& d : divisions()) s.add (d.name);
    return s;
}

int defaultDivisionIndex (const char* name)
{
    auto& d = divisions();
    for (int i = 0; i < (int) d.size(); ++i)
        if (juce::String (d[(size_t) i].name) == name) return i;
    return 0;
}

juce::StringArray rateModeNames()    { return { "Free", "Synced", "Straight", "Triplet", "Dotted", "Scale", "MIDI" }; }
juce::StringArray scaleTypeNames()
{
    return { "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
             "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic", "Blues", "Phrygian Dominant",
             "Hungarian Minor", "Double Harmonic", "Whole Tone", "Custom" };
}
juce::StringArray noteNames()        { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
juce::StringArray filterModeNames()
{
    return { "Lowpass", "Highpass", "Bandpass", "Notch", "Morph", "Vowel", "Comb", "Flange", "Modal",
             "Phaser", "Allpass", "Reducer" };
}
juce::StringArray spreadModeNames()  { return { "Alternate", "Pendulum", "Helix", "Bloom", "Scatter", "Drift" }; }
juce::StringArray shiftPatternNames(){ return { "Hold", "Alternate", "Ramp", "Pulse", "Triangle", "Diverge", "Random", "Chaos" }; }
juce::StringArray fxTypeNames()      { return { "Chorus", "Compressor", "Delay", "Distortion", "Equalizer", "Filter", "Reverb", "Shifter" }; }
juce::StringArray modTypeNames()     { return { "LFO", "Sequencer", "Envelope" }; }
juce::StringArray modSourceNames()   { return { "None", "Macro 1", "Macro 2", "Mod 1", "Mod 2", "Mod 3", "Mod 4" }; }
juce::StringArray loopLengthNames()  { return { "1 beat", "2 beats", "1 bar", "2 bars", "4 bars" }; }
double loopLengthBeats (int i)       { static const double b[] { 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 4, i)]; }
juce::StringArray scrubModeNames()   { return { "Free", "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar" }; }
juce::StringArray morphAlgoNames()   { return { "Blend", "Imprint", "Carve", "Weave", "Duck" }; }
juce::StringArray modelerModeNames() { return { "Pad", "Arp", "Drone", "Psy Bass", "Vowel", "Noise", "Melody" }; }
juce::StringArray melodyStyleNames() { return { "Goa Lead", "Rolling Arp", "Acid Line", "Offbeat Stabs", "Dark Pulse",
                                                 "Hook", "Reese Bass", "Chords", "Rolling Bass" }; }
juce::StringArray melodyTriggerNames() { return { "Grid", "Kick Step", "Kick Gate", "Between Kicks" }; }
juce::StringArray melodyLengthNames() { return { "1 bar", "2 bars", "4 bars" }; }
int melodyLengthSteps (int i) { static const int s[] { 16, 32, 64 }; return s[juce::jlimit (0, 2, i)]; }
double scrubModeBeats (int i)        { static const double b[] { 0, 0.125, 0.25, 0.5, 1, 2, 4 }; return b[juce::jlimit (0, 6, i)]; }

namespace id
{
    juce::String scaleNote (int pc)                  { return "scaleNote" + juce::String (pc); }
    juce::String interval (int i)                    { return "int" + juce::String (i); }
    juce::String intervalOn (int i)                  { return "intOn" + juce::String (i); }
    juce::String fx (int s, const char* f)           { return "fx" + juce::String (s) + f; }
    juce::String mod (int m, const char* f)          { return "mod" + juce::String (m) + f; }
    juce::String mx (int s, const char* f)           { return "mx" + juce::String (s) + f; }
    juce::String mute (int ch)                       { return "mute" + juce::String (ch); }
    juce::String solo (int ch)                       { return "solo" + juce::String (ch); }
}

// ============================================================================
// Parameter specs
// ============================================================================
namespace
{
    struct Spec
    {
        enum Kind { Float, Bool, Choice, Int } kind;
        juce::String id, name;
        juce::NormalisableRange<float> range;
        float def = 0;
        juce::StringArray choices;
        bool modulatable = false;
        juce::String unit;
    };

    juce::NormalisableRange<float> lin (float a, float b, float step = 0.0f) { return { a, b, step }; }
    juce::NormalisableRange<float> skewed (float a, float b, float centre)
    {
        juce::NormalisableRange<float> r (a, b);
        r.setSkewForCentre (centre);
        return r;
    }

    struct SpecList
    {
        std::vector<Spec> specs;

        void f (const juce::String& i, const juce::String& n, juce::NormalisableRange<float> r, float d,
                bool mod = true, const juce::String& unit = {})
        {
            specs.push_back ({ Spec::Float, i, n, r, d, {}, mod, unit });
        }
        void b (const juce::String& i, const juce::String& n, bool d)
        {
            specs.push_back ({ Spec::Bool, i, n, lin (0, 1, 1), d ? 1.0f : 0.0f, {}, false, {} });
        }
        void c (const juce::String& i, const juce::String& n, const juce::StringArray& ch, int d)
        {
            specs.push_back ({ Spec::Choice, i, n, lin (0, (float) ch.size() - 1, 1), (float) d, ch, false, {} });
        }
        void n (const juce::String& i, const juce::String& nm, int lo, int hi, int d)
        {
            specs.push_back ({ Spec::Int, i, nm, lin ((float) lo, (float) hi, 1), (float) d, {}, false, {} });
        }
    };

    const std::vector<Spec>& allSpecs()
    {
        static const std::vector<Spec> list = []
        {
            SpecList s;
            const auto divs = divisionNames();

            // ---- global
            s.c (id::routing,    "Dry/Wet Routing", { "Post Effects", "Pre Effects" }, 0);
            s.f (id::inputGain,  "Input Gain",   lin (-24, 24),        0.0f, true, "dB");
            s.f (id::outputGain, "Output Gain",  lin (-24, 24),        0.0f, true, "dB");
            s.f (id::auxGain,    "Aux Gain",     lin (-24, 24),        0.0f, true, "dB");
            s.b (id::limitOn,    "Limiter",      true);
            s.b (id::grainOn,    "Grain Engine", true);
            s.f (id::globalTune, "Global Tune",  lin (415, 466, 0.1f), 440.0f, false, "Hz");
            s.c (id::scaleRoot,  "Scale Root",   noteNames(), 9);          // A
            s.c (id::scaleType,  "Scale Type",   scaleTypeNames(), 2);     // Minor
            for (int pc = 0; pc < 12; ++pc)
                s.b (id::scaleNote (pc), "Scale Note " + noteNames()[pc], pc == 0 || pc == 2 || pc == 3 || pc == 5
                                                                          || pc == 7 || pc == 8 || pc == 10);

            // ---- grain
            s.f (id::grainRate,  "Grain Rate",   lin (0, 1),           0.45f);
            s.c (id::rateMode,   "Grain Rate Mode", rateModeNames(), 0);
            s.f (id::grainSize,  "Grain Size",   skewed (1, 2000, 120), 120.0f, true, "ms");
            s.f (id::sizeRatio,  "Grain Size Ratio", skewed (0.1f, 8, 1.5f), 2.0f, true, "x");
            s.b (id::sizeLink,   "Grain Size Link", true);
            s.f (id::winShape,   "Window Shape", lin (-1, 1),          0.0f);
            s.f (id::winSkew,    "Window Skew",  lin (-1, 1),          0.0f);
            s.f (id::jitter,     "Grain Rate Jitter", lin (0, 1),      0.0f);

            // ---- playback
            s.c (id::playMode,     "Playback Mode", { "Stretch", "Scrub" }, 0);
            s.c (id::stretchMode,  "Stretch Mode",  { "Grid", "Transient", "Manual" }, 0);
            s.c (id::stretchRetrig,"Stretch Retrigger", divs, defaultDivisionIndex ("1 bar"));
            s.f (id::stretch,      "Stretch",       lin (0, 1),        0.0f);
            s.b (id::stretchSync,  "Stretch Sync",  false);
            s.f (id::stretchSens,  "Stretch Sensitivity", lin (0, 1),  0.5f);
            s.b (id::stretchActive,"Stretch Activate", true);
            s.f (id::scrubPos,     "Scrub Position", lin (0, 1),       0.0f);
            s.c (id::scrubMode,    "Scrub Mode",    scrubModeNames(), 0);
            s.c (id::loopLength,   "Loop Length",   loopLengthNames(), 2);
            s.b (id::recHold,      "Record Hold",   false);
            s.b (id::reverse,      "Reverse",       false);
            s.f (id::revProb,      "Reverse Probability", lin (0, 1), 0.0f);
            s.f (id::spray,        "Spray",         lin (0, 1),        0.0f);
            s.f (id::blur,         "Blur",          lin (0, 1),        0.0f);
            s.f (id::grainLevel,   "Grain Level",   lin (-48, 12),     0.0f, true, "dB");

            // ---- pitch
            s.f (id::pitch,       "Grain Pitch",   lin (-24, 24),      0.0f, true, "st");
            s.c (id::pitchMode,   "Pitch Mode",    { "Repitch", "Free", "Scale" }, 2);
            s.f (id::detune,      "Detune",        lin (0, 1),         0.0f);
            s.c (id::retune,      "Retune Mode",   { "Off", "Smooth", "Hard" }, 0);
            s.c (id::chordMode,   "Chord Mode",    { "Off", "Arp", "Chord" }, 0);
            s.f (id::arpRand,     "Arp Randomize", lin (0, 1),         0.0f);
            s.f (id::chordSpread, "Chord Stereo Spread", lin (0, 1),   0.5f);
            {
                const int defInt[] { 0, 2, 4, 7, 9, 11 };
                for (int i = 1; i <= numIntervals; ++i)
                {
                    s.n (id::interval (i), "Interval " + juce::String (i), -24, 24, defInt[i - 1]);
                    s.b (id::intervalOn (i), "Interval " + juce::String (i) + " Active", i <= 3);
                }
            }

            // ---- grain filter
            s.b (id::fltOn,       "Grain Filter",  false);
            s.c (id::fltMode,     "Filter Mode",   filterModeNames(), 0);
            s.b (id::fltScale,    "Filter Scale",  false);
            s.f (id::fltCutoff,   "Filter Cutoff", skewed (20, 20000, 1000), 2000.0f, true, "Hz");
            s.b (id::fltRandom,   "Filter Random", false);
            s.f (id::fltModDepth, "Filter Mod Depth", lin (-1, 1),     0.0f);
            s.f (id::fltModRate,  "Filter Mod Rate",  skewed (0.25f, 64, 2), 1.0f);
            s.f (id::fltReso,     "Filter Resonance", lin (0, 1),      0.2f);
            s.f (id::fltDrive,    "Filter Drive",  lin (0, 1),         0.0f);
            s.f (id::fltMorph,    "Filter Morph",  lin (0, 1),         0.0f);
            s.f (id::fltDamp,     "Filter Damping",lin (0, 1),         0.3f);
            s.f (id::fltSmooth,   "Filter Smooth", lin (0, 1),         0.0f);
            s.f (id::fltBits,     "Filter Bits",   lin (1, 16),        16.0f);
            s.f (id::fltMix,      "Filter Mix",    lin (0, 1),         1.0f);

            // ---- imager
            s.b (id::imgOn,        "Grain Imager", true);
            s.c (id::spreadMode,   "Spread Mode",  spreadModeNames(), 0);
            s.b (id::smoothSpread, "Smooth Spread", false);
            s.f (id::spread,       "Spread",       lin (0, 1),         0.0f);
            s.f (id::pan,          "Pan",          lin (-1, 1),        0.0f);
            s.f (id::dimension,    "Dimension",    lin (0, 1),         0.0f);
            s.f (id::dimSize,      "Dimension Size", lin (0, 1),       0.5f);
            s.b (id::monoLows,     "Mono Lows",    false);
            s.f (id::monoFreq,     "Mono Frequency", skewed (20, 500, 120), 120.0f, true, "Hz");
            s.f (id::width,        "Stereo Width", lin (0, 2),         1.0f);
            s.b (id::safeWidth,    "Safe Width",   true);

            // ---- grain delay
            s.b (id::dlyOn,       "Grain Delay",   false);
            s.b (id::dlyPingPong, "Ping Pong",     false);
            s.b (id::dlySwap,     "Channel Swap",  false);
            s.b (id::dlySync,     "Delay Sync",    true);
            s.f (id::dlyTime,     "Delay Time",    skewed (1, 2000, 250), 250.0f, true, "ms");
            s.c (id::dlyDiv,      "Delay Division", divs, defaultDivisionIndex ("1/8"));
            s.n (id::dlyTaps,     "Delay Taps",    1, 8, 3);
            s.c (id::dlyPattern,  "Shift Pattern", shiftPatternNames(), 0);
            s.f (id::dlyShift,    "Shift",         lin (-12, 12),      0.0f, true, "st");
            s.f (id::dlyFeedback, "Delay Feedback",lin (0, 0.98f),     0.3f);
            s.f (id::dlySpace,    "Delay Space",   lin (0, 1),         0.0f);
            s.f (id::dlyLP,       "Delay Lowpass", skewed (200, 20000, 4000), 20000.0f, true, "Hz");
            s.f (id::dlyHP,       "Delay Highpass",skewed (20, 5000, 300), 20.0f, true, "Hz");
            s.f (id::dlyMix,      "Delay Mix",     lin (0, 1),         0.4f);

            // ---- macros / animator
            s.f (id::macro1,      "Macro 1",       lin (0, 1),         0.0f, false);
            s.f (id::macro2,      "Macro 2",       lin (0, 1),         0.0f, false);
            s.f (id::animDepth,   "Animator Depth",lin (0, 1),         0.0f, false);
            s.f (id::animDrift,   "Animator Drift",lin (0, 1),         0.0f, false);
            s.c (id::animMode,    "Animator Mode", { "Orbit", "Weave" }, 0);
            s.n (id::animCycles,  "Animator Cycles", 1, 8, 1);
            s.f (id::animWarp,    "Animator Warp", lin (0, 1),         0.3f, false);
            s.f (id::animRate,    "Animator Rate", skewed (0.01f, 10, 0.5f), 0.25f, false, "Hz");
            s.b (id::animSync,    "Animator Sync", false);
            s.c (id::animDiv,     "Animator Division", divs, defaultDivisionIndex ("1 bar"));

            // ---- morph / formant
            s.b (id::morphOn,     "Morph",          false);
            s.c (id::morphSrc,    "Morph Source B", { "Aux In", "Modeler" }, srcModeler);
            s.f (id::morphX,      "Morph X-Fade",   lin (0, 1),       0.5f);
            s.f (id::morphY,      "Morph Depth",    lin (0, 1),       1.0f);
            s.c (id::morphAlgo,   "Morph Algorithm", morphAlgoNames(), 0);
            s.f (id::morphGamma,  "Morph Gamma",    skewed (0.25f, 4.0f, 1.0f), 1.0f, true, "x");
            s.f (id::morphDetail, "Morph Detail",   lin (0, 1),       0.5f);
            s.b (id::formOn,      "Formants",       false);
            s.f (id::formShift,   "Formant Shift",  lin (-12, 12),    0.0f, true, "st");

            // ---- transients
            s.b (id::trOn,        "Transients",     false);
            s.f (id::trSens,      "Transient Sensitivity", lin (0, 1), 0.5f);
            s.f (id::trLength,    "Transient Length", skewed (2, 300, 40), 40.0f, true, "ms");
            s.f (id::trAmount,    "Transient Amount", lin (0, 1),   1.0f);

            // ---- modeler
            s.c (id::mdlMode,     "Modeler Mode",   modelerModeNames(), 0);
            s.n (id::mdlOctave,   "Modeler Octave", -3, 2, 0);
            s.c (id::mdlRate,     "Modeler Rate",   divs, defaultDivisionIndex ("1/16"));
            s.f (id::mdlTone,     "Modeler Tone",   lin (0, 1),       0.5f);
            s.f (id::mdlMotion,   "Modeler Motion", lin (0, 1),       0.3f);
            s.b (id::mdlFollow,   "Modeler Follow", false);

            // ---- mixer
            s.f (id::mixWet,      "Mix Morph",      skewed (0, 1.4f, 0.4f), 1.0f, true, "lvl");
            s.f (id::mixDry,      "Mix Main In",    skewed (0, 1.4f, 0.4f), 0.0f, true, "lvl");
            s.f (id::mixAux,      "Mix Aux In",     skewed (0, 1.4f, 0.4f), 0.0f, true, "lvl");
            s.f (id::mixModel,    "Mix Modeler",    skewed (0, 1.4f, 0.4f), 0.0f, true, "lvl");
            {
                const char* chn[] { "Morph", "Main In", "Aux In", "Modeler" };
                for (int c = 0; c < numMixChannels; ++c)
                {
                    s.b (id::mute (c), juce::String ("Mute ") + chn[c], false);
                    s.b (id::solo (c), juce::String ("Solo ") + chn[c], false);
                }
            }
            s.f (id::revSize,     "Reverb Size",    lin (0, 1),       0.6f);
            s.f (id::revDamp,     "Reverb Damping", lin (0, 1),       0.4f);
            s.f (id::revMix,      "Reverb Amount",  lin (0, 1),       0.0f);
            s.f (id::mmDepth,     "Morph Motion",   lin (0, 1),       0.0f, false);
            s.f (id::mmRate,      "Morph Motion Rate", skewed (0.01f, 10, 0.5f), 0.25f, false, "Hz");
            s.b (id::mmSync,      "Morph Motion Sync", false);
            s.c (id::mmDiv,       "Morph Motion Division", divs, defaultDivisionIndex ("1 bar"));
            s.c (id::mmMode,      "Morph Motion Shape", { "Orbit", "Weave" }, 0);

            // ---- melody generator
            s.c (id::mlStyle,     "Melody Style",   melodyStyleNames(), 0);
            s.f (id::mlVar,       "Melody Variation", lin (1, 99, 1), 1.0f, true);
            s.f (id::mlDensity,   "Melody Density", lin (0, 1),       0.7f);
            s.f (id::mlRange,     "Melody Range",   lin (1, 3, 1),    2.0f, true, "oct");
            s.c (id::mlLength,    "Melody Length",  melodyLengthNames(), 1);
            s.c (id::mlTrigger,   "Melody Trigger", melodyTriggerNames(), 0);
            s.f (id::mlGlide,     "Melody Glide",   lin (0, 1),       0.2f);
            s.f (id::mlSens,      "Kick Sensitivity", lin (0, 1),     0.6f);

            // ---- effects rack
            const int defFx[] { fxEqualizer, fxChorus, fxDelay, fxReverb };
            for (int k = 1; k <= numFxSlots; ++k)
            {
                const auto p = "FX " + juce::String (k) + " ";
                s.c (id::fx (k, "Type"), p + "Type", fxTypeNames(), defFx[k - 1]);
                s.b (id::fx (k, "On"),   p + "On",   false);
                s.f (id::fx (k, "Mix"),  p + "Mix",  lin (0, 1), 1.0f);
                s.f (id::fx (k, "A"),    p + "A",    lin (0, 1), 0.5f);
                s.f (id::fx (k, "B"),    p + "B",    lin (0, 1), 0.5f);
                s.f (id::fx (k, "C"),    p + "C",    lin (0, 1), 0.5f);
                s.f (id::fx (k, "D"),    p + "D",    lin (0, 1), 0.5f);
            }

            // ---- modulators
            for (int m = 1; m <= numModulators; ++m)
            {
                const auto p = "Mod " + juce::String (m) + " ";
                s.c (id::mod (m, "Type"),  p + "Type",  modTypeNames(), m == 4 ? modEnvelope : (m == 3 ? modSequencer : modLfo));
                s.f (id::mod (m, "Rate"),  p + "Rate",  skewed (0.01f, 20, 1), 1.0f, false, "Hz");
                s.b (id::mod (m, "Sync"),  p + "Sync",  true);
                s.c (id::mod (m, "Div"),   p + "Division", divs, defaultDivisionIndex (m == 1 ? "1 bar" : "1/16"));
                s.f (id::mod (m, "Shape"), p + "Shape", lin (0, 1), 0.0f, false);
                s.f (id::mod (m, "Phase"), p + "Phase", lin (0, 1), 0.0f, false);
                s.n (id::mod (m, "Steps"), p + "Steps", 2, numSeqSteps, 8);
                s.f (id::mod (m, "Glide"), p + "Glide", lin (0, 1), 0.0f, false);
                s.f (id::mod (m, "Atk"),   p + "Attack",  skewed (0.1f, 500, 10), 10.0f, false, "ms");
                s.f (id::mod (m, "Rel"),   p + "Release", skewed (1, 2000, 150), 150.0f, false, "ms");
                s.f (id::mod (m, "Gain"),  p + "Gain",    lin (-12, 24), 6.0f, false, "dB");
            }

            // ---- matrix (destinations = every modulatable float so far)
            juce::StringArray dst { "None" };
            for (auto& sp : s.specs)
                if (sp.kind == Spec::Float && sp.modulatable) dst.add (sp.name);

            for (int k = 1; k <= numMatrixSlots; ++k)
            {
                const auto p = "Mod Slot " + juce::String (k) + " ";
                s.c (id::mx (k, "Src"), p + "Source", modSourceNames(), 0);
                s.c (id::mx (k, "Dst"), p + "Target", dst, 0);
                s.f (id::mx (k, "Amt"), p + "Amount", lin (-1, 1), 0.0f, false);
            }
            return s.specs;
        }();
        return list;
    }
}

const juce::StringArray& modTargetIds()
{
    static const juce::StringArray ids = []
    {
        juce::StringArray a;
        for (auto& sp : allSpecs())
            if (sp.kind == Spec::Float && sp.modulatable) a.add (sp.id);
        return a;
    }();
    return ids;
}

juce::StringArray modTargetNames()
{
    juce::StringArray a { "None" };
    for (auto& sp : allSpecs())
        if (sp.kind == Spec::Float && sp.modulatable) a.add (sp.name);
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& sp : allSpecs())
    {
        const juce::ParameterID pid { sp.id, 1 };
        switch (sp.kind)
        {
            case Spec::Float:
            {
                const auto unit = sp.unit;
                auto attrs = juce::AudioParameterFloatAttributes().withLabel (unit == "lvl" ? juce::String ("dB") : unit)
                    .withStringFromValueFunction ([unit] (float v, int)
                    {
                        if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + "k" : juce::String (v, v < 100.0f ? 1 : 0);
                        if (unit == "dB") return juce::String (v, 1);
                        if (unit == "ms") return juce::String (v, v < 10.0f ? 1 : 0);
                        if (unit == "st") return juce::String (v, 1);
                        if (unit == "x")  return juce::String (v, 2);
                        if (unit == "oct") return juce::String ((int) std::lround (v));
                        if (unit == "lvl") return v <= 0.0001f ? juce::String ("-inf") : juce::String (20.0f * std::log10 (v), 1);
                        return juce::String (v, 2);
                    });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, sp.name, sp.range, sp.def, attrs));
                break;
            }
            case Spec::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, sp.name, sp.def > 0.5f));
                break;
            case Spec::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, sp.name, sp.choices, (int) sp.def));
                break;
            case Spec::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, sp.name, (int) sp.range.start,
                                                                       (int) sp.range.end, (int) sp.def));
                break;
        }
    }
    return layout;
}
} // namespace liq
