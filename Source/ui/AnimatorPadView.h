#pragma once

#include "Widgets.h"

namespace liq::ui
{
// XY pad: X = Macro 1, Y = Macro 2. Shows the animator path and the live cursor.
class AnimatorPadView : public juce::Component
{
public:
    explicit AnimatorPadView (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void update();

private:
    juce::Rectangle<float> padArea() const { return getLocalBounds().toFloat().reduced (6.0f); }
    void setFromMouse (juce::Point<float>);
    Ctx& ctx;
    float cx = 0, cy = 0;
    bool dragging = false;
};
}
