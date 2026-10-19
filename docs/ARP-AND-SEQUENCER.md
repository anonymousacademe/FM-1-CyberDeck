# The arpeggiator and the sequencer: a simple guide

For **CYBERDECK**, **TRIPDECK** and **MLdeck** (they share the same arpeggiator and sequencer), and **GROOVEDECK** (its drum sequencer is in section 5). **DoppleDecker** has neither yet.

Every number below was checked in the simulator on the three synths (they behave identically).

## Your questions, answered first

| Question | Answer |
|---|---|
| **Can I put a rest between notes?** | **Yes**, while step recording: tap **HOME** where you want the silence. Each rest takes one step. (Section 2.) |
| **Can I change how long a note lasts?** | **Only for the whole pattern, not note by note.** The **GATE** knob (ARP page, knob 4) sets every note of the sequence and arpeggio from about 12 % to 99 % of a step: short and staccato to nearly connected. No ties, no per-note lengths. (Section 3.) |
| **Can I play a sequence in one sound, then add another layer in a different sound?** | **In CYBERDECK, yes: two-part mode.** Hold **SEQ** to put the sequence (and the arpeggio) in their own sound, then choose another sound for the keys (section 4). TRIPDECK and MLdeck still have one patch for everything. |

---

## 1. The basics (CYBERDECK, TRIPDECK, MLdeck)

- The sequencer plays **one pattern**: **one melody line, one note per step**, 1 to 32 steps. A step is a sixteenth note.
- **PLAY** starts it at once from step 1 and stops it. The PLAY light blinks on the beat.
- **REC** records into the pattern (two ways, section 2).
- The **SEQ** button opens the sequencer page. Its four knobs:

| Knob | What it does |
|---|---|
| 1 BPM | Tempo, 60 to 187 (shared with the arpeggiator) |
| 2 SWING | 50 % (straight) to 75 %: every second step comes late |
| 3 LENGTH | How many steps the pattern has (1 to 32) |
| 4 TRANSPOSE | Shifts the whole line up or down, -24 to +24 semitones |

- The first time you switch on, a short bass line in A minor is already there, so PLAY makes sound at once.
- **SAVE** keeps the pattern together with your patch and settings (there is only one pattern).
- The sequencer plays the **current patch**. Turn **PRESETS** while it plays and the same line carries on in the new sound (one note is cut at the change).

## 2. Recording a sequence

### Step recording (stopped): the way to put in rests

1. With the sequencer **stopped**, press **REC** (its light comes on).
2. Press the keys one after another. **Each key press is the next step**, however long you hold it and however long you wait between presses, so timing does not matter.
3. For a **rest**, **tap HOME** (a quick tap). It takes one step and shows as silence.
4. Press **REC** again to finish. **LENGTH is set for you** to the number of steps you entered. If you want silence at the end of the loop, tap HOME for it *before* you press REC.
5. Press **PLAY**.

**Careful:** arming REC while stopped **wipes the old pattern** as soon as you play the first key or tap the first rest. (If you press REC and then REC again without playing anything, nothing is lost.) You can enter at most 32 steps.

Example, recorded as: key C, **rest**, key G, **rest**, key C#. It plays C, silence, G, silence, C#, then loops. We measured this: the notes start 250 ms apart with a 125 ms gap each time (at 120 BPM).

### Overdub (playing): add or replace notes

1. Start the pattern with **PLAY**.
2. Press **REC** while it plays.
3. Play keys. Each lands on the **nearest step** and **replaces** what was there. You hear your note and the pattern at once.
4. Press **REC** again to stop.

Overdub **cannot erase** a step or make a rest in a recorded pattern, and the length does not change. To get a rest where there is a note, record the pattern again with step recording.

### Accents

A step is an **accent** (it plays at full level) if it was recorded with a **hard hit from a MIDI keyboard** (velocity 110 or more). The panel keys always play at one medium velocity, so they cannot accent.

### What the sequencer cannot do (yet)

No ties or long notes, no different length per step, no chords inside one step, no deleting or inserting a single step, no second pattern. Re-record to change the shape.

## 3. Note length

The sequence and the arpeggio both use **GATE**: **ARP page, knob 4** (press ARP to get there; see below). At 120 BPM a step is 125 ms and we measured:

| GATE | Each note lasts about |
|---|---|
| lowest (12 %) | 12 to 18 ms: very short |
| default (67 %) | 81 to 87 ms |
| highest (99 %) | 121 to 128 ms: nearly joined |

Other ways to change how it *sounds*: **RELEASE** on the ENV page (how long a note rings after it ends), a slower **BPM** (everything gets longer), or a **MONO** patch with **GLIDE** (notes slide into each other).

## 4. Arpeggiator and layers

### Using the arpeggiator

- **ARP** button: the first press turns it **on** and opens the ARP page. While it is on, your keys feed the arpeggiator: they do **not** play directly. Hold a chord and the notes play one after another.
- Press **ARP** again while on that page: **HOLD** (`HLD`): the chord keeps playing after you let go. A new chord replaces the old one. Press once more: **off**.
- Pressing ARP while you are on a *different* page only jumps to the ARP page (and turns the arp on if it was off).
- The ARP page knobs: **ARP MODE** (up, down, up-down, random), **OCTAVES** (1 to 3), **RATE** (1/4, 1/8, 1/16, 1/16 triplet, 1/32), **GATE** (note length, above).
- **HOLD gotcha:** with nothing held it replays the **last notes you pressed**, even ones from step recording. Hold **HOME** (panic) to clear it, or play a new chord.

