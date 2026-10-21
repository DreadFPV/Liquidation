#pragma once

#include <juce_core/juce_core.h>

namespace liq::ui
{
// Plain-language explanation of a control: what it does and what happens when you turn it.
struct ParamHelp
{
    juce::String title, what, more, less;
    bool valid() const noexcept { return title.isNotEmpty(); }
};

ParamHelp helpFor (const juce::String& paramId);
}
