#include "PluginEditor.h"

using namespace liq;
using namespace liq::ui;

LiquidationEditor::LiquidationEditor (LiquidationProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      ctx { p, p.apvts,
            [this] (const juce::String& pid, int src) { return addModulation (pid, src); },
            [this] (const juce::String& pid) { clearModulation (pid); },
            [this] { overlay->open (3); },
            [this] (int page) { overlay->open (page); } }
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (content);

    topBar  = std::make_unique<TopBar> (ctx);
    canvas  = std::make_unique<SignalCanvas> (ctx);
    modBar  = std::make_unique<ModBar> (ctx);
    overlay = std::make_unique<DetailOverlay> (ctx);
    content.addAndMakeVisible (*topBar);
    content.addAndMakeVisible (*canvas);
    content.addAndMakeVisible (*modBar);
    content.addChildComponent (*overlay);
    content.addChildComponent (helpCard);
    helpCard.setInterceptsMouseClicks (false, false);

    modBar->onModulatorClicked = [this] (int m) { overlay->selectModulator (m); overlay->open (3); };
    modBar->onMatrix = [this] { overlay->open (3); };
    topBar->help.onClick = [this]
    {
        helpTarget = nullptr;
        if (topBar->help.getToggleState())
            helpCard.show ("Hilfe-Modus", "Fahre mit der Maus über einen Block, um zu sehen, was er macht.\n"_de
                                          "Signalfluss: links rein, rechts raus. A = deine Spur, B = Sidechain oder Modeler.\n"
                                          "Nochmal auf ? klicken beendet die Hilfe.",
                           topBar->help.getBounds().translated (0, 40), content.getLocalBounds().reduced (8));
        else
            helpCard.setVisible (false);
    };

    content.addMouseListener (this, true);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setResizeLimits ((int) (baseW * 0.6), (int) (baseH * 0.6), (int) (baseW * 1.6), (int) (baseH * 1.6));
    setSize (baseW, baseH);
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

LiquidationEditor::~LiquidationEditor()
{
    stopTimer();
    content.removeMouseListener (this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void LiquidationEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void LiquidationEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (juce::AffineTransform::scale (scale));
    topBar->setBounds (0, 0, baseW, 56);
    canvas->setBounds (0, 56, baseW, 656);
    modBar->setBounds (0, 712, baseW, 88);
    overlay->setBounds (0, 56, baseW, 656);
}

bool LiquidationEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && overlay->isVisible()) { overlay->close(); return true; }
    return false;
}

void LiquidationEditor::showHelpFor (juce::Component* c)
{
    // walk up to the next section that carries a help text
    juce::Component* target = nullptr;
    for (auto* x = c; x != nullptr && x != &content; x = x->getParentComponent())
    {
        if (auto* s = dynamic_cast<Section*> (x); s != nullptr && s->helpText.isNotEmpty()) { target = s; break; }
        if (x == modBar.get()) { target = x; break; }
    }
    if (target == helpTarget) return;
    helpTarget = target;
    if (target == nullptr) { helpCard.setVisible (false); return; }

    const auto bounds = content.getLocalArea (target->getParentComponent(), target->getBounds());
    if (target == modBar.get())
        helpCard.show ("Modulation",
                       "Macro 1/2 und Mod 1-4 sind Quellen. Ziehe eine Quelle auf einen Regler - der grüne Ring zeigt die Stärke. "_de
                       "Klick auf Mod 1-4 öffnet LFO, Sequencer oder Envelope. Rechtsklick auf einen Regler entfernt die Modulation."_de,
                       bounds, content.getLocalBounds().reduced (8));
    else if (auto* s = dynamic_cast<Section*> (target))
        helpCard.show (s->helpHeading, s->helpText, bounds, content.getLocalBounds().reduced (8));
}

void LiquidationEditor::mouseMove (const juce::MouseEvent& e)
{
    if (! topBar->help.getToggleState() || overlay->isVisible()) return;
    showHelpFor (e.eventComponent);
}

void LiquidationEditor::mouseExit (const juce::MouseEvent&) {}

bool LiquidationEditor::addModulation (const juce::String& paramId, int source)
{
    const int dst = modTargetIds().indexOf (paramId) + 1;
    if (dst <= 0 || source <= 0) return false;
    auto get = [this] (int s, const char* f) { return (int) std::lround (proc.apvts.getRawParameterValue (id::mx (s, f))->load()); };

    for (int s = 1; s <= numMatrixSlots; ++s)
        if (get (s, "Src") == source && get (s, "Dst") == dst) return true;

    for (int s = 1; s <= numMatrixSlots; ++s)
    {
        if (get (s, "Src") == 0 || get (s, "Dst") == 0)
        {
            auto set = [this, s] (const char* f, float real)
            {
                auto* p = proc.apvts.getParameter (id::mx (s, f));
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (real));
                p->endChangeGesture();
            };
            set ("Src", (float) source);
            set ("Dst", (float) dst);
            set ("Amt", 0.3f);
            return true;
        }
    }
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Mod Matrix voll",
                                            "Alle 16 Modulations-Slots sind belegt. Entferne einen in der Mod Matrix.");
    return false;
}

void LiquidationEditor::clearModulation (const juce::String& paramId)
{
    const int dst = modTargetIds().indexOf (paramId) + 1;
    for (int s = 1; s <= numMatrixSlots; ++s)
    {
        if ((int) std::lround (proc.apvts.getRawParameterValue (id::mx (s, "Dst"))->load()) != dst) continue;
        for (auto f : { "Src", "Dst", "Amt" })
            if (auto* p = proc.apvts.getParameter (id::mx (s, f)))
                p->setValueNotifyingHost (p->getDefaultValue());
    }
}

void LiquidationEditor::timerCallback()
{
    topBar->update();
    modBar->update();
    if (overlay->isVisible())
    {
        overlay->update();
        overlay->refreshModulation();
    }
    else
    {
        canvas->update();
        canvas->refreshModulation();
    }
    modBar->refreshModulation();
    if (! topBar->help.getToggleState() && helpCard.isVisible()) helpCard.setVisible (false);
}
