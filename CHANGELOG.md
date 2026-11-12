# Changelog

## Unreleased

- The oscillator wave morphs sine (0) → saw (0.57) → square (0.81) → pulse (1.0); over the last stretch the pulse narrows from 50 % to 12.5 % and keeps the square's loudness. The triangle stage is gone. Factory patches and the describe words were moved to the new positions so they keep their sound; saved patches with a wave above 0.81 now play a pulse.
- Oscillator pitch has its own genes: OCT A (−2…+2 octaves), SEMI A (±12), CENT A (±50) and, on top of RATIO B, SEMI B and CENT B. B, FM and the sub follow A. New seeds start in tune; below 50 % atonality, branches keep the octave and semitones and only drift in cents. The old PITCH A gene never changed the pitch and is gone from the editor (old patches still load), so the editor shows 58 genes.
- FILTER TYPE morphs through four filters: band-pass (0) → steep 24 dB low-pass (0.33) → two low-pass peaks an octave apart (0.67) → notch (1). Saved patches with a filter type other than 0 or 1 sound different; the describe words for hollow and vocal sounds were moved to the new positions.
- New gene MOD S&H (in Envelope & LFO): above 0.6 the mod envelope moves in steps, 1 to 64 per second, so pitch sweeps, filter envelopes and FM jump instead of gliding. Seeds start without it; branches can grow into it. The editor now shows 59 genes.
- Each voice runs at twice the sample rate (oscillators, filter, drive and fold) and is brought back down with a half-band filter, so driven and folded sounds on high notes alias much less (a driven saw at 1760 Hz: aliases 31.8 dB below the tone instead of 23.5 dB). A voice now takes about twice the CPU.
- Sample → seed has a newly trained net that also guesses the new pitch genes and MOD S&H, and hears the oversampled voice and the new filter types.
- The genes have their own tab: GENES next to SEED above the branches shows them in place of the seed view, so the keyboard keeps playing while you edit. EDIT GENES (a framed button next to ATONALITY) and Ctrl+G switch to it. In the GENES tab, FIND A GENE searches all tabs by name and by what a gene does, every gene shows its description when you hover it, and a double-click resets a gene.
- Every control explains itself: hover it for a tooltip, and a line at the bottom of the seed view says what it does. Greyed-out WAV and MIDI tiles say what to do first.
- Easier to read: labels are larger and darker, and the mode selectors (BULB, VOICE, WHEEL, SYNC, live input, grain FX) look like buttons.
- No more double names: the live input slider is now LIVE GLIDE and the mod wheel amount is WHEEL DEPTH.
- The gene editor shows real values: ms and s, Hz and kHz, dB, octaves, wave and filter names, ON/OFF where a gene only acts above a point. Times and cutoff are shown as played at C4.
- SOW, NEW SEED, REROLL and CLONE say how they differ, and a first-start hint in the seed view says what to do first until you play a key or drag a branch.
- The FX CHORUS gene now sets the chorus speed (CHORUS RATE, 0.04 … 2.8 Hz, when CHORUS is on); before, it did nothing. Saved patches use the value they already hold, so their chorus can move faster or slower than before. ROOM ATTACK and ROOM LENGTH still do nothing and say NOT USED.
- New FX tab with three effects from the beatsexuell plugins, after the grain FX and Haze: DRIVE (drivesexuell), TAPE (tapesexuell) and VERB (verbsexuell, 21 modes). Each has an ON switch and its main controls, is saved with the patch and can be automated. Their DSP is copied unchanged into Source/retro.
- MPE: a new switch in the MIDI settings. Channel 1 is the master, channels 2–16 play one note each with its own pitch bend (MPE NOTE BEND RANGE, default 48 semitones), pressure (opens the note's filter) and slide on CC74 (brighter or darker). The on-screen keyboard and live input no longer share MIDI channels 16 and 15 with your controller.
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
