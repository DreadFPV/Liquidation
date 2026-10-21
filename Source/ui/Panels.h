#pragma once

#include "Widgets.h"
#include "WaveformDisplay.h"

namespace liq::ui
{
// ---------------------------------------------------------------------------
class GrainPage : public Page
{
public:
    explicit GrainPage (Ctx& c);
    void resized() override;
    void update();

    WaveformDisplay waveform;

private:
    Section *grainSec, *playSec, *pitchSec;
    Knob *rate, *sizeMs, *sizeRatio, *shape, *skew, *jitter;
    ParamCombo* rateMode; ParamToggle* link;
    ParamCombo *playMode, *stretchMode, *retrig, *scrubMode;
    ParamToggle *stretchSync, *activate, *reverse;
    Knob *stretch, *sens, *scrubPos, *spray, *blur, *revProb, *level;
    juce::Label *lblStretchMode, *lblRetrig, *lblScrubMode;
    Knob *pitch, *detune, *arpRand, *spreadK;
    ParamCombo *pitchMode, *retune, *chordMode;
    std::unique_ptr<IntervalStrip> intervals;
    std::unique_ptr<ScaleKeyboard> keyboard;
    int lastRateMode = -1;
};

// ---------------------------------------------------------------------------
class ShapePage : public Page
{
public:
    explicit ShapePage (Ctx& c);
    void resized() override;
    void update();

private:
    Section *fltSec, *imgSec, *dlySec;
    ParamCombo* fltMode; ParamToggle *fltScale, *fltRandom;
    Knob *cutoff, *reso, *drive, *modDepth, *modRate, *morph, *damp, *smooth, *bits, *fmix;
    ParamCombo* spreadMode; ParamToggle *smoothSpread, *monoLows, *safeWidth;
    Knob *spread, *pan, *dim, *dimSize, *monoFreq, *width;
    ParamToggle *ping, *swap, *sync;
    ParamCombo *dlyDiv, *pattern;
    Knob *dlyTime, *taps, *shift, *feedback, *space, *lp, *hp, *dmix;
    std::unique_ptr<TapPatternView> tapView;
};

// ---------------------------------------------------------------------------
class EffectsPage : public Page
{
public:
    explicit EffectsPage (Ctx& c);
    void resized() override;
    void update();

private:
    struct Slot { Section* sec; ParamCombo* type; Knob* mix; std::array<Knob*, 4> k; int lastType = -1; };
    std::array<Slot, numFxSlots> slots;
};

// ---------------------------------------------------------------------------
class StepEditor : public juce::Component
{
public:
    StepEditor (Ctx& c, int modIndex) : ctx (c), m (modIndex) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { edit (e); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }
private:
    void edit (const juce::MouseEvent&);
    Ctx& ctx; int m;
};

class ModShapeView : public juce::Component
{
public:
    ModShapeView (Ctx& c, int modIndex) : ctx (c), m (modIndex) {}
    void paint (juce::Graphics&) override;
private:
    Ctx& ctx; int m;
};

class ModPage : public Page
{
public:
    explicit ModPage (Ctx& c);
    void resized() override;
    void update();
    void selectModulator (int m);    // 0..3
    int selected() const noexcept { return current; }

private:
    struct ModPanel
    {
        Section* sec; ParamCombo *type, *div; ParamToggle* sync;
        Knob *rate, *shape, *phase, *steps, *glide, *atk, *rel, *gain;
        std::unique_ptr<StepEditor> stepEditor;
        std::unique_ptr<ModShapeView> shapeView;
    };
    std::array<ModPanel, numModulators> mods;
    int current = 0;

    Section* matrixSec;
    struct Row { ParamCombo *src, *dst; juce::Slider* amt; juce::TextButton* clear; };
    std::array<Row, numMatrixSlots> rows;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> amtAttachments;
};

}
