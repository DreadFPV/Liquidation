#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/LiqLookAndFeel.h"
#include "ui/Blocks.h"

class LiquidationEditor : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          private juce::Timer
{
public:
    explicit LiquidationEditor (LiquidationProcessor&);
    ~LiquidationEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int baseW = 1280, baseH = 800;

    // used by the snapshot tool
    void openDetails (int page) { overlay->open (page); }
    void closeDetails() { overlay->close(); }
    void setHelpMode (bool on) { topBar->help.setToggleState (on, juce::sendNotification); }
    void showHelpFor (juce::Component* c);

private:
    void timerCallback() override;
    bool addModulation (const juce::String& paramId, int source);
    void clearModulation (const juce::String& paramId);
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    LiquidationProcessor& proc;
    liq::ui::LiqLookAndFeel lnf;
    liq::ui::Ctx ctx;

    struct Content : public juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (liq::ui::col::bg); }
    } content;

    std::unique_ptr<liq::ui::TopBar> topBar;
    std::unique_ptr<liq::ui::SignalCanvas> canvas;
    std::unique_ptr<liq::ui::ModBar> modBar;
    std::unique_ptr<liq::ui::DetailOverlay> overlay;
    liq::ui::HelpCard helpCard;
    juce::TooltipWindow tooltips { this, 500 };
    juce::Component* helpTarget = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiquidationEditor)
};
