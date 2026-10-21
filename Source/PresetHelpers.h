#pragma once
// Helpers shared by the factory preset definitions.
#include "Presets.h"
#include "Params.h"
#include "dsp/GrainEngine.h"
#include "dsp/SpectralMorph.h"
#include "dsp/Modeler.h"

namespace liq
{
namespace presets
{
    enum Src { M1 = 1, M2 = 2, Mod1 = 3, Mod2 = 4, Mod3 = 5, Mod4 = 6 };

    inline void intervals (PresetWriter& w, std::initializer_list<int> values)
    {
        int i = 1;
        for (int v : values)
        {
            w.set (id::interval (i), (float) v);
            w.setOn (id::intervalOn (i), true);
            ++i;
        }
        for (; i <= numIntervals; ++i) w.setOn (id::intervalOn (i), false);
    }

    inline void fx (PresetWriter& w, int slot, int type, float mix, float a, float b, float c, float d)
    {
        w.setChoice (id::fx (slot, "Type"), type);
        w.setOn (id::fx (slot, "On"), true);
        w.set (id::fx (slot, "Mix"), mix);
        w.set (id::fx (slot, "A"), a); w.set (id::fx (slot, "B"), b);
        w.set (id::fx (slot, "C"), c); w.set (id::fx (slot, "D"), d);
    }

    // amount helper: semitones on the +-24 pitch knob -> normalised offset
    inline float semis (float s) { return s / 48.0f; }

    void appendGenrePresets (std::vector<FactoryPreset>& list);
    void appendKickMelodyPresets (std::vector<FactoryPreset>& list);
}
}
