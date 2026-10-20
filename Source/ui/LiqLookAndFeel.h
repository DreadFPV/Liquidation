#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace liq::ui
{
namespace col
{
    const juce::Colour bg        { 0xff090d12 };
    const juce::Colour panel     { 0xff10161e };
    const juce::Colour panelHi   { 0xff161e29 };
    const juce::Colour edge      { 0xff212c39 };
    const juce::Colour track     { 0xff1e2733 };
    const juce::Colour text      { 0xffd9e4ec };
    const juce::Colour dim       { 0xff7d8c9b };
    const juce::Colour faint     { 0xff3a4757 };
    const juce::Colour accent    { 0xff33e1c6 };   // liquid cyan
    const juce::Colour accentDk  { 0xff168a7b };
    const juce::Colour mod       { 0xffa071ff };   // modulation violet
    const juce::Colour amber     { 0xffffb547 };
    const juce::Colour left      { 0xff4fc3ff };
    const juce::Colour right     { 0xffff6fb5 };
    const juce::Colour red       { 0xffff5a5a };
}

juce::Font font (float size, bool bold = false);

class LiqLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LiqLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.5f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f, true); }
    juce::Font getLabelFont (juce::Label&) override { return font (12.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
};
}
