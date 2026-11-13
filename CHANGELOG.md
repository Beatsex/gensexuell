# Changelog

## Unreleased

- A preset browser inside the plugin: PRESETS… in the main menu and at the top of the patch menu opens the 355 factory presets by category, together with your own .seedling files from Documents\Seedling (in USER, or under the name of their folder). Click or ↑ ↓ to load, ← → to change category; ‹ › next to the patch name step through the category of the loaded preset. The factory presets are built into the plugin, so they work without the installer or the Presets folder.
- Fixed: reopening a project reset parameters that the DAW had changed (automation, a mapped MIDI CC or the DAW's own parameter view) to the values the gensexuell window last showed. The project's parameter values now win.
- Fixed: a repeated note of the same pitch (performance → seed, in the rendered WAV and in the exported MIDI file) was cut off after 20 ms, because the previous note's end reached into it.
- Fixed: a short click on a silent sidechain in live mode could read outside the pitch detector's memory.
- Fixed: taking over a sounding voice (the polyphony limit is reached, a mono or legato retrigger) clicked, because the voice restarted from silence. It now continues from its current level, and the voice taken is a released one, the quietest first, before a held one. Extra mono voices fade out in 15 ms instead of stopping dead.
- Fixed: closing the window while text → seed (GROW) was still working could crash the host.
- Fixed: OSC B sample & hold (HOLD B) started each note from the previous note's step.
- Fixed: aftertouch kept its last value on the mod wheel after PRESSURE was switched off.
- Fixed: in MPE mode, All Notes Off and All Sound Off on a note channel were ignored.
- Fixed: switching live input off and on again started with the last tracked note.
- Fixed: the patch counter of sound → seed kept counting from the previous sample.
- Fixed: a pitch sweep or pitch bend of more than an octave up aliased (a saw bent up four octaves: aliases 18.5 dB below the tone, now 66.4 dB), because the oscillator's wave table was chosen for the unbent note. Notes with a pitch sweep, TUNE or bend now start with the table of the pitch they really play, so swept seeds can sound slightly different.
- Sound → seed also compares the first 80 ms of each try with the sample, in 5 ms steps, when it picks the next generation: the attack of plucks and hits now matches about 2.5 times better (average step difference 0.053 → 0.021 on eight percussive tests), at the same match score and speed. The match score shown is unchanged.
- Performance → seed: rendering the WAV no longer freezes the DAW's window while it runs.
- Voices take about 3 % less CPU, and sound → seed tries about 8 % more patches per second. The sound is unchanged.
- The audio thread no longer allocates memory (bass and treble shelves, live input) or calls the host (mapped MIDI CCs, program changes); those now happen on the user interface thread.

## 0.4.1

- 355 presets in 12 categories (atmospheric, basic, basses, chords, effects, keys, leads, pads, percussive, plucks, rhythmic, stabs). The installer puts them in Documents\Seedling\Presets and never overwrites one you changed; the macOS and Windows zips have them in a Presets folder.
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
- GLIDE is gentler: a note only slides while you still hold the previous one (before, in poly every note slid from the last one, even after a pause), and the slider is finest at the short end (halfway is about 1.25 s instead of 5 s).
- 14 new factory sounds (26 in all) that use the new pulse, pitch, filter-type and MOD S&H genes: SQUARE KEYS, THIN PULSE LEAD, OCTAVE BASS, FIFTH STAB, STEP FILTER, TWO PEAK VOX, NOTCH PAD, BAND PLUCK, LASER ZAP, FM PIANO, RING BELLS, DUST ORGAN, SWELL CHOIR, KICK DRUM.
- Fixed: the two extra filter stages of FILTER TYPE kept their state from a voice's previous note, which could click at the start of a note.
- Fixed: ENV TILT used the value of the voice's previous note (or none) for attack and decay; now every note uses its own. Seeds with ENV TILT away from the middle have slightly different attack and decay times than before.
- Fixed: loading a factory sound or a patch saved before the FX tab kept the current DRIVE, TAPE and VERB settings; they now start off.
- FX signal flow: ORDER picks one of the six orders of DRIVE, TAPE and VERB (the cards follow it), PLACE puts them after or before the grain FX, Haze and Dusk. Saved with the patch.
- LOW CPU MODE in ☰: the voices run without oversampling, about half the CPU per voice, with a little more aliasing on driven high notes. It applies from the next note, saved with the project, and the status line shows LOW CPU.
- Fixed: notes repeated while held. Factory sounds and every new seed had an envelope LOOP (factory sounds every 0.63 s, seeds every 1 to 8 s) that restarted long notes. LOOP now starts OFF; branches can still grow into it, and saved patches keep their value.
- Fixed: text → seed could transpose a take (octaves, semitones, cents) and switch LOOP or MOD S&H back on while it refined it. It now leaves pitch, LOOP and MOD S&H as the seed has them.
- Text → seed fits pads, drones, textures and gongs much better: it listened to each take with the key released after 0.8 s, so no sound could be long enough for them (average fit of 39 test descriptions 57 % → 90 %). E-piano and piano are described by their sustain instead of their length.
- Text → seed knows more words: piano, guitar (banjo, mandolin, ukulele), percussion and drums, arpeggio, reese and growl, sine, saw, square and pulse waves, glitch and stutter, pulsing and tremolo, stepped, looping, heavenly and dreamy, creepy, punchy, rough, sparkling, delicate and noise, in German and English. HOW TO DESCRIBE, the describe guides, the manuals and the word list on the website list them.
- Sample → seed has a newly trained net that hears ENV TILT (the old one was trained while ENV TILT had no effect) and learns "no loop" and "no steps" as one value each.
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
