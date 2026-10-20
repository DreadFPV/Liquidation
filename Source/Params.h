#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// ============================================================================
// Parameter IDs and helpers for Liquidation.
// ============================================================================
namespace liq
{
    // ---- Musical divisions --------------------------------------------------
    struct Division
    {
        const char* name;
        double beats;   // length in quarter notes
        int kind;       // 0 straight, 1 triplet, 2 dotted
    };

    const std::vector<Division>& divisions();            // full table, shortest first
    juce::StringArray divisionNames();
    int defaultDivisionIndex (const char* name);

    // ---- Choice lists -------------------------------------------------------
    juce::StringArray rateModeNames();      // Free, Synced, Straight, Triplet, Dotted, Scale, MIDI
    juce::StringArray scaleTypeNames();
    juce::StringArray noteNames();
    juce::StringArray filterModeNames();
    juce::StringArray spreadModeNames();
    juce::StringArray shiftPatternNames();
    juce::StringArray fxTypeNames();
    juce::StringArray modTypeNames();
    juce::StringArray modSourceNames();     // None, Macro 1, Macro 2, Mod 1..4
    juce::StringArray loopLengthNames();
    double loopLengthBeats (int index);
    juce::StringArray scrubModeNames();     // Free + divisions
    double scrubModeBeats (int index);      // 0 for Free

    enum RateMode     { rateFree = 0, rateSynced, rateStraight, rateTriplet, rateDotted, rateScale, rateMidi };
    enum PlayMode     { playStretch = 0, playScrub };
    enum StretchMode  { stretchGrid = 0, stretchTransient, stretchManual };
    enum PitchMode    { pitchRepitch = 0, pitchFree, pitchScale };
    enum RetuneMode   { retuneOff = 0, retuneSmooth, retuneHard };
    enum ChordMode    { chordOff = 0, chordArp, chordChord };
    enum FilterMode   { fLowpass = 0, fHighpass, fBandpass, fNotch, fMorph, fVowel, fComb, fFlange,
                        fModal, fPhaser, fAllpass, fReducer, fNumModes };
    enum SpreadMode   { spAlternate = 0, spPendulum, spHelix, spBloom, spScatter, spDrift };
    enum ShiftPattern { shHold = 0, shAlternate, shRamp, shPulse, shTriangle, shDiverge, shRandom, shChaos };
    enum FxType       { fxChorus = 0, fxCompressor, fxDelay, fxDistortion, fxEqualizer, fxFilter,
                        fxReverb, fxShifter, fxNumTypes };
    enum ModType      { modLfo = 0, modSequencer, modEnvelope };
    enum Routing      { routePost = 0, routePre };

    constexpr int numIntervals   = 6;
    constexpr int numFxSlots     = 4;
    constexpr int numModulators  = 4;
    constexpr int numMatrixSlots = 16;
    constexpr int numSeqSteps    = 16;
    constexpr int numModSources  = 7;   // incl. None

    // ---- Parameter IDs (fixed) ---------------------------------------------
    namespace id
    {
        // global
        inline const juce::String dryWet { "dryWet" }, routing { "routing" }, inputGain { "inputGain" },
            outputGain { "outputGain" }, globalTune { "globalTune" }, scaleRoot { "scaleRoot" },
            scaleType { "scaleType" };
        // grain
        inline const juce::String grainRate { "grainRate" }, rateMode { "rateMode" }, grainSize { "grainSize" },
            sizeRatio { "sizeRatio" }, sizeLink { "sizeLink" }, winShape { "winShape" }, winSkew { "winSkew" },
            jitter { "jitter" };
        // playback
        inline const juce::String playMode { "playMode" }, stretchMode { "stretchMode" },
            stretchRetrig { "stretchRetrig" }, stretch { "stretch" }, stretchSync { "stretchSync" },
            stretchSens { "stretchSens" }, stretchActive { "stretchActive" }, scrubPos { "scrubPos" },
            scrubMode { "scrubMode" }, loopLength { "loopLength" }, recHold { "recHold" }, reverse { "reverse" },
            revProb { "revProb" }, spray { "spray" }, blur { "blur" }, grainLevel { "grainLevel" };
        // pitch
        inline const juce::String pitch { "pitch" }, pitchMode { "pitchMode" }, detune { "detune" },
            retune { "retune" }, chordMode { "chordMode" }, arpRand { "arpRand" }, chordSpread { "chordSpread" };
        // grain filter
        inline const juce::String fltOn { "fltOn" }, fltMode { "fltMode" }, fltScale { "fltScale" },
            fltCutoff { "fltCutoff" }, fltRandom { "fltRandom" }, fltModDepth { "fltModDepth" },
            fltModRate { "fltModRate" }, fltReso { "fltReso" }, fltDrive { "fltDrive" }, fltMorph { "fltMorph" },
            fltDamp { "fltDamp" }, fltSmooth { "fltSmooth" }, fltBits { "fltBits" }, fltMix { "fltMix" };
        // imager
        inline const juce::String imgOn { "imgOn" }, spreadMode { "spreadMode" }, smoothSpread { "smoothSpread" },
            spread { "spread" }, pan { "pan" }, dimension { "dimension" }, dimSize { "dimSize" },
            monoLows { "monoLows" }, monoFreq { "monoFreq" }, width { "width" }, safeWidth { "safeWidth" };
        // grain delay
        inline const juce::String dlyOn { "dlyOn" }, dlyPingPong { "dlyPingPong" }, dlySwap { "dlySwap" },
            dlySync { "dlySync" }, dlyTime { "dlyTime" }, dlyDiv { "dlyDiv" }, dlyTaps { "dlyTaps" },
            dlyPattern { "dlyPattern" }, dlyShift { "dlyShift" }, dlyFeedback { "dlyFeedback" },
            dlySpace { "dlySpace" }, dlyLP { "dlyLP" }, dlyHP { "dlyHP" }, dlyMix { "dlyMix" };
        // animator / macros
        inline const juce::String macro1 { "macro1" }, macro2 { "macro2" }, animDepth { "animDepth" },
            animDrift { "animDrift" }, animMode { "animMode" }, animCycles { "animCycles" }, animWarp { "animWarp" },
            animRate { "animRate" }, animSync { "animSync" }, animDiv { "animDiv" };

        // indexed (1-based)
        juce::String scaleNote (int pc);                 // pc 0..11
        juce::String interval (int i);
        juce::String intervalOn (int i);
        juce::String fx (int slot, const char* field);   // Type, On, Mix, A, B, C, D
        juce::String mod (int m, const char* field);     // Type, Rate, Sync, Div, Shape, Phase, Steps, Glide, Atk, Rel, Gain
        juce::String mx (int s, const char* field);      // Src, Dst, Amt
    }

    // Modulatable (continuous) parameters, in matrix-destination order.
    const juce::StringArray& modTargetIds();
    juce::StringArray modTargetNames();   // "None" + display names

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
