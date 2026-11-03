# gensexuell

**Grow sounds, don't generate songs.** gensexuell is a synthesizer you grow: drag branches out of a seed and each one mutates the sound a little further. Keep the branch you like, sow it as the new seed, repeat. Everything runs on your computer: no cloud, no account, no internet connection. gensexuell makes sounds, never finished music.

*Deutsch weiter unten.*

## Three ways to a sound

- **Grow it.** 12 branches around a seed, each longer branch further from the original. Sow, clone, reroll, undo.
- **Describe it.** Type what you want, in German or English: *"dark, soft bass"*, *"glassy pluck, not too bright"*, *"wärmeres Pad mit viel Raum"*. gensexuell reads the words as qualities of the sound (bright, attack, sustain, noise, motion, low end …) and searches for seeds that actually sound that way. Four takes per click; click GROW again for four new ones.
- **Bring a sample.** Drop a one-shot into SOUND: a neural net guesses the genes, four strands refine the match while you listen.

## Play with your own performance

- **Performance → seed.** Drop a monophonic recording (vocals, humming, bass, a lead line). gensexuell finds the notes, timing, dynamics and bends, and plays them with the current seed. Drag the result into your DAW as WAV or MIDI.
- **Live input.** Route audio to gensexuell's sidechain and play the synth with your voice or an instrument in real time.

## What else is inside

Bulb modes (standard, velocity, ranges, layered), poly/mono/legato with glide, mod wheel ring with ten targets, tempo sync, 35 genes in a gene editor, branch volumes, patch files, programs, MIDI learn, MPE basics, and grain effects from [grainsexuell](https://github.com/Beatsex/grainsexuell) (Mosaic, Cloud, Shard, Tap, Haze reverb, Dusk low-pass).

## Formats

VST3, CLAP, LV2 and Standalone (Windows, macOS), AU (macOS).

## Build

Windows: run `plugin\build.cmd` (Visual Studio 2022 Build Tools with CMake). It builds every format and runs the engine test.
Windows installer: after the build, run `"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\gensexuell.iss`; the setup lands in `plugin\build\installer`.
macOS: see `.github/workflows/macos.yml`.
JUCE 8 and clap-juce-extensions are expected in `plugin/libs` (pinned versions in the workflow). Windows also needs the NuGet package `Microsoft.Web.WebView2` version `1.0.3485.44` in `plugin/libs/nuget`.

The sound → seed net is trained from the engine itself: `plugin/tools/dataset.cmd 150000`, then `py plugin/train/train.py`. Retrain whenever genes or the voice DSP change.

## License

Copyright (C) 2026 beatsexuell. gensexuell is free software under the [GNU AGPL v3](LICENSE): you may use, change and share it, and whoever passes it on, changed or not, passes on the source too.

It uses JUCE 8 under its AGPLv3 option, clap-juce-extensions (MIT), the grainsexuell effects (MIT) and the fonts Inter and JetBrains Mono (SIL OFL 1.1); see [THIRD-PARTY-LICENSES.txt](THIRD-PARTY-LICENSES.txt). The source package leaves out JUCE and clap-juce-extensions: `.github/workflows/macos.yml` pins the exact versions to fetch into `plugin/libs`.

---

## Deutsch

**Klänge wachsen lassen, keine Songs generieren.** gensexuell ist ein Synthesizer zum Wachsenlassen: Zieh Äste aus einem Samen, jeder Ast verändert den Klang ein Stück weiter. Behalte den Ast, der dir gefällt, säe ihn als neuen Samen, und wiederhole. Alles läuft auf deinem Rechner: keine Cloud, kein Konto, keine Internetverbindung. gensexuell erzeugt Klänge, nie fertige Musik.

**Drei Wege zum Klang**
- **Wachsen lassen:** 12 Äste um einen Samen, je länger, desto weiter vom Original.
- **Beschreiben:** Auf Deutsch oder Englisch eintippen, z. B. „dunkler, weicher Bass“ oder „glasiger Pluck, nicht zu hell“. gensexuell versteht die Wörter als Klangeigenschaften und sucht Samen, die wirklich so klingen. Vier Takes pro Klick.
- **Sample mitbringen:** One-Shot in SOUND ziehen: Ein neuronales Netz schätzt die Gene, vier Stränge verfeinern den Treffer.

**Mit deiner eigenen Performance spielen**
- **Performance → Seed:** Eine einstimmige Aufnahme (Gesang, Summen, Bass, Lead) reinziehen. gensexuell erkennt Noten, Timing, Dynamik und Bends und spielt sie mit dem aktuellen Samen. Das Ergebnis als WAV oder MIDI direkt in die DAW ziehen.
- **Live-Eingang:** Audio auf den Sidechain von gensexuell routen und den Synth in Echtzeit mit Stimme oder Instrument spielen.
