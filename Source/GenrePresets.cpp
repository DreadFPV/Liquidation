// Genre presets (genre x source), "Kick -> melody" presets, and the start-up tips.
#include "PresetHelpers.h"

namespace liq
{
// ============================================================================
// Genres and sources
// ============================================================================
namespace
{
    juce::String de (const char* s) { return juce::String::fromUTF8 (s); }

    struct Profile
    {
        const char* name;
        const char* bpm;
        int scaleType;
        const char* scaleHint;
        const char* character;
        const char* grid;        // main rhythmic division
        const char* stab;        // offbeat / stab division
        const char* echo;        // delay division
        int melody;              // MelodyStyle for leads
        int bass;                // MelodyStyle for bass lines
        float dark;              // 0 bright .. 1 dark / dissonant
        float energy;            // 0 calm .. 1 driving
        float space;             // 0 dry .. 1 wide
        bool psy;                // 4/4 psy family (kick on every beat)
    };

    const std::vector<Profile>& profiles()
    {
        static const std::vector<Profile> p {
            { "Goa", "138-148", 13, "Phrygisch-Dominant oder Harmonisch Moll",
              "Melodisch und orientalisch: lange Lead-Linien, Ping-Pong-Delays, viel Bewegung im Filter.",
              "1/16", "1/8", "1/8D", mlGoaLead, mlRollingBass, 0.3f, 0.8f, 0.55f, true },
            { "Full-On", "143-148", 6, "Phrygisch oder Moll",
              "Druck und Energie: rollende 1/16-Bässe, schnelle Arps, kurze Effekte zwischen den Kicks.",
              "1/16", "1/8", "1/8D", mlRollingArp, mlRollingBass, 0.25f, 1.0f, 0.4f, true },
            { "Psytrance", "140-145", 6, "Phrygisch",
              "Hypnotisch: Acid-Linien, Gates, wenige Töne, viel Filterbewegung.",
              "1/16", "1/8", "1/8D", mlAcid, mlRollingBass, 0.45f, 0.85f, 0.45f, true },
            { "Progressive", "132-138", 2, "Moll oder Dorisch",
              "Tief und räumlich: Galopp-Bass, Hooks, lange Hallfahnen, langsame Entwicklung.",
              "1/16", "1/8", "1/8D", mlHook, mlRollingBass, 0.3f, 0.55f, 0.7f, true },
            { "Darkpsy", "148-165", 9, "Lokrisch, Phrygisch oder chromatisch",
              "Dunkel und verdreht: Halbtonschritte, Kammfilter, metallische Splitter, wenig Hall.",
              "1/16", "1/8", "1/8D", mlDarkPulse, mlRollingBass, 0.9f, 0.9f, 0.35f, true },
            { "Hi-Tech", "180-200", 0, "Chromatisch - Tonart spielt kaum eine Rolle",
              "Extrem schnell und detailreich: 1/32-Splitter, Bitcrusher, Glitches.",
              "1/32", "1/16", "1/16D", mlDarkPulse, mlRollingBass, 0.7f, 1.0f, 0.3f, true },
            { "Drum & Bass", "170-176", 2, "Moll oder Dorisch",
              "Rollende Breaks, Reese-Bass, Atmosphären, Stabs auf dem Offbeat.",
              "1/16", "1/8", "1/8D", mlHook, mlReese, 0.35f, 0.8f, 0.6f, false },
            { "Neurofunk", "172-176", 6, "Moll oder Phrygisch",
              "Technisch und aggressiv: sprechende Bässe, Formanten, harte Schnitte.",
              "1/16", "1/8", "1/16D", mlStabs, mlReese, 0.85f, 0.9f, 0.3f, false },
            { "Jungle", "160-170", 5, "Dorisch oder Moll",
              "Zerhackte Breaks, Sub-Bass, warme Akkorde, Dub-Delays.",
              "1/16", "1/8", "1/4D", mlChords, mlReese, 0.3f, 0.7f, 0.7f, false },
        };
        return p;
    }

