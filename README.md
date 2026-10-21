# Liquidation

Granularer Echtzeit-Effekt als VST3 (Windows/macOS) und AU (macOS) – von JustCreateit.ch.
Liquidation zerlegt jedes Eingangssignal in Grains und baut es neu zusammen: immer in der Tonart, im Songtempo und mit vollem Zugriff auf jedes Detail.

## Signalfluss

```
MAIN IN (A) ─────────────────┐
AUX IN (Sidechain) ─┐        ├─► MORPH ─► FORMANTS ─► GRAIN ENGINE ─► GRAIN FX ─► TRANSIENTS ─► MIXER ─► EFFECTS ─► OUTPUT
MODELER ────────────┴─ B ────┘                                                                   ▲
Originale (zeitgleich ausgerichtet) ─────────────────────────────────────────────────────────────┘
```

Zwei Seiten (oben rechts **PLAY | EDIT**):
- **Play** – Preset-Finder (Quelle × Genre), grosses XY-Pad für Macro 1/2 mit Animator-Figuren, die 8 wichtigsten Klangregler, Melodie-Generator und Tipps.
- **Edit** – der ganze Signalfluss oben. Jeder Block zeigt die wichtigsten Regler, **Details** öffnet alles.

Die **Info-Leiste** unten erklärt, was unter der Maus liegt: was der Regler macht, was bei mehr und bei weniger passiert. **?** schaltet zusätzlich den Hilfe-Modus mit Karten pro Block ein.

## Funktionen

**Morph** – verschmilzt deine Spur (A) mit Signal B im Frequenzbereich
- B = Sidechain (Aux In) oder der eingebaute **Modeler** (Pad, Arp, Drone, Psy Bass, Vowel, Noise – in Tonart und Tempo)
- XY-Pad: X = Anteil A↔B, Y = normales Überblenden ↔ volles Morphing
- Algorithmen: Blend, Imprint (A bekommt die Klangfarbe von B), Carve (Vocoder), Weave, Duck (spektrales Ducking)
- Gamma, Detail, Motion (automatische Bewegung des Pads), Formant-Shift

**Grain Engine**
- Laufende, tempo-synchrone Aufnahme; Stretch (Grid/Transient/Manual, bis Freeze) oder Scrub
- Grain Rate Free/Synced/Scale/MIDI, Size, Fenster, Jitter, Spray, Blur, Reverse
- Pitch mit Scale-Lock pro Grain, Chord- und Arp-Modus
- Pro Grain: 12 Filtertypen, Imager (6 Spread-Modi, Dimension, Mono Lows, Width), Grain-Delay mit 8 Shift-Patterns

**Transients, Mixer, Ausgang**
- Transient-Erhalt: der Original-Anschlag (z. B. Kick) bleibt knackig
- Mixer: Morph, Main In, Aux In, Modeler – je Pegel, Mute, Solo; Reverb auf die Summe
- 4 Effekt-Slots (Chorus, Compressor, Delay, Distortion, EQ, Filter, Reverb, Shifter), Pre/Post
- Output-Limiter

**Melodie-Generator (Kick → Melodie)**
- Der Modeler schreibt fertige Phrasen in deiner Tonart: Goa Lead, Rolling Arp, Acid, Stabs, Dark Pulse, Hook, Reese, Chords, Rolling Bass
- Variante 1–99 (jede Zahl = andere Melodie, immer gleich reproduzierbar), Dichte, Umfang, Glide, Länge 1/2/4 Takte
- Auslöser: Grid (im Takt), Kick Step (jede Kick = nächste Note), Kick Gate (klingt nur mit der Kick), Between Kicks (Offbeats)
- **MIDI ziehen**: die aktuelle Phrase als MIDI-Part direkt auf eine Cubase-Spur ziehen

**Modulation & Presets**
- 2 Macros + 4 Modulatoren (LFO, Sequencer, Envelope Follower), 16-Slot-Matrix, Drag & Drop auf jeden Regler
- 203 Werkpresets und eigene Presets
  - 8 × **Kick → Melodie**
  - 9 Genres × 17 Presets: Goa, Full-On, Psytrance, Progressive, Darkpsy, Hi-Tech, Drum & Bass, Neurofunk, Jungle – je für Kick, Bass, Drums, Synth, Melodie, Gitarre, Vocal, Pad/FX
  - 41 Sound-Design-Presets
- Jedes Preset benennt seine Macros (z. B. „Variante“ / „Helligkeit“), die Namen stehen am XY-Pad

**Latenz:** konstant 2115 Samples (ca. 44 ms bei 48 kHz), Cubase gleicht das automatisch aus.

## Installation

Fertige Builds gibt es unter **Actions → neuester Lauf → Artifacts** (`Liquidation-Windows`, `Liquidation-macOS`).

- **Windows:** `Liquidation.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
- **macOS:** Im Artifact liegen `Liquidation-VST3-mac.zip`, `Liquidation-AU-mac.zip` und `Liquidation-App-mac.zip`. Die ZIPs per Doppelklick im Finder entpacken.
  `Liquidation.vst3` nach `/Library/Audio/Plug-Ins/VST3/`, `Liquidation.component` nach `/Library/Audio/Plug-Ins/Components/`, die App nach Programme.
  Die Builds sind nur ad-hoc signiert, deshalb einmalig im Terminal den Download-Vermerk entfernen:
  `sudo xattr -cr /Library/Audio/Plug-Ins/VST3/Liquidation.vst3 /Library/Audio/Plug-Ins/Components/Liquidation.component /Applications/Liquidation.app`

In Cubase danach den Plug-in-Manager neu scannen lassen. Liquidation erscheint als Insert-Effekt.

**Sidechain in Cubase:** Im Plugin-Fenster oben den Sidechain-Knopf aktivieren. Auf der Spur, die als Signal B dienen soll, einen Send auf „Liquidation – Sidechain“ legen. Im Plugin bei MORPH den Schalter auf Aux In stellen.

**MIDI-Modus in Cubase:** Eine MIDI-Spur anlegen und deren Ausgang auf „Liquidation“ der Audiospur setzen, Grain Rate Mode auf „MIDI“.

Eigene Presets liegen unter `~/Library/JustCreateit/Liquidation/Presets` (macOS) bzw. `%APPDATA%\JustCreateit\Liquidation\Presets` (Windows).

## Selbst bauen

Benötigt CMake ≥ 3.22 und einen C++20-Compiler (Visual Studio 2022, Xcode 15 oder GCC 13). JUCE wird automatisch geladen.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release      # DSP-Tests
```

Optional mit lokalem JUCE: `-DJUCE_DIR=/pfad/zu/JUCE`.

## Projektaufbau

```
Source/
  Params.*            alle Parameter (IDs, Bereiche, Modulationsziele)
  Presets.*           Werkpresets und Preset-Verwaltung
  PluginProcessor.*   Audio-Pfad, Modulation, Zustand
  PluginEditor.*      Fenster, Tabs, Skalierung
  dsp/                Grain-Engine, Pitch-Erkennung, Filter, Imager, Delay, Effekte, Modulation, Animator
  ui/                 Look & Feel, Knobs, Waveform, XY-Pad, Seiten
Tests/                Headless-DSP-Tests (alle Presets, alle Modi, Zustand)
```

## Lizenz

Schrift: Barlow Semi Condensed (SIL Open Font License, siehe `Resources/Fonts/OFL.txt`).

JUCE steht unter AGPLv3 oder kommerzieller JUCE-Lizenz. Für einen Verkauf ist eine passende JUCE-Lizenz nötig (bis zu einer Umsatzgrenze kostenlos).
