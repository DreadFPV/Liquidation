#pragma once

#include "Blocks.h"
#include "../Presets.h"

namespace liq::ui
{
// XY pad for Macro 1 (x) and Macro 2 (y), with the animator figure on a calm grid (as in version 1).
class MacroPad : public juce::Component
{
public:
    explicit MacroPad (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void update();
private:
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (2.0f); }
    juce::Rectangle<float> inner() const { return area().reduced (16.0f); }
    void setFromMouse (juce::Point<float>);
    Ctx& ctx;
    float cx = 0, cy = 0;
    bool dragging = false;
    std::array<juce::Point<float>, 48> trail {};
    int trailPos = 0;
};

// Rotary that sets the Morph (effect) and Main In (original) mixer levels as one Dry/Wet control.
class MixKnob : public juce::Component
{
public:
    explicit MixKnob (Ctx& c);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
private:
    Ctx& ctx;
    juce::Slider slider;
    bool internalChange = false;
};

// Lists the factory presets that match the chosen source and genre.
class PresetList : public juce::Component
{
public:
    explicit PresetList (Ctx& c) : ctx (c) {}
    void setItems (const std::vector<int>& indices, bool stripGenre);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int rowHeight() const noexcept { return 24; }
    std::function<void()> onLoaded;
private:
    Ctx& ctx;
    std::vector<int> items;
    bool strip = false;
    int hover = -1;
};

class PresetFinder : public Page
{
public:
    explicit PresetFinder (Ctx& c);
    void resized() override;
    void paint (juce::Graphics&) override {}
    void update();
    void refreshList();
    std::function<void()> onSelectionChanged;
    juce::String source() const;
    juce::String genre() const;
private:
    Section* sec;
    juce::OwnedArray<juce::TextButton> sourceButtons, genreButtons;
    juce::Label sourceLabel, genreLabel, countLabel;
    juce::Viewport viewport;
    PresetList list;
    int lastPreset = -2;
};

// Tips for the chosen source + genre.
class TipsView : public juce::Component
{
public:
    explicit TipsView (Ctx& c);
    void setSelection (const juce::String& source, const juce::String& genre);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    Ctx& ctx;
    juce::String src, gen;
    juce::TextButton applyScale;
    int scaleType = -1;
};

// Mini piano roll of the generated phrase.
class PhraseView : public juce::Component
{
public:
    explicit PhraseView (Ctx& c) : ctx (c) {}
    void paint (juce::Graphics&) override;
    void update();
private:
    Ctx& ctx;
    int playing = -1;
    juce::String lastKey;
};

// Drag this into Cubase to get the phrase as a MIDI clip.
class MidiDragButton : public juce::TextButton
{
public:
    explicit MidiDragButton (Ctx& c);
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::File writeMidi() const;
private:
    Ctx& ctx;
    bool dragging = false;
};

class MelodyPanel : public Page
{
public:
    explicit MelodyPanel (Ctx& c);
    void resized() override;
    void update();
private:
    juce::TextButton activeButton { "Aus" }, prevVar { "<" }, nextVar { ">" };
    ParamCombo *style, *trigger, *length, *rate;
    Knob *variation, *density, *range, *glide, *octave, *tone, *sens, *level;
    PhraseView phrase;
    MidiDragButton midi;
    juce::Label styleLabel, triggerLabel, lengthLabel, rateLabel, status;
};

class PlayPage : public Page
{
public:
    explicit PlayPage (Ctx& c);
    void resized() override;
    void update();
    void refreshModulation();

private:
    PresetFinder finder;
    Section *padSec, *soundSec, *lowerSec;
    MacroPad pad;
    ParamCombo *figure, *figDiv;
    ParamToggle* figSync;
    Knob *figDepth, *figRate, *figCycles, *figWarp, *figDrift;
    juce::Label hint1, hint2, figLabel;
    MixKnob mix;
    Knob *stretch, *pitch, *rate, *spray, *blur, *room, *output;
    juce::TextButton tabMelody { "Melodie" }, tabTips { "Tipps" };
    MelodyPanel melody;
    TipsView tips;
    int lowerTab = 1, lastPreset = -2;
    juce::String lastPresetName;
    void showLower (int t);
};

// One line at the bottom explaining whatever is under the mouse.
class InfoBar : public juce::Component
{
public:
    void setText (const juce::String& title, const juce::String& body, const juce::String& more, const juce::String& less);
    void paint (juce::Graphics&) override;
private:
    juce::String t, b, m, l;
};
}
