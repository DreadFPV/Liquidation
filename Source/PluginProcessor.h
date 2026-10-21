#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/GrainEngine.h"
#include "dsp/EffectsRack.h"
#include "dsp/Modulation.h"
#include "dsp/Animator.h"
#include "dsp/SpectralMorph.h"
#include "dsp/Modeler.h"
#include "dsp/Dynamics.h"
#include "dsp/FdnReverb.h"
#include "dsp/DelayLine.h"
#include "dsp/PitchDetector.h"

#include <unordered_map>

class LiquidationProcessor : public juce::AudioProcessor
{
public:
    LiquidationProcessor();
    ~LiquidationProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return "Liquidation"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor
    juce::AudioProcessorValueTreeState apvts;
    liq::PresetManager presets;
    liq::GrainEngine engine;
    liq::ModSystem modSystem;
    liq::Animator animator;
    liq::SpectralMorph morph;
    liq::Modeler modeler;
    liq::TransientKeeper transients;
    liq::Limiter limiter;

    // grain engine latency (samples) – parallel paths are delayed to match
    static constexpr int grainLatency = 3;
    int totalLatency() const noexcept { return morph.latency() + grainLatency + liq::Limiter::lookahead; }

    std::atomic<float> mainLevel { 0.0f }, auxLevel { 0.0f };
    std::atomic<bool> auxConnected { false };

    std::atomic<double> hostBpm { 120.0 };
    // held while buffers are (re)allocated; the UI only reads engine buffers under a try-lock
    juce::CriticalSection prepareLock;
    std::atomic<float> outputLevel { 0.0f };

    // the current scale (as used by the engine), for the UI
    liq::Scale currentScale() const;
    // Full state (params + sequencer steps) for saving user presets
    juce::ValueTree fullState();
    void applyFullState (const juce::ValueTree& tree);

    // Real (modulated) value of a modulatable parameter, for display.
    float modulatedValue (const juce::String& paramId) const;

private:
    float raw (const juce::String& paramId) const;
    float mod (const juce::String& paramId) const;            // modulated real value
    int   choice (const juce::String& paramId) const { return (int) std::lround (raw (paramId)); }
    bool  flag (const juce::String& paramId) const { return raw (paramId) > 0.5f; }

    void buildEngineParams (liq::EngineParams& p, double ppq, bool playing, double bpm);

    std::unordered_map<juce::String, std::atomic<float>*> rawPtr;
    std::unordered_map<juce::String, int> targetIndex;
    std::vector<juce::RangedAudioParameter*> targetParams;

    liq::EffectsRack rack;
    liq::EngineParams engineParams;

    juce::AudioBuffer<float> dryBuffer, wetBuffer, monoWork;
    liq::FdnReverb mixReverb;
    std::array<liq::FixedDelay, 2> alignMain, alignAux, alignModel, grainBypass;
    liq::PitchDetector followDetector;
    int64_t followPos = 0;
    float auxMeter = 0.0f, mainMeter = 0.0f;
    double sr = 48000.0;
    double internalPpq = 0.0;
    static constexpr int subBlock = 32;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiquidationProcessor)
};
