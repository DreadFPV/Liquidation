// Headless DSP checks for Liquidation: runs every factory preset on test signals and
// verifies the output is finite, audible, bounded and reasonably cheap to compute.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Quality.h"
#include <cstdio>
#include <cmath>

using namespace liq;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } } while (0)

namespace
{
    struct RunResult { float peak = 0, rms = 0; bool finite = true; double seconds = 0; };

    RunResult run (LiquidationProcessor& proc, double seconds, std::function<float (int64_t, int)> signal)
    {
        const double sr = 48000.0;
        const int block = 512;
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        RunResult r;
        double sumSq = 0; int64_t count = 0;
        const int64_t total = (int64_t) (seconds * sr);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int64_t pos = 0; pos < total; pos += block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buf.setSample (ch, i, signal (pos + i, ch));
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) r.finite = false;
                    r.peak = std::max (r.peak, std::abs (v));
                    if (pos > total / 2) { sumSq += v * v; ++count; }
                }
        }
        r.seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        r.rms = (float) std::sqrt (sumSq / std::max<int64_t> (1, count));
        return r;
    }

    float melody (int64_t n, int ch);
    float melodyMono (int64_t n) { return melody (n, 0); }
    float sine (int64_t n, double hz, float amp = 0.5f) { return amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * n / 48000.0); }

    // musical test signal: a little melody of plucked notes plus a kick every beat
    float melody (int64_t n, int ch)
    {
        static const int notes[] { 57, 60, 64, 67, 69, 67, 64, 60 };
        const int64_t noteLen = 12000;                 // 1/8 at 120 bpm
        const int idx = (int) ((n / noteLen) % 8);
        const double hz = 440.0 * std::pow (2.0, (notes[idx] - 69) / 12.0);
        const double t = (double) (n % noteLen) / 48000.0;
        float v = (float) (0.4 * std::exp (-t * 4.0) * std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) n / 48000.0));
        const double tk = (double) (n % 24000) / 48000.0;
        v += (float) (0.5 * std::exp (-tk * 25.0) * std::sin (2.0 * juce::MathConstants<double>::pi * (50.0 + 120.0 * std::exp (-tk * 40.0)) * tk));
        return ch == 0 ? v : v * 0.9f;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::printf ("Scale tests\n");
    {
        Scale s;
        s.set (Scale::presetMask (2, 9), 9, 440.0f);      // A minor
        CHECK (s.nearest (60.8f) == 60, "C#- snaps to C in A minor");
        CHECK (s.nearest (61.2f) == 62, "C#+ snaps to D in A minor");
        CHECK (s.nearest (63.0f) == 62 || s.nearest (63.0f) == 64, "D# snaps to D or E");
        CHECK (s.shiftDegrees (57, 2) == 60, "A + 2 degrees = C");
        CHECK (s.shiftDegrees (57, 7) == 69, "A + 7 degrees = octave");
        CHECK (s.degreeToSemis (4) == 7, "root + 4 degrees = fifth");
        CHECK (std::abs (s.midiToHz (69) - 440.0f) < 0.01f, "A4 = 440");
    }

    std::printf ("Processor tests\n");
    LiquidationProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    {
        auto r = run (proc, 3.0, [] (int64_t n, int) { return sine (n, 220.0); });
        std::printf ("  init on sine: peak %.3f rms %.3f (%.2fs CPU for 3s)\n", r.peak, r.rms, r.seconds);
        CHECK (r.finite, "init output finite");
        CHECK (r.rms > 0.05f, "init output audible");
        CHECK (r.peak < 4.0f, "init output bounded");
    }

    std::printf ("Pitch detection\n");
    {
        proc.presets.load (0);
        run (proc, 1.5, [] (int64_t n, int) { return sine (n, 329.63); });   // E4 = midi 64
        const float d = proc.engine.display.detectedPitch.load();
        std::printf ("  detected %.2f (expected 64)\n", d);
        CHECK (std::abs (d - 64.0f) < 0.3f, "pitch of E4 detected");
    }

    std::printf ("Factory presets\n");
    const auto& f = PresetManager::factory();
    double worst = 0;
    for (int i = 0; i < (int) f.size(); ++i)
    {
        proc.presets.load (i);
        // turn the macros up halfway so routed modulation is exercised
        proc.apvts.getParameter (id::macro1)->setValueNotifyingHost (0.6f);
        proc.apvts.getParameter (id::macro2)->setValueNotifyingHost (0.6f);
        if (f[(size_t) i].name.contains ("MIDI")) proc.engine.noteOn (57);
        auto r = run (proc, 4.0, melody);
        proc.engine.noteOff (57);
        const double load = r.seconds / 4.0 * 100.0;
        worst = std::max (worst, load);
        std::printf ("  %-22s peak %6.3f rms %6.3f cpu %5.1f%%\n", f[(size_t) i].name.toRawUTF8(), r.peak, r.rms, load);
        CHECK (r.finite, (f[(size_t) i].name + " finite").toRawUTF8());
        CHECK (r.peak < 6.0f, (f[(size_t) i].name + " bounded").toRawUTF8());
        CHECK (r.rms > 0.005f, (f[(size_t) i].name + " audible").toRawUTF8());
    }
    std::printf ("  worst single-core load %.1f%%\n", worst);

    std::printf ("Modes sweep\n");
    {
        proc.presets.load (0);
        auto set = [&] (const juce::String& pid, float real)
        { auto* p = proc.apvts.getParameter (pid); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
        for (int fm = 0; fm < fNumModes; ++fm)
        {
            set (id::fltOn, 1); set (id::fltMode, (float) fm); set (id::fltReso, 0.9f); set (id::fltDrive, 0.8f);
            auto r = run (proc, 1.0, melody);
            CHECK (r.finite && r.peak < 6.0f, ("filter mode " + filterModeNames()[fm]).toRawUTF8());
        }
        set (id::fltOn, 0);
        for (int fx = 0; fx < fxNumTypes; ++fx)
        {
            set (id::fx (1, "On"), 1); set (id::fx (1, "Type"), (float) fx); set (id::fx (1, "A"), 0.9f); set (id::fx (1, "C"), 0.9f);
            auto r = run (proc, 1.0, melody);
            CHECK (r.finite && r.peak < 6.0f, ("fx " + fxTypeNames()[fx]).toRawUTF8());
        }
        set (id::fx (1, "On"), 0);
        for (int rm = 0; rm < 6; ++rm)
        {
            set (id::rateMode, (float) rm); set (id::grainRate, 1.0f);
            auto r = run (proc, 1.0, melody);
            CHECK (r.finite && r.peak < 6.0f, ("rate mode " + rateModeNames()[rm] + " max").toRawUTF8());
        }
        // extreme stress: fastest grains + chord + 8 taps
        set (id::rateMode, rateFree); set (id::grainRate, 0.9f); set (id::chordMode, chordChord);
        for (int i = 1; i <= numIntervals; ++i) set (id::intervalOn (i), 1);
        set (id::dlyOn, 1); set (id::dlyTaps, 8); set (id::fltOn, 1); set (id::fltMode, fComb);
        auto r = run (proc, 2.0, melody);
        std::printf ("  stress: peak %.3f cpu %.1f%%\n", r.peak, r.seconds / 2.0 * 100.0);
        CHECK (r.finite && r.peak < 8.0f, "stress test bounded");
    }

    std::printf ("Quality (Init, wet only)\n");
    {
        LiquidationProcessor q;
        q.setPlayConfigDetails (2, 2, 48000.0, 512);
        q.prepareToPlay (48000.0, 512);
        q.presets.load (0);
        auto sine = [] (int64_t n) { return 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) n / 48000.0); };
        auto bass = [] (int64_t n) {
            static const int notes[] { 33, 33, 36, 31 };
            const double hz = 440.0 * std::pow (2.0, (notes[(n / 24000) % 4] - 69) / 12.0);
            double ph = std::fmod (hz * (double) n / 48000.0, 1.0);
            return 0.4f * (float) (2.0 * ph - 1.0) * (float) std::exp (-(double) (n % 24000) / 48000.0 * 2.0); };
        std::vector<float> in;
        auto out = liqtest::render (q, sine, 4.0, &in);
        auto r = liqtest::measure (in, out);
        std::printf ("  sine 220 Hz : residual %6.1f dB  ripple %5.2f dB  lag %d  level %5.1f dB\n", r.residualDb, r.ampRippleDb, r.lag, r.levelDb);
        CHECK (r.residualDb < -30.0, "Init is transparent on a sine (residual < -30 dB)");
        CHECK (r.ampRippleDb < 0.5, "Init has no tremolo on a sine (ripple < 0.5 dB)");
        in.clear();
        q.reset(); q.prepareToPlay (48000.0, 512); q.presets.load (0);
        out = liqtest::render (q, bass, 4.0, &in);
        r = liqtest::measure (in, out);
        std::printf ("  saw bass    : residual %6.1f dB  lag %d  level %5.1f dB\n", r.residualDb, r.lag, r.levelDb);
        CHECK (r.residualDb < -20.0, "Init is transparent on a bass line (residual < -20 dB)");
    }

    std::printf ("Morph engine\n");
    {
        auto setp = [] (LiquidationProcessor& pr, const juce::String& pid, float real)
        { auto* p = pr.apvts.getParameter (pid); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
        auto saw = [] (int64_t n) { return 0.3f * (float) (2.0 * std::fmod (110.0 * (double) n / 48000.0, 1.0) - 1.0); };

        // STFT round trip: morph on, crossfade fully at A -> must reproduce A
        {
            LiquidationProcessor q;
            q.setPlayConfigDetails (2, 2, 48000.0, 512);
            q.prepareToPlay (48000.0, 512);
            q.presets.load (0);
            setp (q, id::morphOn, 1); setp (q, id::morphX, 0); setp (q, id::morphY, 0); setp (q, id::morphSrc, srcModeler);
            std::vector<float> in;
            auto out = liqtest::render (q, saw, 3.0, &in);
            auto r = liqtest::measure (in, out);
            std::printf ("  STFT round trip   : residual %6.1f dB  lag %d (latency %d)\n", r.residualDb, r.lag, q.getLatencySamples());
            CHECK (r.residualDb < -40.0, "STFT reconstruction is transparent");
            CHECK (r.lag == q.getLatencySamples(), "reported latency matches the signal");
        }
        // every algorithm with the modeler: finite, audible, different from the input
        for (int algo = 0; algo < 5; ++algo)
        {
            LiquidationProcessor q;
            q.setPlayConfigDetails (2, 2, 48000.0, 512);
            q.prepareToPlay (48000.0, 512);
            q.presets.load (0);
            setp (q, id::morphOn, 1); setp (q, id::morphSrc, srcModeler); setp (q, id::morphAlgo, (float) algo);
            setp (q, id::morphX, 0.6f); setp (q, id::morphY, 1.0f); setp (q, id::formOn, 1); setp (q, id::formShift, 3.0f);
            std::vector<float> in;
            auto out = liqtest::render (q, melodyMono, 3.0, &in);
            auto r = liqtest::measure (in, out);
            bool finite = true; float pk = 0;
            for (float v : out) { finite = finite && std::isfinite (v); pk = std::max (pk, std::abs (v)); }
            std::printf ("  %-8s          : level %6.1f dB  change %6.1f dB  peak %.2f\n", morphAlgoNames()[algo].toRawUTF8(), r.levelDb, r.residualDb, pk);
            CHECK (finite && pk < 1.0f, ("morph " + morphAlgoNames()[algo] + " bounded").toRawUTF8());
            CHECK (r.levelDb > -30.0, ("morph " + morphAlgoNames()[algo] + " audible").toRawUTF8());
            CHECK (r.residualDb > -15.0, ("morph " + morphAlgoNames()[algo] + " changes the sound").toRawUTF8());
        }
    }

    std::printf ("Sidechain\n");
    {
        LiquidationProcessor q;
        auto layout = q.getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        CHECK (q.setBusesLayout (layout), "sidechain layout accepted");
        q.prepareToPlay (48000.0, 512);
        q.presets.load (0);
        auto setp = [&q] (const juce::String& pid, float real) { auto* p = q.apvts.getParameter (pid); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
        setp (id::morphOn, 1); setp (id::morphSrc, srcAux); setp (id::morphAlgo, morphBlend); setp (id::morphX, 1.0f); setp (id::morphY, 0.0f);
        juce::AudioBuffer<float> b (4, 512); juce::MidiBuffer m;
        double eOut = 0, eAux = 0;
        for (int64_t pos = 0; pos < 48000 * 3; pos += 512)
        {
            for (int k = 0; k < 512; ++k)
            {
                const float main = melodyMono (pos + k);
                const float aux = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) (pos + k) / 48000.0);
                b.setSample (0, k, main); b.setSample (1, k, main); b.setSample (2, k, aux); b.setSample (3, k, aux);
                if (pos > 48000) eAux += aux * aux;
            }
            q.processBlock (b, m);
            if (pos > 48000) for (int k = 0; k < 512; ++k) eOut += b.getSample (0, k) * b.getSample (0, k);
        }
        std::printf ("  X-Fade at B: output %.1f dB vs aux\n", 10.0 * std::log10 (eOut / eAux));
        CHECK (q.auxConnected.load(), "sidechain detected");
        CHECK (std::abs (10.0 * std::log10 (eOut / eAux)) < 2.0, "X-Fade at B outputs the sidechain signal");
    }

    std::printf ("Transients and limiter\n");
    {
        auto kickOnly = [] (int64_t n) {
            const double tk = (double) (n % 24000) / 48000.0;
            return (float) (0.8 * std::exp (-tk * 18.0) * std::sin (2.0 * juce::MathConstants<double>::pi * (45.0 * tk + 140.0 / 40.0 * (1.0 - std::exp (-tk * 40.0))))); };
        auto attackEnergy = [&] (bool tr)
        {
            LiquidationProcessor q;
            q.setPlayConfigDetails (2, 2, 48000.0, 512);
            q.prepareToPlay (48000.0, 512);
            q.presets.load (8);   // Endless Freeze smears the attack
            auto* p = q.apvts.getParameter (id::trOn); p->setValueNotifyingHost (tr ? 1.0f : 0.0f);
            q.apvts.getParameter (id::fx (4, "On"))->setValueNotifyingHost (0.0f);   // compare without the reverb
            std::vector<float> in;
            auto out = liqtest::render (q, kickOnly, 3.0, &in);
            const int lat = q.getLatencySamples();
            double e = 0;   // energy of the first 10 ms after each kick, compared with the input
            double ref = 0;
            for (int64_t k = 24000; k + 24000 < (int64_t) in.size() - lat; k += 24000)
                for (int i = 0; i < 480; ++i)
                {
                    const double d = out[(size_t) (k + i + lat)] - in[(size_t) (k + i)];
                    e += d * d; ref += (double) in[(size_t) (k + i)] * in[(size_t) (k + i)];
                }
            return 10.0 * std::log10 (e / ref);
        };
        const double off = attackEnergy (false), on = attackEnergy (true);
        std::printf ("  kick attack error: transients off %.1f dB, on %.1f dB\n", off, on);
        CHECK (on < off - 10.0, "transient keeper restores the attack");

        LiquidationProcessor q;
        q.setPlayConfigDetails (2, 2, 48000.0, 512);
        q.prepareToPlay (48000.0, 512);
        q.presets.load (0);
        auto* g = q.apvts.getParameter (id::inputGain); g->setValueNotifyingHost (g->convertTo0to1 (18.0f));
        auto loud = [] (int64_t n) { return 0.9f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (double) n / 48000.0); };
        auto out = liqtest::render (q, loud, 2.0);
        float pk = 0; for (float v : out) pk = std::max (pk, std::abs (v));
        std::printf ("  limiter: +18 dB input -> peak %.3f\n", pk);
        CHECK (pk <= 0.9662f, "limiter holds the ceiling");
    }

    std::printf ("Mono track\n");
    {
        LiquidationProcessor mono;
        auto layout = mono.getBusesLayout();
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
        CHECK (mono.setBusesLayout (layout), "mono -> mono layout accepted");
        mono.prepareToPlay (48000.0, 512);
        mono.presets.load (8);   // Endless Freeze
        juce::AudioBuffer<float> b (1, 512); juce::MidiBuffer m;
        double diff = 0, ref = 0; bool finite = true;
        for (int64_t pos = 0; pos < 48000 * 3; pos += 512)
        {
            for (int k = 0; k < 512; ++k) b.setSample (0, k, melody (pos + k, 0));
            juce::AudioBuffer<float> in; in.makeCopyOf (b);
            mono.processBlock (b, m);
            for (int k = 0; k < 512; ++k)
            {
                const float y = b.getSample (0, k);
                if (! std::isfinite (y)) finite = false;
                if (pos > 48000) { diff += (y - in.getSample (0, k)) * (y - in.getSample (0, k)); ref += in.getSample (0, k) * in.getSample (0, k); }
            }
        }
        std::printf ("  mono output differs from input by %.0f%%\n", 100.0 * std::sqrt (diff / ref));
        CHECK (finite && diff / ref > 0.1, "mono track is processed");
    }

    std::printf ("State round trip\n");
    {
        proc.presets.load (5);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        LiquidationProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = true;
        for (auto* p : proc.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (std::abs (rp->getValue() - other.apvts.getParameter (rp->getParameterID())->getValue()) > 1e-4f) same = false;
        }
        CHECK (same, "state restores all parameters");
        std::printf ("  parameters: %d\n", (int) proc.getParameters().size());
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
