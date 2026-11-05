# Changelog

## Unreleased

- Both manuals are on the website: https://beatsex.github.io/gensexuell/manual.html and https://beatsex.github.io/gensexuell/handbuch.html.

## 0.3.1

- The user manual has screenshots of the window, the menus and every dialog, and there is now a German manual (BENUTZERHANDBUCH.html). The German installer puts it in the Start menu.
- Playing branches with the computer keys A W S E D F T G Y H U J is now a switch in the MIDI settings (PLAY BRANCHES WITH COMPUTER KEYS) and off by default. In a DAW the notes sounded twice, because many DAWs turn the same keys into MIDI.

## 0.3.0

- The genome grows from 35 to 54 genes.
- Sample → seed listens in more detail (40 mel bands × 20 frames instead of 32 × 16), with a newly trained net.
- The search info box shows each strand's generation and the net and feature sizes.
- A net that does not fit the features, or a damaged one, now gives a message instead of starting neutral in silence.

## 0.2.0

First version in this repository.

- New name: gensexuell (formerly Seedling). Existing projects and patches still load.
- Growing branches from a seed, text → seed (German and English), sample → seed, performance → seed, live input.
- Grid layout with an on-screen keyboard, an output meter and drop zones for samples and recordings.
- Voice colours at the far end of three genes: ring modulation (SUB), wavefolding (DRIVE), stepped random pitch (LFO TARGET).
- Grain effects from grainsexuell, patches, MIDI learn, 6 dB more output.
- Windows installer, user manual, source package; the license is the GNU AGPL v3.