    int div (const char* name) { return defaultDivisionIndex (name); }
}

const std::vector<GenreInfo>& genres()
{
    static const std::vector<GenreInfo> g = []
    {
        std::vector<GenreInfo> v;
        for (auto& p : profiles())
            v.push_back ({ de (p.name), de (p.bpm), de (p.scaleHint), de (p.character), p.scaleType });
        return v;
    }();
    return g;
}

juce::StringArray sourceNames()
{
    return { "Kick", "Bass", "Drums", "Synth", "Melodie", "Gitarre", "Vocal", "Pad/FX" };
}

juce::String sourceTips (const juce::String& source)
{
    if (source == "Kick")
        return de ("Liquidation direkt auf die Kick legen.\n"
                   "1. Ein 'Kick -> Melodie'-Preset laden: daraus entsteht eine fertige Melodie oder ein Bass in deiner Tonart.\n"
                   "2. Variante (Melodie-Panel) durchklicken, bis die Linie passt. Dichte = mehr / weniger Noten.\n"
                   "3. 'MIDI ziehen' legt die Melodie als MIDI-Spur in Cubase ab - dann mit einem eigenen Synth spielen.\n"
                   "Wichtig: Transients bleiben an, damit die Kick ihren Punch behält. Main In im Mixer bleibt bei 0 dB.");
    if (source == "Bass")
        return de ("Für Bass-Spuren gilt: Tiefen bleiben mono und sauber.\n"
                   "1. Preset deines Genres laden, Mix erst auf 50 % stellen.\n"
                   "2. Pad nach rechts = mehr Bewegung im Filter, nach oben = mehr Effekt.\n"
                   "3. Spray und Blur niedrig lassen: sie machen Bässe matschig.\n"
                   "Tipp: Mit der Kick als Sidechain und 'Spectral Duck' macht der Bass nur dort Platz, wo die Kick spielt.");
    if (source == "Drums")
        return de ("Ideal für Loops und Breaks.\n"
                   "1. Scrub- und Stutter-Presets schneiden den Loop im Takt neu zusammen.\n"
                   "2. Loop Length (in der Grain Engine) = wie viel vom Loop zur Verfügung steht.\n"
                   "3. Für Fills nur kurz aktivieren: den Mix automatisieren.\n"
                   "Tipp: Transients an, wenn die Schläge knackig bleiben sollen.");
    if (source == "Synth")
        return de ("Für Leads, Arps und Pads aus dem Synth.\n"
                   "1. Tonart oben richtig einstellen - Arp, Chord und Shimmer folgen ihr.\n"
                   "2. Pad bewegen: die Presets legen Filter und Effekt auf die zwei Achsen.\n"
                   "3. Delay-Zeiten sind im Songtempo. In Psy-Genres oft 1/8 punktiert (3/16).\n"
                   "Tipp: Mehrere Instanzen mit unterschiedlichen Presets auf Gruppen-Sends klingen sehr breit.");
    if (source == "Melodie")
        return de ("Für eine bestehende Melodie (Lead, Piano, Pluck).\n"
                   "1. Harmonizer-Presets legen Terz und Quinte dazu - immer in deiner Tonart.\n"
                   "2. Grain Pitch transponiert in Tonleiter-Stufen, nicht in Halbtönen.\n"
                   "3. Pitch Mode 'Scale' + Retune 'Hard' korrigiert schiefe Töne.\n"
                   "Tipp: Die Tonart muss stimmen, sonst klingen die zusätzlichen Stimmen falsch.");
    if (source == "Gitarre")
        return de ("Gitarre wird zu Fläche, Shimmer oder Arp.\n"
                   "1. Freeze- und Shimmer-Presets für Intros und Breaks.\n"
                   "2. Akkorde: Pitch Mode 'Free' wählen, da die Tonerkennung nur einzelne Töne sicher erkennt.\n"
                   "3. Reverse-Presets klingen auf Akkord-Anschlägen am besten.\n"
                   "Tipp: Main In im Mixer leise dazu - so bleibt die Gitarre erkennbar.");
    if (source == "Vocal")
        return de ("Für Gesang, Sprache und Samples.\n"
                   "1. Formant-Presets ändern die Stimmfarbe, ohne die Tonhöhe.\n"
                   "2. Harmonizer erzeugt Chöre in der Tonart.\n"
                   "3. Chop-Presets zerschneiden Phrasen im Takt.\n"
                   "Tipp: Für Psy-Vocal-Samples: Formant Down + Delay 3/16 + Reverb.");
    if (source == "Pad/FX")
        return de ("Für Atmosphären, Risers und Effekte.\n"
                   "1. Freeze-Presets machen aus kurzen Sounds lange Flächen.\n"
                   "2. Pad langsam bewegen oder den Animator (Figur) laufen lassen.\n"
                   "3. Reverb im Mixer für mehr Raum.\n"
                   "Tipp: Kurze FX-Samples + Freeze = eigene Drones in deiner Tonart.");
    return de ("Wähle links eine Quelle (was liegt auf der Spur?) und ein Genre.\n"
               "Die Liste zeigt passende Presets - einfach durchklicken, während Cubase abspielt.\n"
               "Das grosse Pad steuert zwei Klang-Eigenschaften gleichzeitig; die Achsen-Namen zeigen welche.\n"
               "Maus über einen Regler: unten steht, was er macht.");
}

// ============================================================================
// Kick -> melody presets
// ============================================================================
namespace presets
{
namespace
{
    // melody setup shared by the kick presets: kick stays, melody from the modeler
    void kickMelody (PresetWriter& w, int style, int trigger, const char* rate, int octave, float tone, float dens, int length = 1)
    {
        w.setChoice (id::mdlMode, mdlMelody);
        w.setChoice (id::mlStyle, style);
        w.setChoice (id::mlTrigger, trigger);
        w.setChoice (id::mdlRate, div (rate));
        w.setChoice (id::mlLength, length);
        w.set (id::mdlOctave, (float) octave);
        w.set (id::mdlTone, tone);
        w.set (id::mlDensity, dens);
        w.set (id::mixWet, 0.0f);
        w.set (id::mixDry, 1.0f);
        w.set (id::mixModel, 0.6f);
        w.setOn (id::trOn, true);
    }

