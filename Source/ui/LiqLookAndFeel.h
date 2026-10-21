#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace liq::ui
{
// Graphite hardware look: warm off-white text, one amber accent, steel signal lines,
// sage for modulation. Panels have cut corners.
namespace col
{
    const juce::Colour bg        { 0xff0f1012 };
    const juce::Colour canvas    { 0xff131416 };
    const juce::Colour panel     { 0xff1a1c1f };
    const juce::Colour panelHi   { 0xff22252a };
    const juce::Colour edge      { 0xff30343a };
    const juce::Colour track     { 0xff2b2f35 };
    const juce::Colour text      { 0xffe9e5dc };
    const juce::Colour dim       { 0xff8d9096 };
    const juce::Colour faint     { 0xff4b4f56 };
    const juce::Colour accent    { 0xffe8a23a };   // amber
    const juce::Colour accentDk  { 0xff5a3f17 };
    const juce::Colour line      { 0xff6f8597 };   // steel signal lines
    const juce::Colour mod       { 0xff79c2a8 };   // sage: modulation
    const juce::Colour amber     { 0xffe8a23a };
    const juce::Colour left      { 0xff7fb0d0 };
    const juce::Colour right     { 0xffd9a46a };
    const juce::Colour red       { 0xffe0574f };
    const juce::Colour sigA      { 0xff7fb0d0 };   // signal A (main)
    const juce::Colour sigB      { 0xffd9a46a };   // signal B (aux / modeler)
}

juce::Font font (float size, bool bold = false);
juce::Font fontLight (float size);

// Chamfered panel outline used by every block.
juce::Path chamfer (juce::Rectangle<float> r, float cut);
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool active, juce::Colour accent, float cut = 9.0f);
void drawCaps (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
               float size, juce::Colour c, bool bold = true, float tracking = 0.09f);

class LiqLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LiqLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.5f); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.5f, true); }
    juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};
}
