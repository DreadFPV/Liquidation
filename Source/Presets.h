#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>

namespace liq
{
class ModSystem;

// Helper used by factory preset definitions.
class PresetWriter
{
public:
    PresetWriter (juce::AudioProcessorValueTreeState& s) : state (s) {}

    void set (const juce::String& paramId, float realValue);
    void setOn (const juce::String& paramId, bool on) { set (paramId, on ? 1.0f : 0.0f); }
    void setChoice (const juce::String& paramId, int index) { set (paramId, (float) index); }
    // Route a modulation source to a target with a normalised amount (-1..1).
    void route (int slot, int source, const juce::String& targetId, float amount);
    // Convenience
    void freeRateMs (float ms);
    void syncedRate (int mode, const char* division);

private:
    juce::AudioProcessorValueTreeState& state;
};

struct FactoryPreset
{
    juce::String name, category;
    std::function<void (PresetWriter&)> apply;
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s);

    static const std::vector<FactoryPreset>& factory();

    juce::File userFolder() const;
    juce::Array<juce::File> userPresets() const;   // cached, refreshed by rescanUserPresets()
    void rescanUserPresets();

    // Combined list: factory first, then user presets ("User/<name>").
    juce::StringArray allNames() const;
    juce::StringArray allCategories() const;
    int numPresets() const;

    void load (int index);          // index into allNames()
    void loadFile (const juce::File& f);
    bool saveUser (const juce::String& name, const juce::ValueTree& fullState);

    int currentIndex() const noexcept { return current; }
    juce::String currentName() const;
    void setCurrentName (const juce::String& n) { currentNameOverride = n; }

    std::function<void (const juce::ValueTree&)> onLoadState;   // replace full state (user presets)
    std::function<void()> onBeforeFactoryLoad;                   // e.g. reset sequencer steps

private:
    void resetToDefaultsKeepingKey();

    juce::AudioProcessorValueTreeState& state;
    int current = 0;
    juce::String currentNameOverride;
    juce::Array<juce::File> userCache;
    bool userCacheValid = false;
};
}
