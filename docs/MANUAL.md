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
5. Tap **HOME** for a surprise.

## 2. The screen

| Area | What it shows |
|---|---|
| Top | CYBERDECK logo; below it the preset number and name (a `*` means edited since loaded); on the right the tempo, `ARP` / `HLD` (arp hold), `>` (playing), `REC` (armed) |
| Scope | A live oscilloscope of the output (on the SEQ page it becomes the step grid) |
| Four bars | The four knobs of the current page, with their values. The last knob you turned is highlighted in yellow |
| Tab row | OSC FLT ENV LFO FX GLO ARP SEQ, the current page lit |
| Bottom line | `CPU` load, `POLY`/`MONO`, octave, and short messages (SAVED, MUTATED, REC...) |

## 3. Controls

| Control | Function |
|---|---|
| 27 keys | Play notes (F3 to G5, shifted by the octave) |
| **OCT-** / **OCT+** | Octave, plus or minus two. Remembered when you save |
| **MASTER** knob | Volume |
| **EDIT SEL ENV LFO FX GLO** | Open the OSC, FLT, ENV, LFO, FX and GLO pages |
| **ARP**, **SEQ** | Arpeggiator and sequencer pages (see sections 6 and 7) |
| **KNOB 1-4** | The four parameters on the page. Slow turns are fine steps, fast turns jump |
| **ALGORITHM** | Cutoff, on every page |
| **SELECT** | Resonance, on every page |
| **PRESETS** | Choose one of 8 patches |
| **SAVE** | Store your work (section 8) |
| **PLAY**, **REC** | Transport and record (section 7) |
| **HOME** | Tap: mutate the patch. Hold: panic (all notes off). While step recording: enter a rest |

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

## 8. Saving

**SAVE** writes to the flash:

- your current settings into the **current preset slot**, replacing what was in it
- the global settings (arp, tempo, swing, length, transpose)
- the **pattern**
- the octave

Notes:

- The write happens about a second later, **when nothing is sounding and the sequencer is stopped**, because it briefly stalls the audio.
- A `*` after the preset name means it differs from the saved version. Turning **PRESETS** away and back reloads the saved version.
- Tempo, arp, swing, length and transpose are **global**: they are not part of a preset, and one value is shared by all patches.
- Saving over a slot overwrites the original factory patch. Reflashing the firmware (or a first start after an update that changes the storage format) restores all the factory patches.

## 9. Mutate

Tap **HOME** for a random walk of the current patch: four to six knobs move a little, and the LFO's target and shape may jump.
Every knob has limits so the result stays playable (no eight-second attacks, the filter never shuts fully, feedback stays safe).
Mode, tuning and the global settings are never changed.

It works while you play, so you can mutate and listen. If you like it, **SAVE**.

## 10. MIDI and USB

- **USB-MIDI in**: notes with velocity, on any channel. Control changes:

| CC | Parameter |
|---|---|
| 74 | Cutoff |
| 71 | Resonance |
| 73 | Attack |
| 72 | Release |
| 1 | LFO depth |
| 91 | Delay mix |

- **USB-MIDI out**: the panel keys send note on and off on channel 1, so the FM-1 also works as a controller.
- **USB audio**: the FM-1 appears as a stereo audio input (16-bit, 44.1 kHz) to your computer, so you can record it directly.

## 11. Trouble

- **Crash**: a red screen shows where, then the FM-1 restarts. After two boot crashes it enters **USB rescue** by itself.
- **Won't start / want another firmware**: hold **OCT-** while switching on (USB rescue), then install from the FM-1 Firmware Hub in Chrome or Edge (https://fm1.designburgapps.com), or run `.\fm1.ps1 install <project>` from the project folder.
- **Crackling**: watch the CPU figure on the bottom line. If it stays near 100 %, reduce the number of notes or the drive. (In testing it stays well under half.)
- Installing firmware is at your own risk. Back up presets and projects before switching firmwares.

## 12. Credits and license

CYBERDECK is free software under the GPL-3.0-only. Its platform is by Leo Kuroshita (Hugelton Instruments) and isod89 (SLOOP),
and its starting point is Dinghy by zvenson. If you pass it on, pass the source on too.
