# CYBERDECK user manual

A four-voice subtractive synthesizer with an arpeggiator, a 32-step sequencer and a patch mutator, for the M-VAVE FM-1.
Neon on the screen, saw waves in the speakers.

Package identity `FM-1_960` · GPL-3.0-only · built on Felucca (Leo Kuroshita), SLOOP (isod89) and Dinghy (zvenson).

---

## 1. Quick start

1. Play the keys. The default patch is **NEON RAIN**, a slow pad.
2. Turn **PRESETS** to hear the other seven patches.
3. Turn **ALGORITHM** (cutoff) and **SELECT** (resonance) to shape the sound live.
4. Press **PLAY** to hear the built-in bassline. Hold a chord with **ARP** on to arpeggiate it.
5. Tap **HOME** to see every page as a neon scene, and tap it twice for a surprise (mutate).

## 2. The screen

| Area | What it shows |
|---|---|
| Top | CYBERDECK logo; below it the preset number and name (a `*` means edited since loaded); on the right the tempo, `ARP` / `HLD` (arp hold), `>` (playing), `REC` (armed) |
| Scope | A live oscilloscope of the output (on the SEQ page it becomes the step grid) |
| Four bars | The four knobs of the current page, with their values. The last knob you turned is highlighted in yellow |
| Tab row | OSC FLT ENV LFO FX GLO ARP SEQ, the current page lit |
| Bottom line | `CPU` load, `POLY`/`MONO`, octave, and short messages (SAVED, MUTATED, REC...) |

That is the **parameter view**. **HOME** (one tap) switches every page to its **scene** (section 2.1), and back.

On switching on, a loading screen (the logo over a neon sunset) runs for about two and a half seconds while the settings load. Any button skips it.

### 2.1 The scenes

Every page has a scene: a picture drawn in EffectCraft (the plates, `assets/art/`), lit in neon, with the page's four knobs drawn over it
as light. The four knobs and their values are along the bottom, the knob you just turned and its value on the top line. The keys,
knobs and MIDI work exactly as in the parameter view; HOME remembers the view with **SAVE**.

| Page | Scene | What is drawn over it |
|---|---|---|
| **OSC** | rainy city | The oscillators' wave: saw, pulse and sub as they add up (MIX, PULSE W, SUB), the detuned one beating behind it (DETUNE) |
| **FLT** | grid corridor | The filter's curve: cutoff, resonance peak, the 12 dB slope |
| **ENV** | rain | The amplitude envelope (ATTACK, DECAY, SUSTAIN, RELEASE), with a light travelling along it |
| **LFO** | wobbling lines | The LFO's shape and depth, how many cycles fit (RATE), its target in the corner |
| **FX** | digital glitch | The echoes: ping-pong taps that die away (DLY MIX, DLY TIME, FEEDBACK). CRUSH reduces the picture's colours |
| **GLO** | radar | A sweeping arm; a blip for every sounding note |
| **ARP** | neon mesh | The chord as a ring of lights; the note that is sounding burns |
| **SEQ** | sunset | The pattern as a skyline: bar height is pitch, yellow bars are accents, the bright one is the playhead |

The picture follows what you play: every note flashes and heats the colours and tears bands of the picture sideways with a red/blue split,
the grid floors fly towards you faster the louder it is, and the palette ripples all the time.

## 3. Controls

| Control | Function |
|---|---|
| 27 keys | Play notes (F3 to G5, shifted by the octave) |
| **OCT-** / **OCT+** | Octave, plus or minus two. Remembered when you save |
| **MASTER** knob | Volume |
| **EDIT SEL ENV LFO FX GLO** | Open the OSC, FLT, ENV, LFO, FX and GLO pages |
| **ARP**, **SEQ** | Arpeggiator and sequencer pages (see sections 6 and 7). **Hold SEQ** (half a second): the layer on or off. **Hold ARP**: swap the two parts (section 7.1) |
| **KNOB 1-4** | The four parameters on the page. Slow turns are fine steps, fast turns jump |
| **ALGORITHM** | Cutoff, on every page |
| **SELECT** | Resonance, on every page |
| **PRESETS** | Choose one of 8 patches |
| **SAVE** | Store your work (section 8) |
| **PLAY**, **REC** | Transport and record (section 7) |
| **HOME** | Tap: parameters <-> scenes. Double-tap: mutate the patch. Hold: panic (all notes off). While step recording: a tap enters a rest |

## 4. The sound engine

