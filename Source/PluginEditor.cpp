#include "PluginEditor.h"

using namespace liq;
using namespace liq::ui;

LiquidationEditor::LiquidationEditor (LiquidationProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      ctx { p, p.apvts,
            [this] (const juce::String& pid, int src) { return addModulation (pid, src); },
            [this] (const juce::String& pid) { clearModulation (pid); },
            [this] { showTab (3); } }
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (content);

    topBar    = std::make_unique<TopBar> (ctx);
    grainPage = std::make_unique<GrainPage> (ctx);
    shapePage = std::make_unique<ShapePage> (ctx);
    fxPage    = std::make_unique<EffectsPage> (ctx);
    modPage   = std::make_unique<ModPage> (ctx);
    animator  = std::make_unique<AnimatorPanel> (ctx);
    modBar    = std::make_unique<ModBar> (ctx);

    for (juce::Component* c : { (juce::Component*) topBar.get(), (juce::Component*) grainPage.get(),
                                (juce::Component*) shapePage.get(), (juce::Component*) fxPage.get(),
                                (juce::Component*) modPage.get(), (juce::Component*) animator.get(),
                                (juce::Component*) modBar.get() })
        content.addAndMakeVisible (c);

    const char* names[] { "GRAIN", "FILTER  IMAGER  DELAY", "EFFECTS", "MODULATION" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setRadioGroupId (1001);
        b->setClickingTogglesState (true);
        b->onClick = [this, i] { showTab (i); };
        content.addAndMakeVisible (b);
    }

    modBar->onModulatorClicked = [this] (int m) { modPage->selectModulator (m); showTab (3); modBar->setSelected (m); };
    modBar->onMatrix = [this] { showTab (3); };
    modBar->setSelected (0);

    fxPage->update();
    showTab (0);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setResizeLimits ((int) (baseW * 0.65), (int) (baseH * 0.65), (int) (baseW * 1.8), (int) (baseH * 1.8));
    setSize (baseW, baseH);
    startTimerHz (30);
}

LiquidationEditor::~LiquidationEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void LiquidationEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void LiquidationEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (juce::AffineTransform::scale (scale));

    topBar->setBounds (0, 0, baseW, 74);
    int x = 10;
    for (auto* t : tabs)
    {
        const int w = t->getButtonText().length() > 12 ? 190 : 120;
        t->setBounds (x, 80, w, 26);
        x += w + 6;
    }
    const juce::Rectangle<int> pageArea (10, 112, 890, baseH - 112 - 78);
    for (juce::Component* pg : { (juce::Component*) grainPage.get(), (juce::Component*) shapePage.get(),
                                 (juce::Component*) fxPage.get(), (juce::Component*) modPage.get() })
        pg->setBounds (pageArea);
    animator->setBounds (908, 80, baseW - 908 - 10, baseH - 80 - 78);
    modBar->setBounds (0, baseH - 70, baseW, 70);
}

void LiquidationEditor::showTab (int t)
{
    currentTab = juce::jlimit (0, 3, t);
    grainPage->setVisible (currentTab == 0);
    shapePage->setVisible (currentTab == 1);
    fxPage->setVisible (currentTab == 2);
    modPage->setVisible (currentTab == 3);
    tabs[currentTab]->setToggleState (true, juce::dontSendNotification);
}

bool LiquidationEditor::addModulation (const juce::String& paramId, int source)
{
    const int dst = modTargetIds().indexOf (paramId) + 1;
    if (dst <= 0 || source <= 0) return false;
    auto get = [this] (int s, const char* f) { return (int) std::lround (proc.apvts.getRawParameterValue (id::mx (s, f))->load()); };

    for (int s = 1; s <= numMatrixSlots; ++s)
        if (get (s, "Src") == source && get (s, "Dst") == dst) return true;   // already routed

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
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Mod matrix full",
                                            "All 16 modulation slots are in use. Clear one in the Modulation tab.");
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
    ++frame;
    topBar->update();
    modBar->update();
    animator->update();
    switch (currentTab)
    {
        case 0: grainPage->update(); break;
        case 1: shapePage->update(); break;
        case 2: fxPage->update(); break;
        case 3: modPage->update(); break;
        default: break;
    }
    // modulation rings: visible page + always-visible panels
    juce::Component* pages[] { grainPage.get(), shapePage.get(), fxPage.get(), modPage.get() };
    if (auto* pg = dynamic_cast<Page*> (pages[currentTab])) pg->refreshModulation();
    animator->refreshModulation();
    topBar->refreshModulation();
}
