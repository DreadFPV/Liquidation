#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LiqLookAndFeel.h"
#include "../Params.h"

class LiquidationProcessor;

namespace liq::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// German UI text is UTF-8: "für"_de
inline juce::String operator""_de (const char* s, size_t n) { return juce::String::fromUTF8 (s, (int) n); }

// ---------------------------------------------------------------------------
// Shared helpers for every page.
// ---------------------------------------------------------------------------
struct Ctx
{
    LiquidationProcessor& proc;
    APVTS& apvts;
    // Assign a modulation source (1..6) to a parameter; returns false if the matrix is full.
    std::function<bool (const juce::String& paramId, int source)> addModulation;
    std::function<void (const juce::String& paramId)> clearModulation;
    std::function<void()> showMatrix;
    std::function<void (int page)> openDetails;   // 0 grain, 1 grain fx, 2 effects, 3 modulation
};

// Rotary knob with name, value readout, modulation ring and drop target for modulators.
class Knob : public juce::Component,
             public juce::DragAndDropTarget,
             public juce::SettableTooltipClient
{
public:
    Knob (Ctx& ctx, const juce::String& paramId, const juce::String& label);

    void setFormatter (std::function<juce::String (float realValue)> f) { formatter = std::move (f); repaint(); }
    void setLabel (const juce::String& l) { label = l; repaint(); }
    const juce::String& getParamId() const noexcept { return paramId; }

    // called by the editor timer
    void refreshModulation();

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    juce::Slider& getSlider() noexcept { return *ks; }

private:
    struct KnobSlider : public juce::Slider
    {
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
            juce::Slider::mouseDown (e);
        }
    };

    void showMenu();

    Ctx& ctx;
    juce::String paramId, label;
    juce::RangedAudioParameter* param = nullptr;
    std::function<juce::String (float)> formatter;
    // order matters: the slider must outlive its attachment
    std::unique_ptr<KnobSlider> ks;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    bool dropHover = false;
    float lastMod = 0.0f, lastLive = -1.0f;
};

// Small text toggle bound to a bool parameter.
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (APVTS& s, const juce::String& paramId, const juce::String& text);
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Combo bound to a choice parameter.
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (APVTS& s, const juce::String& paramId);
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Titled panel with cut corners. Optional power switch (bool parameter), details button and help text.
class Section : public juce::Component
{
public:
    Section (const juce::String& title, APVTS* s = nullptr, const juce::String& powerParam = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (12, 10).withTrimmedTop (24); }
    void setAccent (juce::Colour c) { accentColour = c; repaint(); }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void addDetailsButton (std::function<void()> onClick, const juce::String& text = "Details");
    void setHelp (const juce::String& helpTitle, const juce::String& helpBody) { helpHeading = helpTitle; helpText = helpBody; }
    bool isOn() const { return power == nullptr || power->getToggleState(); }
    void refreshPowerState();

    juce::String helpHeading, helpText;

private:
    juce::String title;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
    std::unique_ptr<juce::TextButton> details;
    juce::Colour accentColour = col::accent;
    bool lastOn = true;
};

// Draggable modulator chip (drag onto any knob to modulate it).
class ModChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModChip (Ctx& ctx, int sourceIndex, const juce::String& name);
    void setValue (float v) { if (std::abs (v - value) > 0.002f) { value = v; repaint(); } }
    void setSubtitle (const juce::String& s) { if (s != subtitle) { subtitle = s; repaint(); } }
    void setSelected (bool b) { if (b != selected) { selected = b; repaint(); } }
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    Ctx& ctx;
    int source;
    juce::String name, subtitle;
    float value = 0.0f;
    bool selected = false, dragging = false;
};

// 12-key scale editor.
class ScaleKeyboard : public juce::Component
{
public:
    explicit ScaleKeyboard (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
private:
    Ctx& ctx;
    std::array<bool, 12> mask {};
    int root = 0;
};

// Six interval editors (value + active) for chord / arp.
class IntervalStrip : public juce::Component
{
public:
    explicit IntervalStrip (Ctx& c);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Ctx& ctx;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<ParamToggle> toggles;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> attachments;
};

// Peak meter.
class Meter : public juce::Component
{
public:
    void setLevel (float l) { const float v = std::max (l, level * 0.88f); if (std::abs (v - level) > 0.001f) { level = v; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float level = 0.0f;
};

// Draw the grain-delay tap pattern.
class TapPatternView : public juce::Component
{
public:
    explicit TapPatternView (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
private:
    Ctx& ctx;
};

// Base class with helpers to create bound controls.
class Page : public juce::Component
{
public:
    explicit Page (Ctx& c) : ctx (c) {}

protected:
    Knob& knob (const juce::String& paramId, const juce::String& label, juce::Component* parent = nullptr);
    ParamCombo& combo (const juce::String& paramId, juce::Component* parent = nullptr);
    ParamToggle& toggle (const juce::String& paramId, const juce::String& text, juce::Component* parent = nullptr);
    Section& section (const juce::String& title, const juce::String& powerParam = {});
    juce::Label& caption (const juce::String& text, juce::Component* parent = nullptr);

    // lay out a row of components evenly inside r
    static void row (juce::Rectangle<int> r, std::initializer_list<juce::Component*> comps, int gap = 6);

    Ctx& ctx;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::Component> owned;

public:
    void refreshModulation() { for (auto* k : knobs) k->refreshModulation(); }
};
}
