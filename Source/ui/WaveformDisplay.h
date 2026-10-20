#pragma once

#include "Widgets.h"

namespace liq::ui
{
// Shows the captured loop, the playhead, spray range, retrigger markers and live grains.
class WaveformDisplay : public juce::Component
{
public:
    explicit WaveformDisplay (Ctx& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();   // called from the editor timer

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void updatePeaks();
    void setScrubFromMouse (float x);

    Ctx& ctx;
    std::vector<float> peaks;      // per 256-sample block
    int64_t peaksDoneUpTo = 0;     // absolute block index
    static constexpr int blockSize = 256;

    struct GrainDot { float x, trail, amp; int side; };
    std::vector<GrainDot> dots;
    struct Trail { float x; float life; int side; };
    std::vector<Trail> trails;
    float playhead = 0, spray = 0;
    int64_t regionStart = 0, regionLen = 1;
    std::vector<float> markerX;
    bool recording = true, scrub = false;
    float inputLevel = 0.0f;

    std::unique_ptr<ParamCombo> loopLength;
    std::unique_ptr<ParamToggle> hold;
    Meter meter;
    juce::Label info;
    bool draggingScrub = false;
};
}
