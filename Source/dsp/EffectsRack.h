#pragma once

#include "FdnReverb.h"
#include "../Params.h"
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace liq
{
struct FxSlotParams
{
    int   type = fxChorus;
    bool  on = false;
    float mix = 1.0f;
    float a = 0.5f, b = 0.5f, c = 0.5f, d = 0.5f;
};

// Human readable labels for the four generic knobs of each effect type.
juce::StringArray fxKnobLabels (int type);
juce::String fxKnobText (int type, int knob, float v);

class EffectsRack
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const std::array<FxSlotParams, numFxSlots>& slots, double bpm);

    // ---------------------------------------------------------------------
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void lowShelf (double fs, double f, double gainDb);
        void highShelf (double fs, double f, double gainDb);
        void peak (double fs, double f, double q, double gainDb);
        void lowpass (double fs, double f, double q);
    };

    class Effect
    {
    public:
        virtual ~Effect() = default;
        virtual void prepare (double sampleRate, int maxBlock) = 0;
        virtual void reset() = 0;
        virtual void process (float* l, float* r, int n, const FxSlotParams& p, double bpm) = 0;
    };

private:
    struct Slot
    {
        std::array<std::unique_ptr<Effect>, fxNumTypes> fx;
        std::vector<float> dryL, dryR;
        int lastType = -1;
        float smoothedMix = 0.0f;
    };
    std::array<Slot, numFxSlots> slots;
    double sr = 48000.0;
};
}