    void variationMacros (PresetWriter& w, const char* what)
    {
        w.route (1, M1, id::mlVar, 0.3f);
        w.route (2, M2, id::mdlTone, 0.45f);
        w.route (3, M2, id::mlDensity, 0.25f);
        w.macros ("Variante", de ("Pad nach rechts: andere Melodie-Variante (") + de (what) + ")",
                  "Helligkeit", de ("Pad nach oben: heller, mehr Noten"));
    }
}

void appendKickMelodyPresets (std::vector<FactoryPreset>& list)
{
    auto add = [&list] (const char* name, const char* desc, std::function<void (PresetWriter&)> f)
    {
        FactoryPreset p { de (name), "Kick -> Melodie", std::move (f) };
        p.genre = "Alle";
        p.source = "Kick";
        p.description = de (desc);
        list.push_back (p);
    };

    add ("Kick -> Goa Lead",
         "Zur Kick spielt eine fertige Goa-Lead-Linie in deiner Tonart (1/16). Variante wechselt die Melodie.",
         [] (PresetWriter& w) {
             kickMelody (w, mlGoaLead, trigGrid, "1/16", 0, 0.55f, 0.75f, 2);
             fx (w, 3, fxDelay, 0.3f, 0.78f, 0.45f, 0.8f, 0.6f);
             variationMacros (w, "Goa Lead"); });
    add ("Kick -> Rolling Bass",
         "Psytrance-Bass, der automatisch zwischen deinen Kicks rollt - folgt der echten Kick, auch bei Breaks.",
         [] (PresetWriter& w) {
             kickMelody (w, mlRollingBass, trigBetweenKicks, "1/16", 0, 0.35f, 0.8f, 2);
             w.set (id::mixModel, 0.9f);
             variationMacros (w, "Bass-Fills"); });
    add ("Kick -> Acid Line",
         "303-artige Acid-Linie mit Slides und Akzenten, im 1/16-Raster zur Kick.",
         [] (PresetWriter& w) {
             kickMelody (w, mlAcid, trigGrid, "1/16", 0, 0.45f, 0.7f, 1);
             variationMacros (w, "Acid"); });
    add ("Kick -> Step Melody",
         "Jeder Kick-Schlag spielt die nächste Note einer Melodie - die Kick wird zum Melodie-Takt.",
         [] (PresetWriter& w) {
             kickMelody (w, mlHook, trigKickStep, "1/16", 1, 0.6f, 0.8f, 2);
             w.set (id::mlGlide, 0.35f);
             variationMacros (w, "Hook"); });
    add ("Kick becomes Melody",
         "Die Kick selbst wird zur Melodie: Lautstärke und Rhythmus kommen von der Kick, die Töne vom Generator.",
         [] (PresetWriter& w) {
             kickMelody (w, mlGoaLead, trigKickGate, "1/16", 0, 0.6f, 0.9f, 2);
             w.set (id::mixModel, 1.0f);
             w.set (id::mixDry, 0.5f);
             variationMacros (w, "Melodie"); });
    add ("Kick -> Offbeat Stabs",
         "Akkord-Stabs auf dem Offbeat zwischen den Kicks - Progressive- und House-Feeling.",
         [] (PresetWriter& w) {
             kickMelody (w, mlStabs, trigGrid, "1/16", 0, 0.5f, 0.7f, 2);
             fx (w, 4, fxReverb, 0.3f, 0.6f, 0.5f, 1.0f, 0.1f);
             variationMacros (w, "Akkordfolge"); });
    add ("Kick -> Dark Pulse",
         "Dunkler Halbton-Puls zwischen den Kicks - Darkpsy-Grundlage.",
         [] (PresetWriter& w) {
             kickMelody (w, mlDarkPulse, trigBetweenKicks, "1/16", 0, 0.4f, 0.8f, 1);
             variationMacros (w, "Puls"); });
    add ("Kick -> Reese Steps",
         "DnB: Jeder Kick schaltet den Reese-Bass auf den nächsten Ton - Bass folgt dem Break.",
         [] (PresetWriter& w) {
             kickMelody (w, mlReese, trigKickStep, "1/16", -1, 0.4f, 0.6f, 2);
             w.set (id::mlGlide, 0.5f);
             variationMacros (w, "Reese"); });
}

// ============================================================================
// Genre presets: one per genre and source
// ============================================================================
void appendGenrePresets (std::vector<FactoryPreset>& list)
{
    for (const auto& g : profiles())
    {
        const juce::String gname = de (g.name);
        auto add = [&] (const juce::String& source, const juce::String& title, const juce::String& desc,
                        std::function<void (PresetWriter&)> f)
        {
            FactoryPreset p { gname + " - " + title, gname, std::move (f) };
            p.genre = gname;
            p.source = source;
            p.description = desc;
            list.push_back (p);
        };
        const Profile prof = g;   // captured by value below

        // ------------------------------------------------------------------ Kick
        add ("Kick", "Kick Tone", de ("Die Kick bekommt einen tiefen Ton in der Tonart (Morph mit Drone). Anschlag bleibt original."),
             [prof] (PresetWriter& w) {
                 w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlDrone);
                 w.set (id::mdlOctave, prof.psy ? -1.0f : -2.0f); w.set (id::mdlTone, 0.3f + 0.3f * (1.0f - prof.dark));
                 w.setChoice (id::morphAlgo, morphBlend); w.set (id::morphX, 0.4f);
                 w.setOn (id::trOn, true); w.set (id::trLength, 25.0f);
                 w.route (1, M1, id::morphX, 0.4f); w.route (2, M2, id::trLength, 0.35f);
                 w.macros ("Ton", de ("rechts: mehr Ton, weniger Original"), "Klick", de ("oben: längerer Original-Anschlag")); });

        add ("Kick", prof.psy ? "Kick -> Bassline" : "Kick -> Reese", de (prof.psy
                 ? "Fertige Bassline, die zwischen deinen Kicks rollt - in Tonart und Tempo."
                 : "Reese-Bass, der bei jedem Kick einen Ton weiterschaltet."),
             [prof] (PresetWriter& w) {
                 kickMelody (w, prof.bass, prof.psy ? trigBetweenKicks : trigKickStep, "1/16",
                             prof.psy ? 0 : -1, 0.3f + 0.3f * prof.energy, prof.psy ? 0.6f + 0.3f * prof.energy : 0.6f, 2);
                 if (prof.bass == mlReese) w.set (id::mlGlide, 0.5f);
                 variationMacros (w, "Bass"); });

        add ("Kick", "Kick -> Lead", de ("Zur Kick läuft eine fertige Lead-Melodie im Stil des Genres. 'MIDI ziehen' übernimmt sie nach Cubase."),
             [prof] (PresetWriter& w) {
                 kickMelody (w, prof.melody, trigGrid, prof.grid, 0, 0.45f + 0.25f * (1.0f - prof.dark), 0.6f + 0.3f * prof.energy, 2);
                 fx (w, 3, fxDelay, 0.25f + 0.2f * prof.space, prof.space > 0.6f ? 0.9f : 0.78f, 0.4f, 0.8f, 0.55f);
                 if (prof.space > 0.5f) fx (w, 4, fxReverb, 0.2f + 0.2f * prof.space, 0.7f, 0.5f, 1.0f, 0.1f);
                 variationMacros (w, "Lead"); });

        // ------------------------------------------------------------------ Bass
        add ("Bass", "Bass Motion", de ("Bewegung im Bass: Filter pro Grain im Takt, Tiefen bleiben mono."),
             [prof] (PresetWriter& w) {
                 w.syncedRate (rateStraight, prof.grid); w.set (id::sizeRatio, 1.1f);
                 w.setOn (id::fltOn, true);
                 if (prof.dark > 0.75f) { w.setChoice (id::fltMode, fComb); w.setOn (id::fltScale, true); w.set (id::fltCutoff, 110.0f); w.set (id::fltReso, 0.6f); w.set (id::fltDamp, 0.5f); }
                 else { w.setChoice (id::fltMode, fLowpass); w.set (id::fltCutoff, 500.0f + 900.0f * (1.0f - prof.dark)); w.set (id::fltReso, 0.45f); }
                 w.set (id::fltModDepth, 0.5f); w.set (id::fltModRate, 1.0f); w.set (id::fltDrive, 0.25f);
                 w.setOn (id::monoLows, true); w.setOn (id::trOn, true);
                 w.dryWet (0.6f);
                 w.route (1, M1, id::fltCutoff, 0.35f); w.route (2, M2, id::mixWet, 0.3f); w.route (3, M2, id::fltModDepth, 0.3f);
                 w.macros ("Filter", "rechts: heller", "Bewegung", "oben: mehr Effekt und Filter-Bewegung"); });

        add ("Bass", prof.psy ? "Bass Talk" : "Neuro Talk", de ("Sprechender Bass: Vokal-Klangfarbe und Formanten auf deiner Bass-Spur."),
             [prof] (PresetWriter& w) {
                 w.setOn (id::morphOn, true); w.setChoice (id::morphSrc, srcModeler); w.setChoice (id::mdlMode, mdlVowel);
                 w.setChoice (id::morphAlgo, morphImprint); w.set (id::morphX, 0.55f); w.set (id::morphDetail, 0.6f);
                 w.set (id::mdlMotion, 0.4f + 0.4f * prof.energy);
                 w.setOn (id::formOn, true); w.set (id::formShift, prof.dark > 0.6f ? -3.0f : 2.0f);
                 w.setOn (id::monoLows, true);
                 w.set (id::mmDepth, 0.3f); w.setOn (id::mmSync, true); w.setChoice (id::mmDiv, div (prof.psy ? "1 bar" : "1/2"));
                 w.route (1, M1, id::mdlMotion, 0.5f); w.route (2, M2, id::formShift, 0.25f);
                 w.macros ("Sprechen", "rechts: schnellere Vokal-Bewegung", "Formant", "oben: hellere Stimme"); });

        // ------------------------------------------------------------------ Drums
        add ("Drums", prof.psy ? "Loop Stutter" : "Break Chop", de (prof.psy
                 ? "Stutter im 1/16-Raster: ein Sequencer springt durch den Loop."
                 : "Break wird im Takt neu zerschnitten, einzelne Stücke rückwärts - Amen-Chop-Feeling."),
             [prof] (PresetWriter& w) {
                 w.setChoice (id::playMode, playScrub); w.setChoice (id::scrubMode, prof.psy ? 2 : 2);
                 w.setChoice (id::loopLength, prof.psy ? 2 : 3);
                 w.syncedRate (rateStraight, prof.grid); w.set (id::sizeRatio, 1.3f);
                 w.set (id::revProb, prof.psy ? 0.1f : 0.25f);
                 w.setChoice (id::mod (3, "Type"), modSequencer); w.setChoice (id::mod (3, "Div"), div (prof.psy ? "1 bar" : "2 bars"));
                 w.set (id::mod (3, "Steps"), prof.psy ? 8.0f : 16.0f);
                 w.setOn (id::trOn, true);
                 w.route (1, Mod3, id::scrubPos, 1.0f); w.route (2, M1, id::revProb, 0.5f);
                 w.dryWet (0.75f);
                 w.macros ("Rückwärts", "rechts: mehr Stücke rückwärts", "Original", "oben: Original-Loop dazumischen");
                 w.route (3, M2, id::mixDry, 0.6f); });

        add ("Drums", "Drum Space", de ("Grain-Delay mit Tonhöhen-Mustern und Raum auf den Drums - für Breaks und Übergänge."),
             [prof] (PresetWriter& w) {
                 w.syncedRate (rateStraight, prof.stab); w.set (id::sizeRatio, 0.9f);
                 w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true); w.setChoice (id::dlyDiv, div (prof.echo));
                 w.set (id::dlyTaps, 3.0f); w.setChoice (id::dlyPattern, prof.dark > 0.6f ? shChaos : shRamp);
                 w.set (id::dlyShift, prof.dark > 0.6f ? 1.0f : 2.0f); w.set (id::dlySpace, 0.3f + 0.4f * prof.space);
                 w.set (id::dlyHP, 300.0f); w.set (id::dlyMix, 0.5f);
                 if (prof.dark > 0.6f) { w.setOn (id::fltOn, true); w.setChoice (id::fltMode, fReducer); w.set (id::fltCutoff, 7000.0f); w.set (id::fltBits, 8.0f); }
                 w.setOn (id::trOn, true);
                 w.dryWet (0.5f);
                 w.route (1, M1, id::dlyFeedback, 0.5f); w.route (2, M2, id::dlySpace, 0.5f);
                 w.macros ("Echo", "rechts: längere Echos", "Raum", "oben: mehr Hall auf den Echos"); });

