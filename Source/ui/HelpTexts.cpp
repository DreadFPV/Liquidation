#include "HelpTexts.h"
#include <map>

namespace liq::ui
{
namespace
{
    struct Raw { const char* title; const char* what; const char* more; const char* less; };

    const std::map<juce::String, Raw>& table()
    {
        static const std::map<juce::String, Raw> t {
            // ---- global
            { "inputGain",  { "Input Gain", "Lautstärke deiner Spur, bevor sie bearbeitet wird.", "lauter rein - Effekte greifen stärker", "leiser rein" } },
            { "auxGain",    { "Aux Gain", "Lautstärke des Sidechain-Signals (Signal B).", "B wird stärker im Morph", "B wird schwächer" } },
            { "outputGain", { "Output", "Gesamtlautstärke am Ausgang.", "lauter", "leiser" } },
            { "globalTune", { "Stimmung A4", "Referenzton. 440 Hz ist Standard; nur ändern, wenn dein Song anders gestimmt ist.", "alles höher gestimmt", "alles tiefer gestimmt" } },
            { "scaleRoot",  { "Grundton", "Tonart deines Songs. Alles, was 'in Key' arbeitet, folgt diesem Ton.", "", "" } },
            { "scaleType",  { "Skala", "Tonleiter deines Songs. Melodien, Akkorde und Tonhöhenkorrektur bleiben darin.", "", "" } },
            { "limitOn",    { "Limiter", "Verhindert Übersteuern am Ausgang (Decke -0.3 dB).", "", "" } },
            { "routing",    { "Effekt-Routing", "Post: Effekte nur auf das bearbeitete Signal. Pre: Effekte auf die ganze Summe inkl. Original.", "", "" } },
            // ---- macros / animator
            { "macro1",     { "Macro 1", "Erster Hebel des XY-Pads (links/rechts). Was er steuert, steht am Pad.", "Pad nach rechts", "Pad nach links" } },
            { "macro2",     { "Macro 2", "Zweiter Hebel des XY-Pads (unten/oben).", "Pad nach oben", "Pad nach unten" } },
            { "animDepth",  { "Figur-Grösse", "Der Animator fährt eine Figur um den Pad-Punkt und bewegt so beide Macros automatisch.", "grössere Figur, mehr Bewegung", "0 = Pad steht still" } },
            { "animRate",   { "Figur-Tempo", "Wie schnell die Figur abgefahren wird.", "schneller", "langsamer" } },
            { "animSync",   { "Sync", "Figur-Tempo im Songtempo (Notenwert statt Hz).", "", "" } },
            { "animDiv",    { "Figur-Länge", "Wie lange eine Runde der Figur im Songtempo dauert.", "", "" } },
            { "animMode",   { "Figur", "Orbit: Kreise und Blüten. Weave: sich kreuzende Schleifen.", "", "" } },
            { "animCycles", { "Schleifen", "Wie verschlungen die Figur ist.", "mehr Schleifen, wirkt schneller", "einfacher Kreis" } },
            { "animWarp",   { "Form", "Verbiegt die Figur (Blütenblätter bei Orbit, Versatz bei Weave).", "spitzer, komplexer", "runder" } },
            { "animDrift",  { "Drift", "Zufälliges Wackeln um die Figur - wirkt lebendiger.", "unruhiger", "exakt auf der Figur" } },
            // ---- grain
            { "grainRate",  { "Grain Rate", "Wie oft ein neues Grain (kurzes Klangstück) startet.", "dichter; sehr schnell wird es ein eigener Ton", "weniger Grains, rhythmischer" } },
            { "rateMode",   { "Rate-Modus", "Free = Millisekunden. Synced/Straight/Triplet/Dotted = Notenwerte im Songtempo. Scale = Grains als Ton in der Tonart. MIDI = per Noten gespielt.", "", "" } },
            { "grainSize",  { "Grain Size", "Länge jedes Grains.", "weicher, fliessender", "kürzer, perkussiver, abgehackt" } },
            { "sizeRatio",  { "Grain Size", "Länge jedes Grains im Verhältnis zur Rate. 1x = Grains berühren sich.", "überlappend, weich", "Lücken, Gate-Effekt" } },
            { "sizeLink",   { "Link", "An: Grösse folgt der Rate (bleibt gleich dicht). Aus: Grösse in ms.", "", "" } },
            { "winShape",   { "Shape", "Form der Lautstärke jedes Grains.", "voller, eckiger, mehr Druck", "spitzer, weicher Anschlag" } },
            { "winSkew",    { "Skew", "Verschiebt den lautesten Punkt im Grain.", "langsamer Einsatz, kurzes Ende (wie rückwärts)", "schneller Anschlag, langes Ausklingen" } },
            { "jitter",     { "Jitter", "Zufällige Zeitverschiebung der Grains.", "lockerer, chaotischer", "exakt im Raster" } },
            { "stretch",    { "Stretch", "Verlangsamt die Wiedergabe, die Tonhöhe bleibt gleich. Ganz rechts = Freeze (Klang steht).", "langsamer bis eingefroren", "Original-Tempo" } },
            { "stretchSync",{ "Stretch Sync", "Stretch springt auf musikalische Werte (2x, 4x ...).", "", "" } },
            { "stretchSens",{ "Sensitivity", "Wie leicht ein Schlag (Transient) das Stretching neu startet.", "reagiert auch auf leise Schläge", "nur auf laute Schläge" } },
            { "scrubPos",   { "Position", "Wo im aufgenommenen Loop gespielt wird (Scrub-Modus). Mit einem Modulator bewegt = neue Abfolge.", "später im Loop", "früher im Loop" } },
            { "spray",      { "Spray", "Zufällige Startposition je Grain.", "diffuser, wolkiger", "präzise, wie das Original" } },
            { "blur",       { "Blur", "Verwischt die Grains ineinander - glättet Stretch und Freeze.", "weicher, flächiger", "klarer, mehr Kontur" } },
            { "revProb",    { "Reverse", "Wahrscheinlichkeit, dass ein Grain rückwärts spielt.", "mehr Grains rückwärts", "alles vorwärts" } },
            { "grainLevel", { "Level", "Lautstärke der Grains.", "lauter", "leiser" } },
            { "pitch",      { "Pitch", "Transponiert die Grains. Im Scale-Modus in Stufen deiner Tonleiter.", "höher", "tiefer" } },
            { "detune",     { "Detune", "Zufällige leichte Verstimmung je Grain - wie ein Chor.", "breiter, schwebender", "sauber gestimmt" } },
            { "arpRand",    { "Arp Random", "Wie zufällig die Reihenfolge der Arp-Töne ist.", "zufälliger", "feste Reihenfolge" } },
            { "chordSpread",{ "Chord Spread", "Verteilt die Akkordstimmen im Stereo.", "breiter", "alles in der Mitte" } },
            { "chordMode",  { "Chord / Arp", "Off = einzelne Töne. Arp = Töne nacheinander. Chord = alle Töne gleichzeitig - immer in deiner Tonart.", "", "" } },
            { "playMode",   { "Wiedergabe", "Stretch: läuft mit, dehnbar bis Freeze. Scrub: du fährst durch den aufgenommenen Loop.", "", "" } },
            // ---- grain fx
            { "fltCutoff",  { "Cutoff", "Grenzfrequenz des Filters auf jedem Grain.", "heller", "dunkler" } },
            { "fltReso",    { "Resonance", "Betont die Frequenz am Cutoff.", "pfeifender, resonanter", "neutraler" } },
            { "fltDrive",   { "Drive", "Sättigung im Filter.", "dreckiger, lauter", "sauber" } },
            { "fltModDepth",{ "Mod Depth", "Wie weit sich der Filter in jedem Grain bewegt.", "mehr Wah-Bewegung", "statischer Filter" } },
            { "fltModRate", { "Mod Rate", "Wie oft sich der Filter pro Grain bewegt.", "schneller, bis zum Brummen", "eine Bewegung pro Grain" } },
            { "fltMorph",   { "Morph", "Ändert den Charakter des Filters (je nach Typ: LP->HP, Vokal, Abstand ...).", "", "" } },
            { "fltMix",     { "Filter Mix", "Anteil des gefilterten Signals.", "nur gefiltert", "ungefiltert" } },
            { "spread",     { "Spread", "Wie weit die Grains im Stereo verteilt werden.", "breiter", "mono" } },
            { "pan",        { "Pan", "Mittelpunkt der Grain-Verteilung.", "nach rechts", "nach links" } },
            { "width",      { "Width", "Stereobreite am Ende des Imagers. 100 % = unverändert.", "breiter", "schmaler bis mono" } },
            { "dimension",  { "Dimension", "Raumeffekt, der die Grains breiter wirken lässt.", "breiter, räumlicher", "trocken" } },
            { "dlyFeedback",{ "Feedback", "Wie oft die Grain-Echos wiederholt werden.", "längere Echofahne", "kurze Echos" } },
            { "dlyMix",     { "Delay Mix", "Anteil der Grain-Echos.", "mehr Echo", "weniger Echo" } },
            { "dlyShift",   { "Shift", "Tonhöhen-Schritt zwischen den Echos (Muster wählbar).", "grössere Tonsprünge", "gleiche Tonhöhe" } },
            { "dlySpace",   { "Space", "Hall auf den Echos - mit Shift entsteht Shimmer.", "mehr Raum", "trocken" } },
            { "dlyTaps",    { "Taps", "Anzahl der Echos pro Grain.", "mehr Echos", "weniger" } },
            // ---- morph / formant / transients
            { "morphX",     { "X-Fade", "Anteil von A (deine Spur) und B (Sidechain oder Modeler).", "mehr B", "mehr A" } },
            { "morphY",     { "Morph", "Unten normales Überblenden, oben volles Verschmelzen im Spektrum.", "stärker verschmolzen", "einfaches Überblenden" } },
            { "morphGamma", { "Gamma", "Kontrast im Spektrum.", "spitzer, resonanter", "weicher, rauschiger" } },
            { "morphDetail",{ "Detail", "Wie fein die Klangfarbe übertragen wird.", "feiner, genauer", "grober, weicher" } },
            { "mmDepth",    { "Motion", "Bewegt den Punkt im Morph-Pad automatisch.", "grössere Bewegung", "0 = steht" } },
            { "mmRate",     { "Speed", "Tempo der Morph-Bewegung.", "schneller", "langsamer" } },
            { "formShift",  { "Formant Shift", "Verschiebt die Klangfarbe, ohne die Tonhöhe zu ändern.", "heller, kleiner (Mickey)", "dunkler, grösser (Monster)" } },
            { "trSens",     { "Sense", "Wie leicht ein Anschlag als Transient erkannt wird.", "auch leise Schläge", "nur harte Schläge" } },
            { "trLength",   { "Length", "Wie lange der Original-Anschlag erhalten bleibt.", "mehr Original-Punch", "nur der allererste Klick" } },
            { "trAmount",   { "Amount", "Wie stark der Original-Anschlag zurückkommt.", "voller Original-Anschlag", "bearbeitet" } },
            // ---- modeler + melody
            { "mdlOctave",  { "Octave", "Tonlage des Modelers.", "höher", "tiefer" } },
            { "mdlTone",    { "Tone", "Helligkeit des Modelers.", "heller, offener Filter", "dunkler, weicher" } },
            { "mdlMotion",  { "Motion", "Bewegung im Modeler-Klang (Filter, Vibrato, Schwebung).", "lebendiger", "statisch" } },
            { "mdlRate",    { "Rate", "Notenlänge für Arp, Psy Bass und Melodie.", "", "" } },
            { "mdlFollow",  { "Follow", "Der Modeler folgt der Tonhöhe deiner Spur.", "", "" } },
            { "mlStyle",    { "Melodie-Stil", "Welche Art Phrase geschrieben wird: Lead, Arp, Acid, Stabs, Bass ...", "", "" } },
            { "mlVar",      { "Variante", "Jede Nummer ist eine andere fertige Melodie im gleichen Stil. Durchklicken, bis es passt.", "nächste Melodie", "vorige Melodie" } },
            { "mlDensity",  { "Dichte", "Wie viele Noten die Phrase hat.", "mehr Noten, treibender", "mehr Pausen, luftiger" } },
            { "mlRange",    { "Umfang", "Über wie viele Oktaven sich die Melodie bewegen darf.", "grössere Sprünge", "enger, ruhiger" } },
            { "mlLength",   { "Länge", "Wie lang die Phrase ist, bevor sie sich wiederholt.", "", "" } },
            { "mlTrigger",  { "Auslöser", "Grid: läuft im Takt. Kick Step: jeder Kick spielt die nächste Note. Kick Gate: Melodie nur, wo die Kick klingt. Between Kicks: spielt zwischen den Kicks.", "", "" } },
            { "mlGlide",    { "Glide", "Gleiten zwischen Noten bei Slides.", "langsames Gleiten", "springt sofort" } },
            { "mlSens",     { "Kick-Erkennung", "Wie leicht ein Schlag deiner Spur als Kick erkannt wird.", "auch leise Schläge", "nur laute Kicks" } },
            // ---- mixer
            { "quickMix",   { "Mix", "Original gegen Effekt in einem Regler. Links = nur deine Spur, Mitte = beides voll, rechts = nur Effekt.", "mehr Effekt", "mehr Original" } },
            { "mixWet",     { "Morph (Effekt)", "Pegel des bearbeiteten Signals.", "mehr Effekt", "kein Effekt" } },
            { "mixDry",     { "Main In (Original)", "Pegel deiner Original-Spur. Zusammen mit Morph = Dry/Wet.", "mehr Original", "nur Effekt" } },
            { "mixAux",     { "Aux In", "Pegel des Sidechain-Signals pur.", "lauter", "aus" } },
            { "mixModel",   { "Modeler / Melodie", "Pegel des Modelers bzw. der generierten Melodie.", "lauter", "aus" } },
            { "revSize",    { "Reverb Size", "Grösse des Raums.", "grösserer Raum, längerer Hall", "kleiner Raum" } },
            { "revDamp",    { "Reverb Damp", "Dämpft die Höhen im Hall.", "dunkler Hall", "heller Hall" } },
            { "revMix",     { "Reverb", "Menge des Halls auf der Summe.", "mehr Raum", "trocken" } },
        };
        return t;
    }
}

ParamHelp helpFor (const juce::String& paramId)
{
    const auto& t = table();
    auto it = t.find (paramId);
    if (it == t.end())
    {
        // effects-rack knobs and modulators share generic texts
        if (paramId.startsWith ("fx") && paramId.endsWith ("Mix"))
            return { "Effekt Mix", juce::String::fromUTF8 ("Anteil dieses Effekts."), "mehr Effekt", "weniger Effekt" };
        if (paramId.startsWith ("mx") && paramId.endsWith ("Amt"))
            return { "Mod-Stärke", juce::String::fromUTF8 ("Wie stark die Quelle den Regler bewegt. Negativ = umgekehrte Richtung."), "", "" };
        return {};
    }
    const auto& r = it->second;
    return { juce::String::fromUTF8 (r.title), juce::String::fromUTF8 (r.what),
             juce::String::fromUTF8 (r.more), juce::String::fromUTF8 (r.less) };
}
}