Each of the four voices: **saw** oscillator + **pulse** oscillator (detunable, with pulse width) + a **sub** square one octave down, into a resonant
**state-variable low-pass filter**, then a **drive** stage, then the **amplifier envelope**.

- The filter has its own envelope. It decays with the **DECAY** time and moves the cutoff by **ENV AMT** (positive opens, negative closes).
- The filter follows the keyboard about half a semitone per semitone, so high notes stay bright.
- **Velocity** (from MIDI, or the accent in the sequencer) scales the volume by **VELOCITY** amount. The panel keys play at a fixed medium velocity.
- An output **limiter** keeps loud patches from clipping.

### 4.1 Pages and parameters (all 0 to 127 unless noted)

**OSC** - the oscillators

| Knob | Parameter | What it does |
|---|---|---|
| 1 | MIX | Balance between the saw (0) and the pulse (127) |
| 2 | PULSE W | Pulse width. 64 is a square. Extremes are thin and nasal |
| 3 | DETUNE | Detunes the pulse against the saw, up to about 85 cents. A fat chorus effect |
| 4 | SUB | Level of the sub-oscillator |

**FLT** - the filter

| Knob | Parameter | What it does |
|---|---|---|
| 1 | CUTOFF | Filter frequency, 20 Hz to 18 kHz, exponential. Also on the ALGORITHM knob |
| 2 | RESO | Resonance. High values ring and squeal. Also on the SELECT knob |
| 3 | ENV AMT | Filter envelope depth, -64 to +63 (shown signed) |
| 4 | DRIVE | Saturation after the filter, from clean to heavily overdriven |

**ENV** - the amplifier envelope (ADSR)

| Knob | Parameter | What it does |
|---|---|---|
| 1 | ATTACK | 1 ms to 8 s |
| 2 | DECAY | 1 ms to 8 s. Also sets the filter envelope's decay |
| 3 | SUSTAIN | Level held while a key is down |
| 4 | RELEASE | 1 ms to 8 s |

**LFO** - one low-frequency oscillator

| Knob | Parameter | What it does |
|---|---|---|
| 1 | RATE | 0.05 Hz to 30 Hz |
| 2 | DEPTH | How much the LFO moves its target |
| 3 | DEST | Target: **CUTOFF**, **PITCH** (vibrato, up to about two semitones) or **PWM** (pulse width). Three zones across the knob |
| 4 | SHAPE | **TRI**, **SAW**, **SQR**, or **S&H** (sample and hold: random steps, the "glitch" setting). Four zones |

**FX**

| Knob | Parameter | What it does |
|---|---|---|
| 1 | CRUSH | Bit crusher. Lowers the bit depth and the sample rate together for digital grit |
| 2 | DLY MIX | Level of the delay |
| 3 | DLY TIME | About 35 ms to 740 ms. Changing it while sound plays smears the repeats like a tape |
| 4 | FEEDBACK | Number of repeats. The delay is **ping-pong** (left to right) and each repeat is darker than the last |

**GLO** - global voice behavior

| Knob | Parameter | What it does |
|---|---|---|
| 1 | GLIDE | Portamento time. Works in **MONO** mode only |
| 2 | MODE | **POLY** (four voices) or **MONO** (one voice, legato: playing a new note while another is held glides instead of retriggering) |
| 3 | TUNE | Fine tuning, about +/- 47 cents (center is 0) |
| 4 | VELOCITY | How much velocity affects volume |

Changing MODE silences held notes.

## 5. Presets

| # | Name | Character |
|---|---|---|
| 1 | NEON RAIN | Slow, wide pad with a filter that breathes and a long ping-pong delay |
| 2 | CHROME BASS | Mono bass with a sub, a resonant filter bite, and a little glide |
| 3 | KANEDA | Mono lead with vibrato and a delay tail |
| 4 | ZAIBATSU | Dark stabs with heavy resonance, a slow filter LFO, and crush |
| 5 | GLITCH ICE | Random filter steps (sample and hold) on a short pluck, crushed |
| 6 | TYRELL | Slow-attack brassy chord sound |
| 7 | WIRED | Short pluck made for the arpeggiator, with a ping-pong delay |
| 8 | DATA MOSH | Mono wobble bass, square LFO, heavy crush and drive |

## 6. Arpeggiator

*New to the arpeggiator and sequencer? A step-by-step guide with answers to the common questions (rests, note length, layers) is in [ARP-AND-SEQUENCER.md](ARP-AND-SEQUENCER.md).*