        // ------------------------------------------------------------------ Synth
        add ("Synth", "Synth Arp", de ("Jeder Synth-Ton wird zum Arpeggio in der Tonart, mit Delay im Genre-Tempo."),
             [prof] (PresetWriter& w) {
                 w.syncedRate (rateStraight, prof.grid); w.set (id::sizeRatio, 1.2f);
                 w.setChoice (id::chordMode, chordArp);
                 if (prof.dark > 0.75f) intervals (w, { 0, 1, 4, 7 }); else intervals (w, { 0, 2, 4, 7 });
                 w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true); w.setChoice (id::dlyDiv, div (prof.echo));
                 w.set (id::dlyTaps, 2.0f); w.set (id::dlyFeedback, 0.3f + 0.2f * prof.space); w.set (id::dlyMix, 0.35f);
                 w.dryWet (0.8f);
                 w.route (1, M1, id::arpRand, 0.8f); w.route (2, M2, id::dlyMix, 0.4f); w.route (3, M2, id::dlyFeedback, 0.3f);
                 w.macros ("Zufall", "rechts: Arp-Reihenfolge zufälliger", "Echo", "oben: mehr Delay"); });

        add ("Synth", prof.dark > 0.6f ? "Synth Shred" : "Synth Gate", de (prof.dark > 0.6f
                 ? "Metallische Splitter: Kammfilter in Tonart + Bitcrusher."
                 : "Trance-Gate mit Filter-Sweep - klassisch für Leads und Pads."),
             [prof] (PresetWriter& w) {
                 w.syncedRate (rateStraight, prof.grid); w.set (id::sizeRatio, 0.7f); w.set (id::winShape, 0.6f);
                 w.setOn (id::fltOn, true);
                 if (prof.dark > 0.6f) { w.setChoice (id::fltMode, fComb); w.setOn (id::fltScale, true); w.setOn (id::fltRandom, true);
                                         w.set (id::fltCutoff, 300.0f); w.set (id::fltReso, 0.8f); w.set (id::fltModDepth, 0.5f);
                                         fx (w, 2, fxDistortion, 0.4f, 0.4f, 0.3f, 0.6f, 0.45f); }
                 else { w.setChoice (id::fltMode, fLowpass); w.set (id::fltCutoff, 2500.0f); w.set (id::fltReso, 0.35f); }
                 w.dryWet (0.85f);
                 w.route (1, M1, id::fltCutoff, 0.35f); w.route (2, M2, id::sizeRatio, -0.3f);
                 w.macros ("Filter", "rechts: heller", "Gate", "oben: kürzere, härtere Grains"); });

