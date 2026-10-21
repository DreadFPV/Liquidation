// Dev tool: renders the editor (main view, detail pages, help mode) to PNG files after feeding audio,
// then stresses the editor lifecycle. Usage: LiquidationSnapshot <outDir> [presetIndex]
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;

    LiquidationProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.presets.load (preset);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    int64_t pos = 0;
    auto feed = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i, ++pos)
            {
                const double t = (double) (pos % 12000) / 48000.0;
                static const int notes[] { 57, 60, 64, 67, 69, 67, 64, 60 };
                const double hz = 440.0 * std::pow (2.0, (notes[(pos / 12000) % 8] - 69) / 12.0);
                float v = (float) (0.4 * std::exp (-t * 3.0) * std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) pos / 48000.0));
                const double tk = (double) (pos % 24000) / 48000.0;
                v += (float) (0.5 * std::exp (-tk * 22.0) * std::sin (2.0 * juce::MathConstants<double>::pi * (50.0 * tk + 3.0 * (1.0 - std::exp (-tk * 40.0)))));
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
            }
            proc.processBlock (buf, midi);
        }
    };
    feed (400);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<LiquidationEditor*> (ed.get());
    if (editor == nullptr) return 1;
    editor->setVisible (true);
    {
        const auto names = proc.presets.allNames();
        for (int i = 0; i < names.size(); ++i) std::printf ("%d:%s ", i, names[i].toRawUTF8());
        std::printf ("\n");
    }

    auto pump = [&] (int frames)
    {
        for (int f = 0; f < frames; ++f) { feed (3); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }
    };
    auto shot = [&] (const juce::String& name)
    {
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = outDir.getChildFile (name + ".png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
    };

    pump (25);
    editor->setPage (0);
    pump (10);
    shot ("play");
    {
        const auto names = proc.presets.allNames();
        for (int i = 0; i < names.size(); ++i)
            if (names[i].contains ("Goa Lead")) { proc.presets.load (i); break; }
        pump (40);
        if (auto* c = editor->getComponentAt (900, 330)) editor->updateInfo (c);
        pump (3);
        shot ("play_melody");
    }
    editor->setPage (1);
    pump (10);
    shot ("main");
    for (int page = 0; page < 4; ++page)
    {
        editor->openDetails (page);
        pump (12);
        shot ("details" + juce::String (page));
    }
    editor->closeDetails();
    editor->setHelpMode (true);
    pump (5);
    // hover the morph block
    if (auto* c = editor->getComponentAt (420, 200)) editor->showHelpFor (c);
    pump (5);
    shot ("help");
    editor->setHelpMode (false);

    // lifecycle stress: re-prepare while the editor is open, close, reopen
    proc.prepareToPlay (96000.0, 256);
    pump (10);
    ed.reset();
    for (int k = 0; k < 3; ++k)
    {
        std::unique_ptr<juce::AudioProcessorEditor> e2 (proc.createEditor());
        pump (5);
        proc.prepareToPlay (k % 2 ? 44100.0 : 48000.0, 1024);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }
    std::printf ("lifecycle OK\n");
    return 0;
}
