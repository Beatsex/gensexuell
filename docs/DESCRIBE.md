# Describe guide

**English** · [Deutsch](DESCRIBE.de.md)

In the **DESCRIBE** field you describe a sound in German or English, or a mix of both. **GROW** or Enter gives you four takes, the best one first. A take is a normal seed: drag branches, sow, save. **GROW** again gives four new takes for the same words.

The words describe the sound, not the knobs. gensexuell tries seeds, measures how they actually sound and keeps the closest ones. All of it happens locally on your computer.

The app has this guide built in: click **HOW TO ?** next to DESCRIBE.

## How to build a description

**What + how** is usually enough. Take one kind of sound and one to three qualities:

| You type | Why it works |
|---|---|
| `dark soft bass` | a kind of sound plus two qualities that fit it |
| `kurze metallische Glocke` | length and material give a clear direction |
| `glassy pluck, not too bright` | "not too" tones it down instead of flipping it |
| `808 kick, very short, slightly distorted` | a stronger and a weaker quality |
| `wide Juno-pad with chorus` | hyphens get split: Juno + pad |
| `warm pad with lots of reverb` | room and reverb words set the effect directly |

## What you can type

**Kinds of sound** set the basic shape and usually the register too:
pad / strings · bass · sub / 808 · pluck / pizz · lead / solo · bell / chime · mallet / marimba · kick · snare / clap · hat / cymbal · drone · fx / riser / sweep · texture / atmo / ambient · organ · chord / stab

**Qualities**

| Direction | Words |
|---|---|
| Brightness | bright, crisp · dark, muffled |
| Character | warm, soft, smooth, mellow · hard, aggressive, harsh |
| Material | glassy, crystal · metallic · wood, bamboo · icy, cold, cool |
| Dirt | dirty, gritty, broken, lofi · distorted, crunch · clean, pure |
| Length | short, staccato, percussive, snappy, tight · long, endless, sustained |
| Attack | slow, swell · fast, immediate |
| Air | airy, breathy, noisy |
| Motion | wobble, wavy, vibrato · moving, evolving, organic · static, steady |
| Tuning | detuned, wonky, atonal · tonal, melodic |
| Body | fat, thick, full, wide · thin · low, deep · high · hollow, nasal |
| Space | reverb, spacey, roomy · dry, close · shimmer, cathedral, church |
| Era and effects | analog, vintage, retro · FM, digital, 80s · acid, 303 · chorus, phaser, flanger · tape, cassette, vinyl, flutter · laser, zap, sci-fi, sonar |

**References** describe a typical character. They are approximations, not recreations:
- Synths: CS-80, Vangelis · Juno, Jupiter · Moog, Taurus · Prophet, Oberheim · DX7, Rhodes, Wurli · MS-20 · Buchla, West Coast · Mellotron · supersaw, trance · chiptune, 8-bit, C64, Game Boy (`gameboy`)
- Instruments: kalimba, handpan · harp, koto, sitar · cello, violin, viola · flute, shakuhachi, duduk · brass, trumpet, horn · choir, voice, vocal · gong, gamelan, singing bowl (`bowl`)

## Fine control

| Write | Effect |
|---|---|
| `very`, `extremely`, `lots`, `more`, `strong` | the next word counts more |
| `slightly`, `a bit`, `little`, `less`, `subtly` | the next word counts less |
| `too` | the next word counts a little more |
| `not`, `no`, `without` | the next word flips: `not bright` means dark, `no reverb` means dry |
| `not too` | the next word lands in the middle: `not too airy` is moderately airy, not free of air |
| comma | ends a modifier you started |

A modifier only applies to **the next word**. `not bright and warm` means: not bright, but warm.

If you name several kinds of sound, the **last** one sets the register. Single typos in longer words are forgiven.

## Reading the display

- **UNDERSTOOD** shows what was recognised. Words struck through under **NOT IN THE LEXICON** were ignored; swap them for something from the lists above.
- **RANDOM SEEDS** appears when nothing was recognised: the takes are then random.
- **The percentage** appears when you hover over a take. It says how close it is to the words that can be measured. It does not grade how good it sounds, so listen to the weaker takes too.

Two quirks of the percentage:
- Room and reverb words set the effect directly and do not count. `viel Raum` on its own therefore always shows 100 %, however it sounds.
- A low number often means the words are hard to combine. In testing, `langsamer dreckiger Drone ohne Hall` only reached 26 %, `dunkler weicher Bass` 87 %. The exact numbers change from click to click.

## When it doesn't fit

- **Too vague:** name a kind of sound. gensexuell doesn't know `beautiful`, but it does know `warm pad`.
- **Too much at once:** three good words usually hit better than eight that contradict each other.
- **Almost right:** take the best take and drag branches from there. Words for the direction, branches for the finish.
- **Wrong register:** the kind of sound only picks the note the search runs on. You can play every take on any key.
- **Ring modulation, wavefolding and stepped pitch** are easiest to reach directly in the gene editor, see the manual, chapter 10.