        // ------------------------------------------------------------------ Melodie
        add ("Melodie", "Harmonies", de ("Legt Terz und Quinte (in der Tonart) zu deiner Melodie, breit im Stereo."),
             [prof] (PresetWriter& w) {
                 w.setChoice (id::chordMode, chordChord);
                 if (prof.dark > 0.75f) intervals (w, { 0, 1, 4 }); else if (prof.space > 0.6f) intervals (w, { 0, 2, 4, 7 }); else intervals (w, { 0, 2, 4 });
                 w.setChoice (id::retune, retuneHard); w.setOn (id::sizeLink, false); w.set (id::grainSize, 80.0f); w.freeRateMs (30.0f);
                 w.set (id::chordSpread, 0.8f);
                 w.setOn (id::dlyOn, true); w.setChoice (id::dlyDiv, div (prof.echo)); w.set (id::dlyTaps, 2.0f); w.set (id::dlyMix, 0.25f);
                 w.dryWet (0.5f);
                 w.route (1, M1, id::chordSpread, 0.2f); w.route (2, M2, id::dlyMix, 0.5f); w.route (3, M2, id::dlySpace, 0.5f);
                 w.macros ("Breite", "rechts: Stimmen weiter auseinander", "Raum", "oben: mehr Echo und Hall"); });

