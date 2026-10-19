# Two-part mode: design and what was built

**Status: built in CYBERDECK** (section 12). The design below is as it was agreed; where the build differs, section 12 says so.

**Goal (from you):** play a sequence in one sound, then move on and play something in a different sound on top. A **different patch for the sequence/arpeggiator and for the keys**.

This is a design for **CYBERDECK first**. TRIPDECK and MLdeck share its engine and would follow once it works (section 8).

## 1. How it works today (checked in the code)

- **One patch drives everything.** Every voice reads the same 24 numbers (`P[]`): oscillators, filter, envelope, LFO, drive, crush, delay. The sequence, the arpeggio and your keys all go through them.
- **One LFO and one effects chain** (crusher, delay, limiter) for the whole mix.
- **Turning PRESETS replaces the patch for everyone**, and it also cuts every sounding voice (it sends a "panic").
- **Mono patches use voice 0 for everything**, so a sequence and a lead on a mono patch cut each other off.
- **Four voices** in all (`NV = 4`).
- **The saved settings block is 236 of its 256 bytes**, so a second full patch (24 bytes) does not fit there.

## 2. The idea in one picture

Two **parts**:

| Part | Plays | Sound |
|---|---|---|
| **A: keys** | what you play on the panel keys, and a MIDI keyboard (when the arpeggiator is off) | its own patch |
| **B: layer** | the **sequencer** and the **arpeggiator** | its own patch |

Not in two-part mode, nothing changes: one patch, as now.

## 3. How you use it (recommended)

The least to learn: **send the current sound to the layer, then pick another sound for the keys.**

1. Choose a sound you like for the sequence (say CHROME BASS) and record or start the sequence.
2. **Hold SEQ for about half a second: LAYER ON.** The current sound is copied to the layer part. The top line shows both: `A NEON RAIN  B CHROME BASS`.
3. Turn **PRESETS** and the knobs as usual: now they change only **part A, the keys**. The sequence keeps playing its bass.
4. To change the sound of the layer, **hold ARP for half a second: SWAP**. The keys and the layer exchange sounds, so the knobs now edit the layer's patch. Hold ARP again to swap back.
5. **Hold SEQ again: LAYER OFF.** The layer follows the keys again (one sound, as before).

Why these gestures: hold-to-trigger needs no new button (every button is already used), a tap on SEQ or ARP does what it does today, and it works the way HOME's long press (panic) already does. Alternatives are in section 9.

