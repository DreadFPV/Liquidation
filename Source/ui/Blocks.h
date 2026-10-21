#pragma once

#include "Widgets.h"
#include "WaveformDisplay.h"
#include "Panels.h"

namespace liq::ui
{
// ---------------------------------------------------------------------------
// Small building blocks
// ---------------------------------------------------------------------------

// Radio buttons for a choice parameter.
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (APVTS& s, const juce::String& paramId, juce::StringArray labels, int columns);
    void resized() override;
    void setTooltips (const juce::StringArray& tips);
    int selected() const noexcept { return current; }
private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns = 1, current = 0;
};

// Power switch bound to a bool parameter (used inside blocks for sub-sections).
class PowerToggle : public juce::ToggleButton
{
public:
    PowerToggle (APVTS& s, const juce::String& paramId, const juce::String& text = {});
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Horizontal fader with caps label and value readout (mixer rows, gains).
class FaderRow : public juce::Component
{
public:
    FaderRow (Ctx& c, const juce::String& paramId, const juce::String& label, int labelWidth = 70);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    Ctx& ctx;
    juce::String label;
    int labelW;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Mixer channel: fader + mute + solo.
class MixChannel : public juce::Component
{
public:
    MixChannel (Ctx& c, int channel, const juce::String& paramId, const juce::String& label, juce::Colour tag);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    FaderRow fader;
    ParamToggle mute, solo;
    juce::Colour tagColour;
};

// Compact value field (drag up/down), e.g. global tune in the top bar.
class ValueField : public juce::Component
{
public:
    ValueField (Ctx& c, const juce::String& paramId, const juce::String& label);
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider slider;
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// XY pad of the morph block: X = cross-fade A/B, Y = morph depth. Shows the animator path.
class MorphPad : public juce::Component
{
public:
    explicit MorphPad (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void update();
private:
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (1.0f); }
    void setFromMouse (juce::Point<float>);
    Ctx& ctx;
    float cx = 0.5f, cy = 1.0f;
    bool dragging = false;
};

// Live spectra of A, B and the morph output.
class SpectrumStrip : public juce::Component
{
public:
    explicit SpectrumStrip (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
private:
    Ctx& ctx;
};

// Note display of the modeler (one octave with scale notes and the sounding note).
class NoteStrip : public juce::Component
{
public:
    explicit NoteStrip (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void update();
private:
    Ctx& ctx;
    int note = -1;
    std::array<bool, 12> mask {};
};

// B-input selector drawn between the inputs and the morph block.
class SourceSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SourceSwitch (Ctx& c);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void update();
private:
    Ctx& ctx;
    int src = 1;
};

// ---------------------------------------------------------------------------
// Blocks of the signal-flow view
// ---------------------------------------------------------------------------
class Block : public Page
{
public:
    Block (Ctx& c, const juce::String& title, const juce::String& powerParam = {});
    void resized() override;
    virtual void layout (juce::Rectangle<int> content) = 0;
    virtual void update() {}
    Section& panel() noexcept { return *sec; }
protected:
    Section* sec;
};

class MainInBlock : public Block
{
public:
    explicit MainInBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
    void paint (juce::Graphics&) override {}
private:
    FaderRow gain;
    Meter meter;
};

class AuxInBlock : public Block
{
public:
    explicit AuxInBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    FaderRow gain;
    Meter meter;
    juce::Label status;
};

class ModelerBlock : public Block
{
public:
    explicit ModelerBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    ChoiceButtons modes;
    Knob *octave, *tone, *motion;
    ParamCombo* rate;
    ParamToggle* follow;
    NoteStrip notes;
    Meter meter;
    juce::Label hint, rateLabel;
};

class MorphBlock : public Block
{
public:
    explicit MorphBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    MorphPad pad;
    ChoiceButtons algos;
    juce::Label algoInfo, motionLabel, warning;
    Knob *gamma, *detail, *depth, *speed;
    ParamCombo *shape, *div;
    ParamToggle* sync;
    SpectrumStrip spectrum;
    int lastAlgo = -1;
};

class FormantBlock : public Block
{
public:
    explicit FormantBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
private:
    Knob* shift;
    juce::Label info;
};

class GrainBlock : public Block
{
public:
    explicit GrainBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    WaveformDisplay wave;
    ChoiceButtons mode;
    ParamCombo *rateMode, *chord;
    Knob *rate, *sizeRatio, *sizeMs, *stretch, *position, *pitch, *spray, *blur, *reverse;
};

class GrainFxBlock : public Block
{
public:
    explicit GrainFxBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    PowerToggle fltOn, imgOn, dlyOn;
    ParamCombo *fltMode, *spreadMode, *pattern;
    Knob *cutoff, *reso, *spread, *width, *feedback, *mix;
};

class TransientBlock : public Block
{
public:
    explicit TransientBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
    void paintOverChildren (juce::Graphics&) override;
private:
    Knob *sens, *length, *amount;
    float gate = 0.0f;
};

class MixerBlock : public Block
{
public:
    explicit MixerBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
private:
    juce::OwnedArray<MixChannel> channels;
    juce::Label revLabel;
    Knob *size, *damp, *amount;
};

class EffectsBlock : public Block
{
public:
    explicit EffectsBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    struct Slot : public juce::Component
    {
        Slot (Ctx& c, int index);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;
        Ctx& ctx; int k;
        PowerToggle power;
        juce::String name;
    };
    juce::OwnedArray<Slot> slots;
    ChoiceButtons routing;
};

class OutputBlock : public Block
{
public:
    explicit OutputBlock (Ctx& c);
    void layout (juce::Rectangle<int>) override;
    void update() override;
private:
    FaderRow gain;
    ParamToggle* limit;
    Meter meter;
    juce::Label gr;
};

// ---------------------------------------------------------------------------
// Canvas: positions the blocks and draws the signal lines behind them.
// ---------------------------------------------------------------------------
class SignalCanvas : public juce::Component
{
public:
    explicit SignalCanvas (Ctx& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();
    void refreshModulation();
    juce::Array<Block*> blocks() const;

private:
    Ctx& ctx;
    MainInBlock mainIn;
    AuxInBlock auxIn;
    ModelerBlock modeler;
    MorphBlock morph;
    FormantBlock formant;
    GrainBlock grain;
    GrainFxBlock grainFx;
    TransientBlock transients;
    MixerBlock mixer;
    EffectsBlock effects;
    OutputBlock output;
    SourceSwitch sourceSwitch;
    int frame = 0, lastWireState = -1;
};

// ---------------------------------------------------------------------------
// Top bar, modulation bar, help card, detail overlay
// ---------------------------------------------------------------------------
class TopBar : public Page
{
public:
    explicit TopBar (Ctx& c);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
    void refreshPresetList();
    juce::TextButton help { "?" }, playTab { "Play" }, editTab { "Edit" };
    std::function<void (int)> onPage;   // 0 = Play, 1 = Edit
    void setPage (int p);

private:
    void savePreset();
    juce::ComboBox presetBox;
    juce::TextButton prev { "<" }, next { ">" }, save { "Save" };
    ParamCombo *root, *scale;
    ValueField tune;
    juce::String lastName;
    int lastCount = -1;
};

class ModBar : public Page
{
public:
    explicit ModBar (Ctx& c);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
    std::function<void (int)> onModulatorClicked;   // 0..3
    std::function<void()> onMatrix;

private:
    std::array<std::unique_ptr<ModChip>, 6> chips;
    Knob *m1, *m2;
    juce::TextButton matrixButton { "Mod Matrix" };
};

class HelpCard : public juce::Component
{
public:
    void show (const juce::String& title, const juce::String& text, juce::Rectangle<int> near, juce::Rectangle<int> limits);
    void paint (juce::Graphics&) override;
private:
    juce::String heading, body;
};

class DetailOverlay : public juce::Component
{
public:
    explicit DetailOverlay (Ctx& c);
    void open (int page);
    void close() { setVisible (false); current = -1; }
    int currentPage() const noexcept { return current; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();
    void refreshModulation();
    void selectModulator (int m) { modPage->selectModulator (m); }
    std::function<void()> onClosed;

private:
    Ctx& ctx;
    std::unique_ptr<GrainPage> grainPage;
    std::unique_ptr<ShapePage> shapePage;
    std::unique_ptr<EffectsPage> fxPage;
    std::unique_ptr<ModPage> modPage;
    juce::TextButton closeButton { "Close" };
    int current = -1;
    juce::String title;
};
}