        add ("Melodie", "Echo Steps", de ("Echos der Melodie steigen stufenweise in der Tonleiter - typisch für Breakdowns."),
             [prof] (PresetWriter& w) {
                 w.freeRateMs (50.0f); w.set (id::sizeRatio, 2.5f);
                 w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true); w.setChoice (id::dlyDiv, div (prof.echo));
                 w.set (id::dlyTaps, 4.0f); w.setChoice (id::dlyPattern, prof.dark > 0.7f ? shAlternate : shRamp);
                 w.set (id::dlyShift, prof.dark > 0.7f ? 1.0f : 2.0f); w.set (id::dlyFeedback, 0.4f);
                 w.set (id::dlySpace, 0.3f + 0.5f * prof.space); w.set (id::dlyMix, 0.5f); w.set (id::dlyHP, 250.0f);
                 w.dryWet (0.55f);
                 w.route (1, M1, id::dlyShift, 0.17f); w.route (2, M2, id::dlySpace, 0.4f);
                 w.macros ("Stufen", "rechts: grössere Tonsprünge", "Raum", "oben: mehr Hall"); });

        // ------------------------------------------------------------------ Gitarre
        add ("Gitarre", prof.space > 0.5f ? "Guitar Shimmer" : "Guitar Arp", de (prof.space > 0.5f
                 ? "Gitarre wird zur schimmernden Fläche - Echos eine Quinte höher."
                 : "Gitarren-Akkorde werden zu einem Arpeggio im Genre-Raster."),
             [prof] (PresetWriter& w) {
                 w.setChoice (id::pitchMode, pitchFree);
                 if (prof.space > 0.5f)
                 {
                     w.freeRateMs (45.0f); w.set (id::sizeRatio, 3.0f);
                     w.setOn (id::dlyOn, true); w.setOn (id::dlySync, false); w.set (id::dlyTime, 180.0f); w.set (id::dlyTaps, 3.0f);
                     w.set (id::dlyShift, 7.0f); w.set (id::dlySpace, 0.85f); w.set (id::dlyFeedback, 0.55f); w.set (id::dlyMix, 0.6f);
                 }
                 else
                 {
                     w.syncedRate (rateStraight, prof.grid); w.set (id::sizeRatio, 1.2f);
                     w.setChoice (id::chordMode, chordArp); intervals (w, { 0, 4, 7, 4 });
                     w.setOn (id::dlyOn, true); w.setChoice (id::dlyDiv, div (prof.echo)); w.set (id::dlyMix, 0.3f);
                 }
                 w.dryWet (0.6f);
                 w.route (1, M1, id::dlyFeedback, 0.3f); w.route (2, M2, id::dlySpace, 0.4f);
                 w.macros ("Länge", "rechts: längere Echos", "Raum", "oben: mehr Hall"); });

        add ("Gitarre", "Guitar Freeze", de ("Friert die Gitarre zu einem stehenden Klang ein - für Intros und Breaks."),
             [prof] (PresetWriter& w) {
                 w.set (id::stretch, 1.0f); w.setChoice (id::stretchRetrig, div ("4 bars"));
                 w.freeRateMs (16.0f); w.setOn (id::sizeLink, false); w.set (id::grainSize, 320.0f);
                 w.set (id::blur, 0.8f); w.set (id::spray, 0.15f); w.set (id::dimension, 0.5f);
                 w.setOn (id::fltOn, true); w.set (id::fltCutoff, 2500.0f + 4000.0f * (1.0f - prof.dark));
                 w.set (id::revMix, 0.2f + 0.3f * prof.space);
                 w.dryWet (0.7f);
                 w.route (1, M1, id::fltCutoff, 0.3f); w.route (2, M2, id::pitch, semis (prof.dark > 0.6f ? -12 : 12));
                 w.macros ("Helligkeit", "rechts: heller", prof.dark > 0.6f ? "Tiefe" : de ("Höhe"),
                           prof.dark > 0.6f ? "oben: eine Oktave tiefer" : de ("oben: eine Oktave höher")); });

        // ------------------------------------------------------------------ Vocal
        add ("Vocal", "Vocal Space", de ("Stimme mit Formant-Färbung, Delay im Genre-Tempo und Hall."),
             [prof] (PresetWriter& w) {
                 w.setOn (id::formOn, true); w.set (id::formShift, prof.dark > 0.6f ? -4.0f : 1.5f);
                 w.setOn (id::dlyOn, true); w.setOn (id::dlyPingPong, true); w.setChoice (id::dlyDiv, div (prof.echo));
                 w.set (id::dlyTaps, 3.0f); w.set (id::dlyFeedback, 0.35f); w.set (id::dlySpace, 0.4f + 0.4f * prof.space); w.set (id::dlyMix, 0.4f);
                 w.freeRateMs (30.0f); w.set (id::sizeRatio, 2.5f);
                 w.dryWet (0.5f);
                 w.route (1, M1, id::formShift, 0.25f); w.route (2, M2, id::dlyMix, 0.4f);
                 w.macros ("Formant", "rechts: hellere Stimmfarbe", "Echo", "oben: mehr Delay"); });

        add ("Vocal", "Vocal Chop", de ("Zerschneidet Stimmen im Takt - Stücke springen, teils rückwärts."),
             [prof] (PresetWriter& w) {
                 w.setChoice (id::playMode, playScrub); w.setChoice (id::scrubMode, 3); w.setChoice (id::loopLength, 3);
                 w.syncedRate (rateStraight, prof.stab); w.set (id::sizeRatio, 1.0f); w.set (id::revProb, 0.2f);
                 w.setChoice (id::mod (2, "Type"), modLfo); w.set (id::mod (2, "Shape"), 1.0f); w.setChoice (id::mod (2, "Div"), div (prof.stab));
                 w.setChoice (id::pitchMode, pitchScale); w.setChoice (id::retune, retuneHard);
                 w.dryWet (0.8f);
                 w.route (1, Mod2, id::scrubPos, 1.0f); w.route (2, M1, id::pitch, semis (prof.dark > 0.6f ? -5 : 7)); w.route (3, M2, id::revProb, 0.6f);
                 w.macros (de ("Tonhöhe"), "rechts: Stimmen springen in der Tonart", de ("Rückwärts"), "oben: mehr Stücke rückwärts"); });

        // ------------------------------------------------------------------ Pad / FX
        add ("Pad/FX", "Atmosphere", de ("Macht aus jedem Sound eine Fläche passend zum Genre."),
             [prof] (PresetWriter& w) {
                 w.set (id::stretch, 0.995f); w.set (id::blur, 1.0f); w.freeRateMs (10.0f);
                 w.setOn (id::sizeLink, false); w.set (id::grainSize, 400.0f); w.set (id::spray, 0.3f);
                 w.setOn (id::fltOn, true);
                 if (prof.dark > 0.75f) { w.setChoice (id::fltMode, fComb); w.setOn (id::fltScale, true); w.set (id::fltCutoff, 150.0f); w.set (id::fltReso, 0.7f); }
                 else { w.set (id::fltCutoff, 1500.0f + 4000.0f * (1.0f - prof.dark)); w.set (id::fltReso, 0.15f); }
                 w.set (id::revMix, 0.2f + 0.4f * prof.space);
                 w.dryWet (0.75f);
                 w.route (1, M1, id::fltCutoff, 0.35f); w.route (2, M2, id::detune, 0.5f); w.route (3, M2, id::dimension, 0.7f);
                 w.macros ("Helligkeit", "rechts: heller", "Breite", "oben: verstimmter und breiter"); });

        add ("Pad/FX", "Riser Grains", de ("Für Risers und Übergänge: Grains werden mit dem Pad dichter, höher und breiter."),
             [prof] (PresetWriter& w) {
                 w.syncedRate (rateStraight, prof.grid); w.set (id::sizeRatio, 1.5f); w.set (id::jitter, 0.3f);
                 w.setChoice (id::spreadMode, spScatter); w.set (id::spread, 0.8f);
                 w.setOn (id::dlyOn, true); w.set (id::dlySpace, 0.6f); w.set (id::dlyMix, 0.4f);
                 w.dryWet (0.7f);
                 w.route (1, M1, id::pitch, semis (12));
                 w.route (2, M2, id::jitter, 0.6f); w.route (3, M2, id::dlyFeedback, 0.5f);
                 w.macros (de ("Höhe"), "rechts: bis eine Oktave höher", "Chaos", "oben: unruhiger, mehr Echos"); });
    }
}
}
}