**One side effect to know about:** ARP currently acts the instant you press it (on, hold, off). To tell a tap from a hold, its normal action would move to the moment you **release** it (as HOME's tap already does), so it reacts a fraction of a second later than now. SEQ's tap only opens the sequencer page, so it can stay on the press.

**What stays shared:** tempo (BPM), swing, pattern length, transpose, arpeggiator settings and the recorded pattern are one set for both parts.

### What each action does while LAYER is on

| Action | Result |
|---|---|
| **PRESETS** / knobs / pages | Edit part A only |
| **ALGORITHM / SELECT** (cutoff, resonance) | Part A only |
| **HOME double-tap (mutate)** | Part A only |
| **Hold HOME (panic)** | Stops everything, both parts |
| **PLAY / REC / sequencer** | Play and record part B's line, as now |
| **Arpeggiator on** | Arpeggio plays in part B's sound; the keys then feed the arp instead of sounding (as today) |
| **Notes from a MIDI keyboard** | Part A (or the arp, if on) |
| **Changing the patch** | Cuts only part A's notes; the sequence keeps going (today it cuts everything) |
| **SAVE** | Saves A's patch, the settings and pattern, and (see section 6) B's patch |
| **MIDI out over USB** | Part A on channel 1, part B on channel 2 (so a DAW or another synth can take the layers apart). One-part mode stays on channel 1 as now |

## 4. How the sound engine changes

The voices already work out their filter, envelope and pitch numbers from the patch **once per voice per 16 samples**, so giving each voice **its own patch** is mostly pointing it at a different set of 24 numbers: almost no extra work.

| Piece | Today | Two-part mode |
|---|---|---|
| Patch parameters | `P[]` read by every voice | Each voice has a part tag (A or B) and reads **its part's** 24 numbers |
| LFO | one, global | **one per part** (own rate, shape, depth, phase) |
| Bit crusher | one, on the mix | **one per part**: the two parts are mixed on two buses, crushed separately, then added |
| Delay | one line | **one shared line** (32768 samples per side, no extra memory). Its **time and feedback follow part A**; each part has its own **delay send** (the DLY MIX knob). So a clean lead can have no echo while the bass has some |
| Limiter, master volume | one | one, on the summed mix |
| Glide and mono mode | global | per part: a mono part uses **its own reserved voice** (so a mono bass and a poly lead no longer fight) |

### Voices (4 in total, shared)

- **Voice 0 is reserved for part A, voice 3 for part B**; voices 1 and 2 are shared.
- A part can steal only from itself first; it takes a shared voice before touching the other part's reserved one.
- With a **mono** patch, that part plays only on its reserved voice (legato within the part).
- This means a bass sequence plus a three-note chord on the keys works (B: 1 voice, A: up to 3), and a sequence plus an arpeggio plus keys on poly patches still shares four voices as it does now.
- Six voices would be possible (cost in section 7) but we should read the on-screen CPU figure on the unit first.

## 5. What is *not* in the first version

- **Different delay time or feedback per part** (shared line follows A). A second delay line costs about 23 % more processor and 128 KB of memory (section 7).
- **A separate pattern per part.** There is still **one** sequence (and the arp). Layers in different sounds come from sequence + arp + keys, not from a second recorded line.
- **More than two parts.**
- Anything in TRIPDECK's and MLdeck's extra engines (effects such as the reverb, the motion system).

## 6. Where the layer's patch is stored

Part B's patch is **24 bytes**. The 256-byte settings block has only about 20 bytes free, so two choices:

1. **Store only which patch slot (1-8) the layer uses** (1 byte, fits). Simple, but if you edited the sound before locking it, the edits are lost on power-off.
2. **Use a second flash sector** (4 KB), the way MLdeck and GroveDeck already do (`project_load` / `project_save` at `0xFB000`). It holds the layer's exact patch (edits included). It would also give room for **many more patches than 8** (a sector holds about 160 of them), which is useful on its own for the "100+ sounds" question. Costs a small platform change (the same one MLdeck uses) and one more flash write when you save.

**Recommended: option 2.**

## 7. Cost: voices and processor

Measured in the simulator on the real engine (the same C code that runs on the unit), timing one block of 256 audio frames, **minimum of three runs** per case. These are **host (PC) timings, so only the ratios mean anything**; the unit's own load must be read from the on-screen `CPU` figure after a trial build.

| Case | Time per block | What it tells us |
|---|---|---|
| Idle: effects, LFO and limiter only | 1.95 µs | the shared part, about **23 %** of a full chord |
| One voice sounding | 3.6 µs | |
| **Each extra voice** | **about 1.65 µs** | about **19 %** of a four-voice load |
| Four voices (a chord) | 8.5 µs | the baseline: voices are about 77 %, the shared effects 23 % |
| Same, but the LFO block run twice | 8.4-8.8 µs | **a second LFO costs nothing measurable** (inside the noise of about 2 %) |
| Same, with CRUSH at maximum | 8.8-8.9 µs | **a crusher costs about 2.5 %** |

What that gives for the design (percent of today's four-voice load):

| Change | Extra processor |
|---|---|
| Per-voice patch pointer, second LFO | about 0-2 % |
| Second crusher and a second bus | about 3 % |
| Delay sends (shared line) | under 1 % |
| **Two-part mode as designed** | **about 4-8 % in total** |
| A whole second effects chain (own delay line) | about +23 % and +128 KB of pool |
| Six voices instead of four | about +38 % |

**Memory:** two patches of 24 bytes, a second LFO's state and a second mix bus: well under 1 KB of RAM; the image grows by a few hundred bytes. The pool (161,792 of 344,064 bytes in use) is unaffected unless we add a second delay line.

**The open number is the unit's real load with four voices.** The manual says "well under half" in testing. If the on-screen figure with a four-note chord is, say, 30 %, two-part mode would add 1-2 points and six voices about 11 points. Please read that figure on your unit (hold a four-note chord on NEON RAIN and look at the bottom line) so we can decide about six voices on facts.

## 8. TRIPDECK and MLdeck

- **MLdeck** has the same voice engine plus a per-parameter motion system (each parameter has its own LFO). Per-part motion means a second set of motion state: more memory and a bit more processor than CYBERDECK, to be sized when we get there.
- **TRIPDECK** adds voice engines (pad, sitar, organ, bell), the reverb, the spiral echo and the vowel filter. Two parts there means two engine choices and probably a shared reverb; that needs its own cost check.
- Plan: build and test CYBERDECK first, then decide per project.

## 9. Alternatives we considered

| Idea | Why not the default |
|---|---|
| Hold **ARP + SEQ together** for LAYER (and swap with a different chord) | Needs a short delay on both buttons to tell a chord from a single press; easy to trigger by accident |
| A **PART knob** on a new page | The pages and knobs are full; a ninth tab does not fit on the screen |
| Choose **A or B explicitly** with a button every time | More controls to learn than "send this sound to the layer" |
| A second recorded pattern per part | A bigger change (two patterns, storage, UI); better as a second step |

## 10. How we would build and test it (when you say go)

1. **Regression first:** with two-part mode **off**, the sound must be **bit-identical** to today's (the project has golden-style host tests for this). That keeps everything you have working.
2. Per-voice patch pointer, per-part LFO and crush, the shared delay with sends, reserved voices, per-part mono. Host tests with scripted layering (a sequence plus keys in different patches, mono against poly, swap, change patch while playing).
3. The gestures (long-press SEQ and ARP), the A/B top line, MIDI-out channel 2, SAVE with the second sector.
4. Build with the brick-protection checks; **you** flash and read the CPU figure.
5. Update the manual and the sequencer guide.

## 11. Decisions (chosen)

1. **Gesture:** hold SEQ = layer on/off, hold ARP = swap.
2. **Effects:** one shared delay line with a send per part and a crusher per part.
3. **Voices:** four.
4. **Storage:** a second flash sector (`0xFB000`) for the layer's exact patch.

## 12. What was built (CYBERDECK, in `firmware/src/cyberdeck.c` and `platform.c`)

- `PB[]` (the layer's patch), `layer`, a per-voice `part` tag; each voice reads its part's patch, LFO and bus. The layer flag is latched once per engine tick (`lay_now`), so one tick sees one state.
- **Layer off is bit-identical to before:** all nine reference host scripts (demo, seq, arp, midi, mutate, scenes, scenes_stress and two sequencer runs) give the same WAV and the same trace as the build before the change. Checked with `md5sum` against saved baselines.
- **Voices:** with the layer on, voice 0 is the keys', voice 3 the layer's, 1 and 2 are shared, and a part never takes a voice the other part is holding. A mono part plays on its reserved voice.
- **Effects:** with the layer on, each part is crushed by its own patch; the delay's time and feedback come from part A and each part has its own send (DLY MIX), returning at full level. With the layer off the original single path runs (`send = 128`, `return = DLY MIX`).
- **Notes that the switching catches:** a note that started in one part and ends after the layer flag changed ends in the part it started in; turning PRESETS cuts only the keys' notes while the layer is on (a panic with a part number: 0 all, 1 keys only, 2 layer only).
- **Gestures:** hold SEQ (600 ms) toggles the layer; hold ARP (600 ms, layer on) swaps the patches under `irq_save` and ends both parts' notes. With the layer on, ARP's tap acts when the button is let go; with it off it acts on the press, as before.
- **Storage:** `project_load/project_save` (a CRC'd record in the sector below the settings) hold `{layer, curB, editedB, PB[24]}`; written with the settings after SAVE (when silent), read at start-up (values above 127 are rejected). The OTA range check allows that one extra sector.
- **Display:** `A1 NEON RAI B2 CHROME B` on the title line (names cut to fit), `*` marks an edited patch for each part. MIDI out: part B on channel 2.
- **Per-part LFO:** while there is no layer the layer's LFO follows the keys' phase, so a layer starts as an exact copy.

### Tests (`tests/layer*.txt`, `tests/check_layer.py`: 17 checks)

Layer on/swap/off timings; the sequence stays on its time grid through a patch change on the keys; the layer's mono bass on voice 3 only; a three-note chord on voices 0-2; the keys' patch change does not stop the layer's note; panics per part; mono keys against a poly layer after the swap; MIDI out channels; layer off tags nothing; the arp plays in the layer and MIDI in plays part A; a power cycle (the host build keeps the flash sectors in a file when `FM1_HOST_STORE` is set); and both parts with the same patch are as loud as one part (the waveform cannot be compared sample for sample: a note lands on another voice, whose oscillators are in another phase). Also run under AddressSanitizer and UndefinedBehaviorSanitizer: clean.

### Measured cost (host timing, minimum of three runs of a 256-frame block; only ratios mean anything)

| Case | Before | After (layer off) |
|---|---|---|
| Idle (effects, LFO, limiter) | 1.95 µs | 2.06 µs |
| One voice | 3.6 µs | 3.7 µs |
| Four voices | 8.5 µs | 8.8 µs (+3 %) |
| Four voices, CRUSH at maximum | 8.8 µs | 9.0 µs |
| Layer on: a 3-note chord on the keys plus a mono bass in the layer | n/a | 6.4-6.9 µs |

So layer off costs about **3 %** and a layered mix costs about what the same number of voices cost. Flash image: 248,776 B of 581,564; RAM 19,344 B of 98,304; pool unchanged. **Still to read on the unit:** the on-screen `CPU` figure with four voices sounding, with the layer on and off.

### Not done (and why)

TRIPDECK and MLdeck keep one patch (their engines are larger; each needs its own cost check). No separate delay time per part, no second pattern, no more than two parts (section 5).