Press **ARP** to switch it on and open its page. Hold notes and they play one after another.
Pressing **ARP** again while on its page steps through **ON** (`ARP`), **HOLD** (`HLD`), **OFF**.

- **HOLD** keeps the chord playing after you let go. Playing a new chord replaces it.
- With the arp on, the keys control the arp only and do not play notes directly.

| Knob | Parameter | Choices |
|---|---|---|
| 1 | ARP MODE | **UP**, **DOWN**, **UP-DN**, **RAND** |
| 2 | OCTAVES | 1, 2 or 3: the chord repeats an octave higher (the notes still play low to high) |
| 3 | RATE | **1/4**, **1/8**, **1/16**, **1/16T** (triplet), **1/32** |
| 4 | GATE | Note length as a percentage of the step (about 12 to 99 %) |

The tempo, gate and swing are shared with the sequencer (see below).

## 7. Sequencer, transport and recording

*See [ARP-AND-SEQUENCER.md](ARP-AND-SEQUENCER.md) for a simple walk-through (how to put in rests, change note length, overdub, layers).*

The sequencer is a mono pattern of up to **32 steps** (sixteenth notes). **SEQ** opens its page and the step grid
(bar height is pitch, yellow bars are accents, the bright frame is the playhead).

| Knob | Parameter | Meaning |
|---|---|---|
| 1 | BPM | 60 to 187 |
| 2 | SWING | 50 % (straight) to 75 %. Delays every second step |
| 3 | LENGTH | Pattern length, 1 to 32 steps |
| 4 | TRANSPOSE | -24 to +24 semitones, applied at playback |

- **PLAY** starts and stops the pattern. The first step sounds immediately. The PLAY light blinks on the beat.
- **GATE** (on the ARP page) also sets the note length of the sequence.
- Sequencer notes use the current preset, so mutating or changing patches while it plays is part of the fun.
- A default A-minor bassline is loaded the first time you switch the firmware on.

### Recording

**REC** arms recording (its light and `REC` on the title show it).

- **Step record** (arm while stopped): each note you play goes into the next step. **HOME** inserts a rest.
  Press **REC** again to finish: the length is set automatically. Arming starts from an empty pattern.
- **Overdub** (arm while playing): notes you play are placed at the nearest step, replacing what was there.
  Playing hard (MIDI velocity 110 or more) marks the step as an accent.
- Recorded notes are stored relative to TRANSPOSE, so changing TRANSPOSE later shifts the whole line.

### 7.1 Two parts: a different sound for the sequence and the keys

Normally one patch plays everything. In **two-part mode** the keys and the sequencer/arpeggiator each have a patch:

| Part | Plays | Patch |
|---|---|---|
| **A** (keys) | the panel keys and a MIDI keyboard (when the arp is off) | the one PRESETS and the knobs edit |
| **B** (layer) | the **sequencer** and the **arpeggiator** | its own, shown as `B2 CHROME B` on the title line |

How to use it:

1. Choose the sound you want for the sequence (say CHROME BASS) and start it with **PLAY** (or turn the arp on).
2. **Hold SEQ** for half a second: `LAYER ON`. The current sound is copied to the layer. The title shows `A1 NEON RAI B2 CHROME B`.
3. Turn **PRESETS** and the knobs: they edit **part A** (the keys) only. The sequence goes on in its sound. A patch change no longer cuts the sequence.
4. To change the layer's sound: **hold ARP** (half a second): `SWAPPED`. The keys and the layer exchange patches, so the knobs and PRESETS now edit the layer's. Hold ARP again to swap back.
5. **Hold SEQ** again: `LAYER OFF`. The layer stops and the sequencer and arp play the keys' sound again.

What is separate for each part: the whole patch, its own **LFO** (own phase), **crusher**, **MONO/POLY** and glide, and its **delay send** (the DLY MIX knob).
What is shared: the **delay's time and feedback** (part A's), tempo, swing, length, transpose and the arp settings, and the recorded pattern.
The four voices are shared: voice 1 is the keys' and voice 4 the layer's; the two in between go to whichever part needs them, and a part never takes the other part's sounding note. A mono part plays on its own voice, so a **mono bass in the sequence and a poly chord on the keys** work together.
**SAVE** keeps the layer's exact patch as well, in a second flash sector (it comes back when you switch on). **MIDI out** (USB): part A on channel 1, the layer on channel 2 (one-part mode stays on channel 1). **Hold HOME** (panic) stops both parts.
**ARP** now acts when you let go of the button (it has to tell a tap from a hold) while the layer is on; with the layer off it acts at once, as before.

