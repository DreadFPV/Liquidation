# Liquidation

Granularer Echtzeit-Effekt als VST3 (Windows/macOS) und AU (macOS) – von JustCreateit.ch.
Liquidation zerlegt jedes Eingangssignal in Grains und baut es neu zusammen: immer in der Tonart, im Songtempo und mit vollem Zugriff auf jedes Detail.

## Funktionen

**Grain Engine**
- Laufende, tempo-synchrone Aufnahme (Loop 1 Beat bis 4 Takte), Hold friert den Puffer ein
- Grain Rate: Free (ms), Synced, Straight, Triplet, Dotted, Scale (stimmt wie ein Oszillator), MIDI (per Noten spielbar)
- Grain Size in ms oder gekoppelt an die Rate, Fensterform, Skew, Jitter

**Playback**
- Stretch: Grid, Transient oder Manual als Retrigger, 1x bis Freeze, Sync, Stretched-BPM-Anzeige
- Scrub: Position frei oder im Raster, per Maus direkt in der Waveform
- Reverse, Reverse-Wahrscheinlichkeit pro Grain, Spray, Blur, Level

**Pitch**
- Repitch, Free und Scale: Tonhöhe jedes Grains wird erkannt und auf die Skala korrigiert (Off, Smooth, Hard)
- Detune, Chord- und Arp-Modus mit 6 Intervallen (Halbtöne oder Skalenstufen), Arp Random, Stereo-Spread
- 17 Skalen plus eigene Skala, Global Tune (A4 = 415–466 Hz)

**Pro Grain**
- Filter mit 12 Typen: Lowpass, Highpass, Bandpass, Notch, Morph, Vowel, Comb, Flange, Modal, Phaser, Allpass, Reducer – mit Scale-Lock, Random, Modulation pro Grain, Drive
- Imager: Alternate, Pendulum, Helix, Bloom, Scatter, Drift, Smooth Spread, Dimension, Mono Lows, Width mit Safe-Modus
- Grain Delay: jede Wiederholung ist ein eigenes Grain, 8 Shift-Patterns, Ping Pong, Channel Swap, Feedback, Space (Shimmer), Low-/Highpass

**Effekte, Modulation, Presets**
- 4 Effekt-Slots mit je 8 Typen: Chorus, Compressor, Delay, Distortion, Equalizer, Filter, Reverb, Shifter; Dry/Wet vor oder nach dem Rack
- 2 Macros auf einem XY-Pad mit Animator (Orbit/Weave, Depth, Drift, Cycles, Warp, Rate/Sync)
- 4 Modulatoren (LFO mit Morph-Form, Step-Sequencer, Envelope Follower), 16-Slot-Matrix, Drag & Drop auf jeden Knopf
- 31 Werkpresets (Harmonic, Rhythmic, Freeze & Pads, Shimmer, Goa / Psy, Transform, Glitch, Vocal) und eigene Presets

## Installation

Fertige Builds gibt es unter **Actions → neuester Lauf → Artifacts** (`Liquidation-Windows`, `Liquidation-macOS`).

- **Windows:** `Liquidation.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
- **macOS:** Im Artifact liegen `Liquidation-VST3-mac.zip`, `Liquidation-AU-mac.zip` und `Liquidation-App-mac.zip`. Die ZIPs per Doppelklick entpacken (nicht mit einem anderen Tool, sonst gehen die Rechte verloren).
  `Liquidation.vst3` nach `~/Library/Audio/Plug-Ins/VST3/`, `Liquidation.component` nach `~/Library/Audio/Plug-Ins/Components/`.
  Die Builds sind nur ad-hoc signiert, deshalb einmalig im Terminal den Download-Vermerk entfernen:
  `xattr -cr ~/Library/Audio/Plug-Ins/VST3/Liquidation.vst3 ~/Library/Audio/Plug-Ins/Components/Liquidation.component /Applications/Liquidation.app`

In Cubase danach den Plug-in-Manager neu scannen lassen. Liquidation erscheint als Insert-Effekt.

**MIDI-Modus in Cubase:** Eine MIDI-Spur anlegen und deren Ausgang auf „Liquidation“ der Audiospur setzen, Grain Rate Mode auf „MIDI“.

Eigene Presets liegen unter `Dokumente/JustCreateit/Liquidation/Presets`.

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

JUCE steht unter AGPLv3 oder kommerzieller JUCE-Lizenz. Für einen Verkauf ist eine passende JUCE-Lizenz nötig (bis zu einer Umsatzgrenze kostenlos).