### Layering: two-part mode (CYBERDECK)

CYBERDECK can give the **sequence and the arpeggio one sound and your keys another**:

1. Start the sequence in the sound you want for it (say CHROME BASS).
2. **Hold SEQ** for half a second: `LAYER ON`. The sound is copied to the layer.
3. Turn **PRESETS**: now only the **keys** change. The sequence plays on in its sound.
4. **Hold ARP** to swap (the knobs then edit the layer's sound); **hold SEQ** again to switch the layer off.

A mono bass in the sequence and a poly chord on the keys work together (each part has its own voice and its own MONO/POLY, LFO, crusher and delay send). MIDI out sends the layer on channel 2. Full description: [MANUAL.md](MANUAL.md) section 7.1; how it was designed and measured: [TWO-PART-MODE-DESIGN.md](TWO-PART-MODE-DESIGN.md).

### Layering in one sound (TRIPDECK, MLdeck, or CYBERDECK with the layer off)

You can have **three layers at once**, all in the *same sound*: the **sequence**, the **arpeggio** and the **keys you play** (or a MIDI keyboard). Tips:

- **Use a POLY patch.** The voices are shared (four in all): sequence, arpeggio and keys take turns. With a **MONO** patch (in CYBERDECK: CHROME BASS, KANEDA and DATA MOSH; any patch with MODE set to MONO on the GLO page), all three share **one voice** and cut each other off.
- **One sound for all layers** here: changing the patch changes it for every layer (in CYBERDECK, two-part mode above gives the sequence/arp and the keys a sound each). TRIPDECK's VOICE (pad, sitar, organ, bell) is also one engine at a time.

**Ways to get layers in different sounds today:**

1. **Record take by take.** Play the sequence and record the FM-1's **USB audio** into a computer (a DAW), then change the patch, play something else, and record again. Layer the takes there.
2. **Use MIDI out.** The sequencer's and arpeggiator's notes go out over **USB-MIDI on channel 1**, so the FM-1 can play one part while a computer, phone or another synth plays a different sound from the same notes.
3. **A second FM-1 or app** playing along (start both with PLAY).


## 5. GROOVEDECK (drum sequencer)

Different idea: **16 drums, each with its own row of 16 steps**, 8 patterns.

| Want to | Do this |
|---|---|
| Edit steps by hand | Press **SEQ** (STEP mode). Pick a drum (hold **SEL** + its pad, or turn the **SELECT** knob). The 16 keys become its 16 steps: each press goes **off, on, accent, off**. |
| A **rest** | A step that is off. Leave it empty. |
| Record by playing | Start **PLAY**, press **REC**, hit pads: each hit lands on the **nearest step**. Hits of velocity 110+ from a MIDI keyboard become accents (panel pads play at 100: use STEP mode for accents). |
| Make a **layer** | Every drum is its own row, so record the kick, then the hats, then the snare, one after another (REC while playing keeps what is there). All 16 drums can play at once. |
| Change a drum's **length** | The drum's own **DECAY** knob on the **EDIT** page (each drum has its own; the gated snare has GATE). A step is just a hit. |
| Clear | Hold **CLEAR** (key 27) and press a pad to clear that drum's row, or a pattern key (17-24) to clear the whole pattern. |
| Silence or isolate a layer | Hold **MUTE** (key 25) or **SOLO** (key 26) and press a pad. |
| Swing, tempo, accent strength | **GLO** page: **BPM**, **SWING**, **ACCENT** (how much louder accents are), **REPEAT** (note-repeat rate). |
| Roll a drum | Hold **ARP** and the pad: the **note repeat** rolls it at the REPEAT rate. |
| Switch pattern | Keys **17-24**, or **OCT-/OCT+**. While playing it changes at the end of the bar. No chaining yet. |
| Keep it | **SAVE** (when stopped) stores all patterns, sounds, mixer and tempo. |

Patterns are always 16 steps; there are no per-step lengths or ratchets yet.

## 6. Cheat sheet (CYBERDECK, TRIPDECK, MLdeck)

| I want to... | Do |
|---|---|
| Record a melody with rests | Stopped: **REC**, keys (each = a step), **HOME** tap = rest, **REC**, **PLAY** |
| Add a note to a playing pattern | **PLAY**, **REC**, play the key near that step, **REC** |
| Make notes shorter / longer | **ARP** page, knob 4 (GATE) |
| Faster / swing / longer pattern / higher | **SEQ** page: knobs 1 / 2 / 3 / 4 |
| Arpeggiate a chord | **ARP**, hold the chord. Press **ARP** again to hold it, again to stop |
| Stop everything stuck | Hold **HOME** (panic) |
| Layers | Sequence + arpeggio + keys, in a **POLY** patch. Different sounds: CYBERDECK **hold SEQ** (two-part mode); elsewhere record take by take, or MIDI out |