## 8. Saving

**SAVE** writes to the flash:

- your current settings into the **current preset slot**, replacing what was in it
- the global settings (arp, tempo, swing, length, transpose)
- the **pattern**
- the octave
- the view (parameters or scenes)
- the **layer** (two-part mode, section 7.1): on or off, and its patch (a second flash sector)

Notes:

- The write happens about a second later, **when nothing is sounding and the sequencer is stopped**, because it briefly stalls the audio.
- A `*` after the preset name means it differs from the saved version. Turning **PRESETS** away and back reloads the saved version.
- Tempo, arp, swing, length and transpose are **global**: they are not part of a preset, and one value is shared by all patches.
- Saving over a slot overwrites the original factory patch. Reflashing the firmware (or a first start after an update that changes the storage format) restores all the factory patches.

## 9. Mutate

Tap **HOME** twice (quickly) for a random walk of the current patch: four to six knobs move a little, and the LFO's target and shape may jump.
Every knob has limits so the result stays playable (no eight-second attacks, the filter never shuts fully, feedback stays safe).
Mode, tuning and the global settings are never changed.

It works while you play, so you can mutate and listen. If you like it, **SAVE**.

## 10. MIDI and USB

### Playing from an external keyboard (MIDI in)

There are two ways in, and the synth cannot tell them apart:

- **The 3.5 mm TRS jack (MIDI in).** Connect a keyboard's 5-pin DIN **MIDI OUT** to the jack with a DIN-to-TRS cable or adapter of **type A**. The jack is an input only: the FM-1 has no MIDI out jack. (This input is new and not yet tried by us on a unit; see Trouble.)
- **USB-MIDI** from a computer or phone. A USB keyboard cannot plug into the FM-1 directly (its USB port is device-only), so connect it to the computer or phone, or use a small USB-host box.

What it understands, on **any channel**:

| Message | What it does |
|---|---|
| Note on / off | Plays the notes, with velocity. A chord played at once sounds in full. With the arpeggiator on, held notes are arpeggiated; with REC armed, notes are recorded |
| Pitch wheel | Bends the pitch, about two semitones up and down |
| Program change 0-7 | Selects the patch (1-8) |
| CC 123, CC 120 | All notes off (panic) |
| CC 74 / 71 / 73 / 72 | Cutoff / resonance / attack / release |
| CC 1 | LFO depth (the mod wheel) |
| CC 91 | Delay mix |

Not handled yet: sustain pedal (CC 64), aftertouch, MIDI clock (Start/Stop/Clock bytes are received but ignored).

### MIDI out (USB)

The FM-1 sends over **USB-MIDI** (channel 1), so a computer or phone can record or play another instrument from it:

- the notes you play on the panel keys (with the arpeggiator off);
- the arpeggiator's notes, when it is on (instead of the raw keys);
- the sequencer's notes while it plays;
- an **all notes off** (CC 123) when you change patch or hold HOME for panic.

### USB audio

The FM-1 appears as a stereo audio input (16-bit, 44.1 kHz) to your computer, so you can record it directly.

## 11. Trouble

- **Crash**: a red screen shows where, then the FM-1 restarts. After two boot crashes it enters **USB rescue** by itself.
- **Won't start / want another firmware**: hold **OCT-** while switching on (USB rescue), then install from the FM-1 Firmware Hub in Chrome or Edge (https://fm1.designburgapps.com), or run `.\fm1.ps1 install <project>` from the project folder.
- **Choppy picture or sluggish keys in the scenes**: switch back to the parameters (tap HOME); the sound is not affected either way.
- **The TRS jack does nothing**: check the cable is a DIN-to-TRS **type A** one (the polarity differs from type B), that it goes to the keyboard's MIDI OUT, and that it is plugged into the MIDI IN jack. Notes arriving over USB-MIDI still work either way. If you find it still silent, tell us which keyboard and cable.
- **Crackling**: watch the CPU figure on the bottom line (parameter view). If it stays near 100 %, reduce the number of notes or the drive. (In testing it stays well under half.)
- Installing firmware is at your own risk. Back up presets and projects before switching firmwares.

## 12. Credits and license

CYBERDECK is free software under the GPL-3.0-only. Its platform is by Leo Kuroshita (Hugelton Instruments) and isod89 (SLOOP),
and its starting point is Dinghy by zvenson. If you pass it on, pass the source on too.
