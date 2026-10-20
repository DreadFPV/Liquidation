#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/LiqLookAndFeel.h"
#include "ui/Panels.h"

class LiquidationEditor : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          private juce::Timer
{
public:
    explicit LiquidationEditor (LiquidationProcessor&);
    ~LiquidationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1180, baseH = 760;

private:
    void timerCallback() override;
    void showTab (int t);
    bool addModulation (const juce::String& paramId, int source);
    void clearModulation (const juce::String& paramId);

    LiquidationProcessor& proc;
    liq::ui::LiqLookAndFeel lnf;
    liq::ui::Ctx ctx;

    struct Content : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (liq::ui::col::bg); }
    } content;

    std::unique_ptr<liq::ui::TopBar> topBar;
    std::unique_ptr<liq::ui::GrainPage> grainPage;
    std::unique_ptr<liq::ui::ShapePage> shapePage;
    std::unique_ptr<liq::ui::EffectsPage> fxPage;
    std::unique_ptr<liq::ui::ModPage> modPage;
    std::unique_ptr<liq::ui::AnimatorPanel> animator;
    std::unique_ptr<liq::ui::ModBar> modBar;

    juce::OwnedArray<juce::TextButton> tabs;
    int currentTab = 0;
    juce::TooltipWindow tooltips { this, 600 };
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiquidationEditor)
};
