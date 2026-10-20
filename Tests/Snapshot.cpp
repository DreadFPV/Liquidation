// Dev tool: renders every editor tab to PNG after feeding some audio, to check the layout.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 14;

    LiquidationProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.presets.load (preset);
    proc.apvts.getParameter (liq::id::macro1)->setValueNotifyingHost (0.45f);
    proc.apvts.getParameter (liq::id::macro2)->setValueNotifyingHost (0.6f);
    proc.apvts.getParameter (liq::id::animDepth)->setValueNotifyingHost (0.5f);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const int64_t n = (int64_t) b * 512 + i;
            const double t = (double) (n % 12000) / 48000.0;
            static const int notes[] { 57, 60, 64, 67, 69, 67, 64, 60 };
            const double hz = 440.0 * std::pow (2.0, (notes[(n / 12000) % 8] - 69) / 12.0);
            const float v = (float) (0.4 * std::exp (-t * 3.0) * std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) n / 48000.0));
            buf.setSample (0, i, v); buf.setSample (1, i, v * 0.8f);
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<LiquidationEditor*> (ed.get());
    if (editor == nullptr) return 1;

    // let timers run so displays update
    for (int tab = 0; tab < 4; ++tab)
    {
        for (auto* c : editor->getChildren()) juce::ignoreUnused (c);
        // click the tab button
        auto& content = *editor->getChildComponent (0);
        int found = 0;
        for (auto* c : content.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getRadioGroupId() == 1001 && found++ == tab) b->triggerClick();

        for (int f = 0; f < 20; ++f)
        {
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
        }
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto file = outDir.getChildFile ("tab" + juce::String (tab) + ".png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        png.writeImageToStream (img, os);
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
    }
    // lifecycle stress: re-prepare while the editor is open, close, reopen
    proc.prepareToPlay (96000.0, 256);
    for (int f = 0; f < 10; ++f) { proc.processBlock (buf, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }
    ed.reset();
    for (int k = 0; k < 3; ++k)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e2 (proc.createEditor());
        for (int f = 0; f < 5; ++f) { proc.processBlock (buf, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }
        proc.prepareToPlay (k % 2 ? 44100.0 : 48000.0, 1024);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }
    std::printf ("lifecycle OK\n");
    return 0;
}
