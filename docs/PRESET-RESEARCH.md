# CYBERDECK: more factory presets, cyberpunk / sci-fi / Blade Runner / atmospheric

Research note, 2026-10-16. Nothing in `firmware/` was changed. Recipes were rendered on a scratch copy of the
committed engine (`git HEAD`) through the host build, see section 8 for what that proves and what it does not.

## 0. Short version

- **18 recipes (A) fit the CYBERDECK engine and were rendered on the host.** 16 of them are proposed (section 6), 2 are
  not worth a slot (they duplicate NEON RAIN / CHROME BASS / WIRED).
- **7 sounds are out of reach (B):** FM electric piano, glass bells, vocal choir, noise risers/wind, pitched sub drops, a real
  reverb tail and metallic percussion, plus 2 with no route at all (vocoder, saxophone). Each needs something the engine does not have.
- **A finding that changes the plan for (B):** neither sibling project can hand a sound to CYBERDECK.
  DoppleDecker's firmware cannot load a fitted bank yet (`DoppleDecker/docs/BANK.md`: "Not possible yet"), and its output
  is an FM patch, which CYBERDECK has no operators to play. MLdeck has the learned oscillator and noise, but it is a different firmware
  (its presets 9 to 16 are already the "futuristic" set: AURORA PAD, SUPERSAW, NEON BRASS, GLASS PLUCK, HYPER BASS, ORBIT CHOIR, STARDUST, VOID DRONE).
  So a (B) sound ends up in DoppleDecker or MLdeck, or in CYBERDECK only after a new engine feature.
- **Correction after looking at the Firmware Hub (section 10):** "out of reach" above means out of reach for *CYBERDECK's engine*, not for the FM-1.
  Other firmwares on the hub already run six-operator FM with DX7 import, a Juno chorus, a cathedral reverb, noise, a 12-bit sampler and many engines on this same chip.
  The (B) list is therefore a list of features to port or build, and the cheapest route for several of them is existing GPL code, not a sample match.
- **A finding that blocks "just add rows":** the settings block is 236 of the 256 bytes the platform allows
  (`set` in `cyberdeck.c`: 4 + 8 x 24 + 8 + 32). One more saved preset costs 24 bytes, so **no** extra preset fits as it is. See 6.2.
- **Decisions for you:** (1) how to store more than 8 presets (6.2), (2) which of the 16 to take, (3) whether a reverb is worth building (it
  would make most of the pad candidates much closer), (4) whether to make DoppleDecker's bank loadable, or give CYBERDECK a 2-operator FM voice,
  to bring in the FM electric piano and bells.

---

## 1. What the engine can and cannot do (verified against `app_audio`)

Quirks that decided the recipes. The manual does not mention most of them.

| # | Fact | Consequence |
|---|---|---|
| 1 | The filter envelope is **decay only**: it starts at full on every note and falls to zero with the DECAY time (`fenv`). ENV AMT above 64 opens the filter and then closes it. **Below 64 it starts closed and rises** to CUTOFF over the DECAY time. | A negative ENV AMT is the way to make a **swell** (brass, pad, riser). The swell time is the DECAY knob, which also sets the amp decay toward SUSTAIN. |
| 2 | One DECAY knob for amp and filter. | A held pad can keep SUSTAIN high while the filter still moves for seconds (DEC 100 to 125 = 1 to 5 s). |
| 3 | ENV AMT range is about +-4.8 octaves (+-126 table steps; the whole table spans 9.8 octaves in 255 steps). One table step is 0.46 semitone; key tracking adds 1 step per semitone from middle C. | On bass notes (C2 is 24 steps down) the cutoff is already 0.9 octave lower. Rule used: CUTOFF knob = (target index + 24) / 2 for the notes you play (index = 255 x ln(Hz/20) / ln 900). |
| 4 | One LFO, global, free-running, no delay or fade-in, not tempo-synced. Destinations are exclusive: cutoff **or** pitch **or** PWM. Pitch depth 127 is +-2 semitones; depth 7 is about +-12 cents. | Vibrato and PWM cannot be had together. No sidechain pump, no delayed vibrato. |
| 5 | The sub is a square one octave below. At SUB 127 it is about a third of the saw's level. | Big subs also need DRIVE or a low cutoff, not just SUB. |
| 6 | MIX is a crossfade saw to pulse, DETUNE only detunes the pulse (max 85 cents), PW 127 is a 13 % pulse. | No two-saw Reese, no octave-apart oscillators, no supersaw. A pulse at PW != 64 has a DC offset (0.02 to 0.04 of full scale measured on low notes, harmless but it costs limiter headroom). |
| 7 | Mono = voice 0 only; GLIDE works only there; a new key while one is held is legato. | A hoover "pitch dip" needs two overlapping keys, no pitch envelope. |
| 8 | Delay is ping-pong, 34 to 740 ms, darkened by a one-pole in the loop; it is the **only** space in the signal path. No reverb, no chorus. | Pads that rely on a Lexicon-style tail or a Juno chorus lose that part. DLY TIME 61 is a dotted 1/8 at the default 120 BPM; 39 is a 1/8. |
| 9 | No noise source, no FM, no wavetable, one 12 dB low-pass (no HPF), MIDI velocity only scales level. | These define the (B) list. |

Lookup values used (the tables are in `tools/gen_tables.py`):

| knob | 0 | 24 | 40 | 56 | 64 | 72 | 80 | 88 | 96 | 104 | 112 | 120 | 127 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ATK/DEC/REL time to 98 % | 1 ms | 5 ms | 17 ms | 53 ms | 93 ms | 163 ms | 288 ms | 506 ms | 0.9 s | 1.6 s | 2.8 s | 4.9 s | 8 s |
| LFO rate (Hz) | 0.05 | 0.17 | 0.37 | 0.84 | 1.26 | 1.9 | 2.8 | 4.2 | 6.3 | 9.4 | 14 | 21 | 30 |
| DLY TIME (ms) | 34 | 167 | 256 | 345 | 390 | 434 | 478 | 523 | 567 | 612 | 656 | 701 | 740 |
| CUTOFF Hz at C4 (no env) | 20 | 72 | 169 | 397 | 608 | 932 | 1.4k | 2.2k | 3.4k | 5.1k | 7.9k | 12k | 17.5k |

Delay sync (ms to knob): 1/8 at 100 / 120 / 140 BPM = 48 / 39 / 32; dotted 1/8 = 75 / 61 / 52; 1/4 = 102 / 84 / 71.
LFO: 1 Hz = 59, 2 Hz = 73, 4 Hz = 87, 5 Hz = 91, 6 Hz = 95, 8 Hz = 101.

---

## 2. How candidates were gathered

Web search plus reading the pages named in section 7. **Most patch articles give no numbers for the original recordings**; the sound-design
write-ups are rebuilds on other synths. Where a recipe below rests on a source, the source is named; where it rests on my own design
from general practice, it says "design, no source". I did not find usable patch breakdowns for Jarre, Kenji Kawai or Akira (the searches returned
retail pages, not analysis), nor for Zimmer's Blade Runner 2049 bass, so those carry no source-based recipe.

---

## 3. Ranked list of all candidates

Rank is by expected impact on a cyberpunk bank (how identifiable the sound is, how well CYBERDECK can reach it, how much it differs from the 8 existing).
"Rendered" means the recipe was run through the real engine (section 8). Nothing was compared to a reference by ear or by spectrum.

| # | Name (<= 12) | Genre reference | Route | What it needs / what is missing | Rendered |
|---|---|---|---|---|---|
| 1 | CS80 BRASS | Vangelis, Blade Runner brass swells (Main Titles / End Titles) | **A** | Swell by inverse filter env, slow attack, light vibrato. Missing: CS-80 dual filter, ribbon slides, poly aftertouch, Lexicon reverb | yes |
| 2 | REESE | Reese bass; dark DnB, darksynth, BR2049 weight | **A** | Saw + detuned pulse + sub, slow cutoff LFO, mono glide. Missing: a second saw (pulse is a stand-in) | yes, beat rate measured |
| 3 | TEARS EP | Blade Runner "Tears in Rain" Rhodes-type melody, Love Theme e-piano, Drive / Tron keys | **B** | FM tine / bell attack, tremolo. Engine has no FM | n/a |
| 4 | NIGHTCALL | Kavinsky "Nightcall" chorus bass (Drive) | **A** | Saw + sub octave, cutoff about 3.9 kHz, 400 ms contour. Missing: stereo chorus | yes |
| 5 | MONOLITH | BR2049 / Cyberpunk 2077 "huge bass" | **A** | Saw + square sub driven hard. Design, no source | yes |
| 6 | NIGHT DRONE | Cyberpunk 2077 bass drones, Blade Runner opening air | **A** | Detuned saws + sub, very slow swell, breathing cutoff, long echo. Missing: reverb, noise bed | yes |
| 7 | VP STRINGS | Blade Runner chords on a Roland VP-330 | **A** | Saw string pad with light vibrato. Missing: ensemble chorus, the vocal "choir" half of the VP-330 | yes |
| 8 | CHOIR | Vangelis choir pads, Ghost in the Shell "Making of Cyborg" chant | **B (MLdeck TIMBRE)** | Vowel formants. Engine has one low-pass, no formants | n/a |
| 9 | NARROW ARP | Nightcall arpeggio (chorused narrow pulse) | **A** | Narrow pulse pluck for the arp. Missing: chorus | yes |
| 10 | BRAAM | Zimmer-style brass blast, trailer hit | **A** | Detuned stack, sub, heavy drive, slow bloom. Design, no source | yes |
| 11 | NYC PULSE | John Carpenter / Howarth pulse bass, Escape from New York | **A** | Pulse + sub, PWM, tight filter pluck. Design from the gear descriptions, not a patch | yes |
| 12 | JUP SWEEP | Roland Jupiter-8 style sweeping pad | **A** | Inverse-envelope sweep, 12 dB filter (the J-8's 2-pole mode). Missing: HPF, cross-mod, sync | yes |
| 13 | RISER | Tonal riser / build | **A (partial)** | Resonant swell. The true riser is noise (see 15) | yes |
| 14 | BERLIN SEQ | Tangerine Dream / Berlin School Moog sequences | **A** | Pulse + PWM + feedback echo for use with the sequencer. Missing: analog drift | yes |
| 15 | NOISE RISER | Risers, wind, steam, rain hiss | **B (MLdeck)** | White noise source | n/a |
| 16 | GLASS BELL | BR2049 glassy tones, Ghost in the Shell bells, music box | **B (DoppleDecker)** | Inharmonic partials (FM ratios) | n/a |
| 17 | REVERB WASH | Lexicon 224 tail on everything in Blade Runner | **B (feature)** | A reverb in the signal path | n/a |
| 18 | SUB DROP | Pitched sub fall, impact | **B (MLdeck gesture)** | Pitch envelope (the engine has none) | n/a |
| 19 | TEARS BASS | Vangelis low bass under "Tears in Rain" | **A** | Soft low saw pad bass. Source: rebuilds describe three detuned saws, low cutoff, saturation | yes |
| 20 | DEEP SUB | Moog-style sine-ish sub | **A** | Saw + sub through a very low cutoff | yes |
| 21 | PRO BRASS | Prophet-5 brass stab (Carpenter / Howarth era) | **A** | Fast-attack brass. Differs from TYRELL by attack and contour | yes |
| 22 | HOOVER | Rave / darksynth hoover lead | **A (partial)** | Fast PWM, sub, glide. Missing: pitch envelope (emulated by a legato drop) | yes, drop not verified |
| 23 | OUTRUN PAD | Juno-60 pad (synthwave) | **A** | PWM pad. Missing: chorus. Close to NEON RAIN | yes |
| 24 | HERO BASS | Drive "A Real Hero" bass | **A** | Low-cutoff driven pluck. Close to CHROME BASS | yes |
| 25 | (UPSIDE ARP, GRID ARP) | Stranger Things / Tron Legacy arp plucks | **A, not proposed** | WIRED already is this sound | not rendered |
| 26 | AKIRA PERC | Geinoh Yamashirogumi metallic percussion and choir | **B (DoppleDecker)** | Metallic partials. Low priority | n/a |
| 27 | VOCODER | Daft Punk, Ghost in the Shell | **none** | Vocoder DSP | n/a |
| 28 | SAX | Blade Runner Love Theme saxophone | **none** | An acoustic instrument, not a synth sound | n/a |

---

## 4. (A) recipes

Row order: `MIX PW DET SUB | CUT RES ENV DRV | ATK DEC SUS REL | LRT LDP LDS LSH | CRU DMX DTM DFB | GLD MOD TUN VEL`.
"Notes" says how to play it. "Cannot reach" says what the target has that the engine lacks.

### 4.1 CS80 BRASS (rank 1)

`{ 45, 64, 22, 0,  92, 22, 20, 25,  76, 96, 105, 80,  90, 7, 64, 0,  0, 40, 75, 55,  0, 0, 64, 55}`

- **Target.** Slow-attack brass swells, filter opening with the note. Sources: the Yamaha CS-80 with velocity and polyphonic aftertouch, the ribbon for
  the falling slides, a Lexicon 224 on the reverb (Reverb Machine); a synth-school recipe uses two pulse oscillators (one an octave up, 6 cents flat),
  250 ms attack, a filter envelope with a very slow attack, and a triangle LFO on pulse width (Syntorial).
- **How the row works.** ENV AMT 20 starts the cutoff about 3.4 octaves down and lets it rise over DEC 96 (0.9 s): measured centroid 1.4 kHz at the start, 2.4 to 3 kHz
  from 0.25 s on. ATK 76 is about 220 ms. LFO is a 4.6 Hz pitch vibrato at depth 7: measured +-13 cents. Delay 75 / FB 55 is the stand-in for the reverb.
- **Notes.** Poly, play chords around C3 to C4 (MIDI 48 to 67 in the test), long holds.
- **Variant.** For the Syntorial-style PWM movement instead of vibrato: LRT 78, LDP 80, LDS 100 (rendered, centroid 1.2 to 2.9 kHz).
- **Cannot reach.** The octave-up second oscillator, the dual filter, ribbon slides, aftertouch swells, a big reverb.

### 4.2 REESE (rank 2)

`{ 55, 64, 24, 70,  62, 30, 70, 55,  0, 70, 110, 40,  18, 25, 10, 0,  0, 0, 60, 20,  25, 100, 64, 40}`

- **Target.** Two detuned saws through a low-pass, mono, with a sine/sub underneath. Sources: Native Instruments (saws at about +-0.3 semitone, cutoff about 650 Hz, resonance low,
  mono), Attack Magazine (saws about +-0.07 semitone plus a sine sub, cutoff about 600 Hz), Soundfly (Netsky "Escape"). All three say keep the sub centred and the top low-passed.
- **How the row works.** Saw + pulse at DET 24 (about 16 cents) beat against each other: measured beat 0.71 Hz on C2 against 0.62 Hz predicted, amplitude modulation about 0.46 of the mean.
  LFO on cutoff at 0.12 Hz adds the slow sweep. DRIVE 55 gives the growl, GLIDE 25 (about 20 ms) smooths between notes; raise it to 50 (about 70 ms) for audible slides.
- **Notes.** Mono, octave 2 (MIDI 36 to 48), slow lines with the sequencer or the arp off; legato keys glide.
- **Cannot reach.** A true second saw (the pulse has a different harmonic series), chorus, a stereo spread.

### 4.3 NIGHTCALL (rank 4)

`{ 10, 64, 6, 90,  99, 10, 84, 40,  0, 85, 105, 40,  0, 0, 10, 0,  0, 22, 61, 35,  0, 100, 64, 50}`

- **Target.** The chorus bass of "Nightcall" (Drive): one layer, two saws an octave apart, mixer driven, cutoff about 3.9 kHz, contour about 400 ms, no sustain (Reverb Machine, Drive synth sounds / How Kavinsky created Nightcall).
- **How the row works.** Saw plus the sub square (octave below) stand in for the two saws; CUTOFF 99 is about 3.9 kHz at middle C (lower on bass notes by key tracking), DEC 85 is 0.4 s, DRIVE 40 for the driven mixer.
  Measured centroid 2.1 to 2.8 kHz on G2 to D3 eighth notes.
- **Notes.** Mono, MIDI 43 to 50, play eighth notes or run the sequencer. The delay is a dotted 1/8 at 120 BPM.
- **Cannot reach.** Stereo chorus, an exact octave-up saw (the sub is a square).

### 4.4 MONOLITH (rank 5) and 4.5 BRAAM (rank 10)

`MONOLITH { 30, 64, 6, 127,  48, 25, 80, 110,  8, 90, 118, 70,  0, 0, 10, 0,  8, 25, 100, 45,  0, 100, 64, 50}`
`BRAAM    { 30, 64, 70, 100,  70, 20, 50, 110,  30, 90, 110, 80,  0, 0, 10, 0,  0, 40, 100, 50,  0, 100, 64, 55}`

- **Target.** The enormous distorted low end and trailer brass blasts of recent sci-fi scoring. **No source**: I found no write-up of how the Blade Runner 2049 bass was made
  (FACT and Synth Anatomy only list gear: CS-80, Zebra, Diva, Prophet-12, tape delays, reverb). These are my designs: sub-heavy saw/square through DRIVE 110 (8x) and a low-pass.
- **Notes.** Mono, so one note at a time. MONOLITH on MIDI 26 to 31, one long note. BRAAM is a single mono note with a fast 8 ms attack and the filter blooming open over about 0.5 s (ENV 50, DEC 90).
  (The test pressed D1 and D2 together; in MONO only the later key sounds, so the numbers are for D2, MIDI 38. For a two-octave double set MODE to POLY.)
  Delay time 100 with FB 45 (or 50) makes the tail.
- **Cannot reach.** Orchestral layers, noise, true reverb.

### 4.6 NIGHT DRONE (rank 6)

`{ 40, 64, 80, 90,  60, 40, 40, 15,  115, 120, 127, 120,  6, 60, 10, 0,  0, 55, 110, 75,  0, 0, 64, 40}`

- **Target.** Cyberpunk 2077's score is described as mostly ambient, with bass drones and placid synth textures (Den of Geek); the Blade Runner main-title air is the same family. **Design, no recipe source.**
- **How the row works.** ATK 115 (about 3.4 s) and REL 120 (4.9 s) blur the notes together; DET 80 plus SUB 90 are the beating wall; a 0.07 Hz triangle LFO moves the cutoff; the delay (110 = 656 ms) at FB 75 is the echo bed.
- **Notes.** Poly, octave 1 or 2 (MIDI 36 and 43 in the test); hold fifths; push CUTOFF up for a lighter version. Peak reached -1.4 dBFS: lower MASTER or drop DLY MIX when stacking chords.
- **Cannot reach.** A noise bed and a real reverb.

### 4.7 VP STRINGS (rank 7)

`{ 10, 64, 20, 0,  74, 8, 64, 8,  78, 100, 120, 85,  85, 7, 64, 0,  0, 35, 70, 40,  0, 0, 64, 40}`

- **Target.** The Blade Runner chord bed on a Roland VP-330. The same Reverb Machine article says the string sound was a VP-330 mk II sample with tone and volume automated and a Lexicon 224 reverb.
- **How the row works.** Nearly pure saw (MIX 10), a little detune, 250 ms attack, long release, 3.6 Hz vibrato at about +-12 cents, and an ENV-neutral filter so it stays soft.
- **Notes.** Poly, chords in the F3 to A4 range. **Cannot reach.** The VP-330's ensemble chorus and its vocal ("choir") voice.

### 4.8 NARROW ARP (rank 9)

`{127, 118, 30, 30,  74, 30, 90, 20,  0, 50, 0, 40,  0, 0, 10, 0,  0, 50, 61, 55,  0, 0, 64, 55}`

- **Target.** The "Nightcall" arpeggio: a chorused narrow pulse with a short envelope and a touch of delay (Reverb Machine: square wave, pulse width almost at maximum, chorus II, about 20 % reverb).
  Tron Legacy-type arps are the same recipe with a lower cutoff (Ask.Audio rebuild: short decay, no sustain, low-pass).
- **How the row works.** MIX 127 and PW 118 give an 82 / 18 pulse; DET 30 thickens; DEC 50 and SUS 0 make the pluck; DLY TIME 61 is a dotted 1/8 at 120 BPM.
- **Notes.** Poly with **ARP on**, 1/16 or 1/8, 2 octaves, gate about 60 %. Differs from WIRED (saw-heavy). **Cannot reach.** Chorus.

### 4.9 NYC PULSE (rank 11) and 4.10 BERLIN SEQ (rank 14)

`NYC PULSE  { 90, 50, 6, 60,  62, 12, 86, 30,  0, 50, 95, 30,  40, 20, 100, 0,  0, 20, 60, 35,  0, 100, 64, 50}`
`BERLIN SEQ { 70, 56, 10, 50,  56, 45, 85, 25,  0, 48, 70, 40,  20, 30, 100, 0,  0, 55, 80, 80,  0, 100, 64, 50}`

- **Target.** Carpenter / Howarth's scores used a Prophet-5, ARP Quadra and (for the original Halloween) a Moog modular, with Howarth programming many of the Prophet sounds (Fact, Substack, Sequential forum). Tangerine Dream's Phaedra came from a
  Moog and its sequencer, with audible tuning drift and reverb sent into delays (uDiscover). None of these pages gives patch values. **The rows are designs** for the sound those descriptions point to: dark pulse, slow PWM, tight contour.
- **How the rows work.** LDS 100 puts the LFO on pulse width. NYC PULSE is dry and tight (FB 35, DMX 20), measured centroid 230 to 340 Hz on A1 to A2 eighths. BERLIN SEQ adds a longer echo (DLY TIME 80, FB 80) for the stacked-delay wash.
- **Notes.** Mono, MIDI 33 to 50, with the sequencer or an ostinato; BPM 110 to 130. **Cannot reach.** Analog drift (a TUNE offset is static), a real reverb.

### 4.11 JUP SWEEP (rank 12)

`{ 30, 64, 40, 10,  90, 35, 28, 10,  70, 105, 105, 90,  40, 20, 10, 0,  0, 45, 80, 55,  0, 0, 64, 50}`

- **Target.** Roland describes the Jupiter-8's 2-pole (12 dB) filter as the soft setting for pads, brass from a slightly detuned second oscillator plus a filter envelope, and a negative envelope for a bright sweeping sound (Roland Cloud sound-design article).
  CYBERDECK's filter is a 12 dB state-variable low-pass, so the character is close.
- **How the row works.** ENV 28 starts the cutoff about 2.8 octaves down; DEC 105 is about 1.7 s: measured centroid 0.7 kHz rising to 2.4 kHz over the first second.
- **Notes.** Poly, MIDI 48 to 64 chords. **Cannot reach.** The J-8 HPF, cross-modulation, sync.

### 4.12 RISER (rank 13)

`{ 45, 64, 50, 40,  100, 60, 10, 40,  120, 125, 110, 90,  0, 0, 10, 0,  0, 55, 80, 80,  0, 0, 64, 50}`

- **Design, no source.** One held fifth: a 4.9 s attack, a cutoff that climbs from about 130 Hz to 2 kHz, resonance 60, delay wash. Centroid 0.6 kHz to 1.7 kHz over the hold.
- **Notes.** Poly, hold MIDI 45 + 52 for 5 s or more. Peak reached -0.9 dBFS (resonance plus delay): trim RES or DLY MIX if you layer it.
- **Cannot reach.** A noise riser (no noise source), a pitch rise.

### 4.13 TEARS BASS, DEEP SUB, PRO BRASS, HOOVER (ranks 19 to 22)

```
TEARS BASS { 35, 64, 14, 50,  58, 15, 70, 50,  60, 100, 110, 85,  0, 0, 10, 0,  0, 30, 80, 45,  0, 0, 64, 50}
DEEP SUB   {  0, 64, 0, 127,  40, 20, 70, 30,  0, 60, 118, 30,  0, 0, 10, 0,  0, 0, 60, 0,  15, 100, 64, 50}
PRO BRASS  { 35, 64, 30, 20,  62, 25, 100, 40,  12, 70, 85, 50,  0, 0, 10, 0,  0, 25, 61, 35,  0, 0, 64, 50}
HOOVER     { 60, 64, 40, 60,  95, 35, 64, 45,  0, 90, 118, 40,  100, 70, 100, 0,  0, 0, 60, 30,  58, 100, 64, 50}
```

- **TEARS BASS.** A remake write-up of the Blade Runner low bass (per the Reverb Machine "2022" article, read only from a search snippet) lists three slightly detuned saws, a low cutoff and saturation. Poly, MIDI 34 to 43, 70 ms attack, soft contour. Cannot reach: the third saw, the room.
- **DEEP SUB.** Saw + sub through a cutoff of 40 so the fundamental dominates: centroid 105 Hz on G1 / C2, over 18 dB down above 150 Hz. Mono, MIDI 24 to 40. Distinct from CHROME BASS (no resonance bite).
- **PRO BRASS.** Fast-attack (ATK 12 = 2 ms) stab with a strong contour; poly, chords; delay at the dotted 1/8. Differs from TYRELL by the attack.
- **HOOVER.** Sources: three pulse-saw PWM oscillators, two +-40 cents and one lower, a pitch envelope of about 8 semitones that falls then settles (MusicRadar); a fast, wide PWM pulse with a sub an octave below through an open filter (forum posts, Elektronauts thread). The row has the fast PWM (LFO 7.7 Hz on pulse width, depth 70) and the sub.
  The pitch dip is only imitated: hold a note an octave or so higher, press the target key (legato, GLIDE 58 = about 130 ms) and release the first. The glide itself was **not** verified (my pitch tracker failed on this timbre). Mono.

### 4.14 Not proposed

- **OUTRUN PAD** `{45,55,30,45, 78,12,70,12, 80,100,110,90, 22,40,100,0, 0,40,62,50, 0,0,64,40}`: Juno-60 pad. Sources (bonedo walkthrough): sub about 40 %, pulse width about 40 %, slow VCA swell, LFO to the filter,
  chorus I (without it clipping). The row is a PWM pad; without the chorus its nearest existing preset is NEON RAIN (a slow wide pad), so it would sound like a sibling. Keep it as a user preset at best.
- **HERO BASS** `{60,64,4,40, 48,18,96,80, 0,75,0,40, 0,0,10,0, 0,0,60,0, 0,100,64,50}`: "A Real Hero" bass: two saws and a square, cutoff about 120 Hz, contour 55 %, decay 200 ms, driven (Reverb Machine). A low-cutoff driven pluck; its nearest existing preset is CHROME BASS and it would not be heard as new.
- **UPSIDE ARP / GRID ARP** (Stranger Things pluck, Tron arp): sources agree on short decay, no sustain, low-pass, a pluck on a saw/pulse mix; reachable, but WIRED already is that sound.

---

## 5. (B) candidates: what is missing and the route

None of these can go into CYBERDECK's FACTORY table today. The "route" is where the sound would be built.
Read this table together with section 10: the Firmware Hub shows that the FM-1 hardware can do all of these, so each "lacks" is a CYBERDECK feature gap, and porting an existing engine is a real alternative to the routes listed here.

| Candidate | What the engine lacks | Better route | Why |
|---|---|---|---|
| **TEARS EP** (Rhodes / FM electric piano) | FM, a tine attack that darkens, tremolo | **DoppleDecker** patch fit | `RECORDING_GUIDE.md` lists electric piano first among "FM is built for these"; the repo benchmark fits a 4-op e-piano at score 0.105 against 0.469 for a sine. The fitted patch plays only in the DoppleDecker firmware, and that firmware cannot load a bank yet (use `bank.h` and rebuild). |
| **GLASS BELL** (BR2049 glass, Ghost in the Shell bells, music box, Akira metal) | Inharmonic partials | **DoppleDecker** | Bells, chimes, marimba and kalimba are in its good list; a kalimba take already exists in `samples/`. Same delivery limit. |
| **CHOIR** (vocal pad, GitS chant) | Formants | **MLdeck TIMBRE** | `train_timbre.py` already trains on vowel formants (A, E, I, O, U with randomised bandwidths) and MLdeck ships AURORA PAD and ORBIT CHOIR on it. It has no WAV-to-latent step that I could find, so fitting a particular recording needs a small script (harmonic amplitudes at f0, through the VAE encoder). A 32-harmonic static spectrum will not reproduce a moving choir. |
| **NOISE RISER / WIND / STEAM** | Noise source | **MLdeck** | It has a noise level per preset (`P_NOISE`) and the GESTURE / PATH pages move pitch, cutoff and map point over time. Build by hand, not by fitting. DoppleDecker's README says it handles noise poorly. |
| **SUB DROP** (pitched fall, impact) | Pitch envelope | **MLdeck gesture** | A gesture path of pitch (down 24 to 36 semitones) plus cutoff; no fit needed. |
| **REVERB WASH** | Reverb | **new CYBERDECK feature** | No sample match helps. A small reverb (comb + all-pass, integer) is the feature the pad candidates (CS80 BRASS, VP STRINGS, NIGHT DRONE, JUP SWEEP) would gain from most. It needs pool RAM (the two delay lines already use 128 KB of the 336 KB pool) and ISR time (target under 2.9 ms per block). |
| **VOCODER, SAX** | A vocoder; an acoustic instrument | none | Not worth chasing. |

Whichever route: the fit is only as good as the one clean note you give it (DoppleDecker: one note, no reverb or chorus on it; its README says pads, chords and
effects fit poorly).

---

## 6. Suggested set to add

### 6.1 In order of impact: 12 core and 4 optional

Core (1 to 12): CS80 BRASS, REESE, NIGHTCALL, MONOLITH, NIGHT DRONE, VP STRINGS, NARROW ARP, BRAAM, NYC PULSE, JUP SWEEP, RISER, BERLIN SEQ.
Optional (13 to 16): TEARS BASS, DEEP SUB, PRO BRASS, HOOVER.
That is 5 pads/swells (CS80 BRASS, VP STRINGS, JUP SWEEP, NIGHT DRONE, RISER), 6 basses (REESE, NIGHTCALL, MONOLITH, NYC PULSE, BERLIN SEQ, BRAAM), 1 arp, plus the optional ones. Rows are in FACTORY order and ready to paste
(`MIX PW DET SUB | CUT RES ENV DRV | ATK DEC SUS REL | LRT LDP LDS LSH | CRU DMX DTM DFB | GLD MOD TUN VEL`):

```c
    /* ---- core */
    /* CS80 BRASS  */ { 45,  64,  22,   0,   92,  22,  20,  25,   76,  96, 105,  80,   90,   7,  64,   0,    0,  40,  75,  55,    0,   0,  64,  55},
    /* REESE       */ { 55,  64,  24,  70,   62,  30,  70,  55,    0,  70, 110,  40,   18,  25,  10,   0,    0,   0,  60,  20,   25, 100,  64,  40},
    /* NIGHTCALL   */ { 10,  64,   6,  90,   99,  10,  84,  40,    0,  85, 105,  40,    0,   0,  10,   0,    0,  22,  61,  35,    0, 100,  64,  50},
    /* MONOLITH    */ { 30,  64,   6, 127,   48,  25,  80, 110,    8,  90, 118,  70,    0,   0,  10,   0,    8,  25, 100,  45,    0, 100,  64,  50},
    /* NIGHT DRONE */ { 40,  64,  80,  90,   60,  40,  40,  15,  115, 120, 127, 120,    6,  60,  10,   0,    0,  55, 110,  75,    0,   0,  64,  40},
    /* VP STRINGS  */ { 10,  64,  20,   0,   74,   8,  64,   8,   78, 100, 120,  85,   85,   7,  64,   0,    0,  35,  70,  40,    0,   0,  64,  40},
    /* NARROW ARP  */ {127, 118,  30,  30,   74,  30,  90,  20,    0,  50,   0,  40,    0,   0,  10,   0,    0,  50,  61,  55,    0,   0,  64,  55},
    /* BRAAM       */ { 30,  64,  70, 100,   70,  20,  50, 110,   30,  90, 110,  80,    0,   0,  10,   0,    0,  40, 100,  50,    0, 100,  64,  55},
    /* NYC PULSE   */ { 90,  50,   6,  60,   62,  12,  86,  30,    0,  50,  95,  30,   40,  20, 100,   0,    0,  20,  60,  35,    0, 100,  64,  50},
    /* JUP SWEEP   */ { 30,  64,  40,  10,   90,  35,  28,  10,   70, 105, 105,  90,   40,  20,  10,   0,    0,  45,  80,  55,    0,   0,  64,  50},
    /* RISER       */ { 45,  64,  50,  40,  100,  60,  10,  40,  120, 125, 110,  90,    0,   0,  10,   0,    0,  55,  80,  80,    0,   0,  64,  50},
    /* BERLIN SEQ  */ { 70,  56,  10,  50,   56,  45,  85,  25,    0,  48,  70,  40,   20,  30, 100,   0,    0,  55,  80,  80,    0, 100,  64,  50},
    /* ---- optional */
    /* TEARS BASS  */ { 35,  64,  14,  50,   58,  15,  70,  50,   60, 100, 110,  85,    0,   0,  10,   0,    0,  30,  80,  45,    0,   0,  64,  50},
    /* DEEP SUB    */ {  0,  64,   0, 127,   40,  20,  70,  30,    0,  60, 118,  30,    0,   0,  10,   0,    0,   0,  60,   0,   15, 100,  64,  50},
    /* PRO BRASS   */ { 35,  64,  30,  20,   62,  25, 100,  40,   12,  70,  85,  50,    0,   0,  10,   0,    0,  25,  61,  35,    0,   0,  64,  50},
    /* HOOVER      */ { 60,  64,  40,  60,   95,  35,  64,  45,    0,  90, 118,  40,  100,  70, 100,   0,    0,   0,  60,  30,   58, 100,  64,  50},
```

Names for `PNAME`: `"CS80 BRASS", "REESE", "NIGHTCALL", "MONOLITH", "NIGHT DRONE", "VP STRINGS", "NARROW ARP", "BRAAM", "NYC PULSE", "JUP SWEEP", "RISER", "BERLIN SEQ", "TEARS BASS", "DEEP SUB", "PRO BRASS", "HOOVER"` (all 12 characters or fewer; the title line truncates at 20 characters).

Placing them in the preset order: pads and swells together, mono basses together (PRESETS steps through them with the knob).
Mutate (HOME) works on all of them; REESE, MONOLITH, BRAAM, NYC PULSE, BERLIN SEQ, NIGHTCALL, DEEP SUB, HOOVER are mono (MODE 100), the others poly.

### 6.2 Storage: the part that needs a decision

`set` is `{magic, cur, oct, pad, slot[NPRESET][24], g[8], pat[32]}` = 236 B, and `settings_save` accepts at most 256 B (`platform.c`). Only 20 B are left, one preset is 24 B.
The recipes cannot simply be appended to `FACTORY` and `NPRESET` raised. Options:

1. **Second bank in ROM, edits in the 8 slots.** Selecting a ROM preset copies it into a working slot; SAVE writes the working copy into slot `cur` only for the first 8. Cheapest, but SAVE on a bank-2 preset needs a rule (overwrite which slot?).
2. **A second flash sector** (as MLdeck does with 0xFB000; GROVEDECK uses the same address, so it is already a known-safe pattern for these projects) holding all presets. Needs the flash range allowed in `FL_RANGE_OK` and a magic + CRC. The cleanest, with the most code.
3. **Shrink what is saved:** the sequencer pattern (32 B) could move out, which frees 52 B, room for two presets. Not enough on its own.
4. **Replace weaker factory presets** instead of adding (for example, swap one of the eight). Costs nothing, but the user asked for more, not different.

My recommendation is option 2 if you expect to grow past 16, option 1 if you want 8 to 16 more with the least code.

---

## 7. Sources

Sound design and gear (read in full unless marked "snippet"):

- Vangelis' Blade Runner synth sounds (Rhodes, VP-330, CS-80 brass, Lexicon 224): <https://reverbmachine.com/blog/vangelis-blade-runner-synth-sounds>
- Blade Runner remake, 2022 article (low bass description, snippet only): <https://reverbmachine.com/blog/vangelis-blade-runner-synth-sounds-2022/>
- Syntorial, Vangelis Blade Runner brass preset recipe: <https://www.syntorial.com/preset-recipe/vangelis-blade-runner-brass/>
- Synthtopia, the synth sounds of Blade Runner (gear mentions): <https://www.synthtopia.com/content/2017/10/06/the-synth-sounds-of-blade-runner/>
- Synth Anatomy, sound design of Blade Runner 2049 (snippet): <https://synthanatomy.com/2017/10/have-a-look-at-the-sound-design-of-blade-runner-2049-including-the-yamaha-cs-80-synthesizer.html>
- FACT, Zimmer and Wallfisch on Blade Runner 2049 (snippet): <https://factmag.com/2017/10/20/hans-zimmer-wallfisch-blade-runner-2049-interview>
- Reverb Machine, Drive synth sounds (Nightcall chorus bass, arp, Under Your Spell, A Real Hero): <https://reverbmachine.com/blog/drive-synth-sounds-2019> and <https://reverbmachine.com/blog/how-kavinsky-created-nightcall/>
- Reese bass: <https://blog.native-instruments.com/reese-bass/>, <https://www.attackmagazine.com/technique/synth-secrets/fred-again-inspired-reese-basslines-with-twin-3/>, <https://flypaper.soundfly.com/produce/rebuild-the-reese-bass-synth-patch-netsky-escape/>
- Carpenter and Howarth (gear): <https://microchop.substack.com/p/samplers-synths-and-the-halloween>, <https://factmag.com/2015/10/28/alan-howarth-interview-escape-from-new-york>, <https://forum.sequential.com/index.php/topic,1872.msg40286.html> (a forum post, its claims about the original Halloween are second-hand)
- Tangerine Dream, Phaedra: <https://www.udiscovermusic.com/stories/tangerine-dream-phaedra-making-of-feature/>
- Stranger Things (gear accounts disagree, forum claims are unverified): <https://splice.com/blog/recreating-stranger-things-theme>, <https://reverb.com/news/the-synth-sounds-of-stranger-things>, <https://www.syntorial.com/preset-recipe/survive-stranger-things-theme-pluck/>
- Juno-60 pad: <https://www.bonedo.de/artikel/sound-workshop-pad-lorde-team> (German), <https://en.wikipedia.org/wiki/Roland_Juno-60>
- Tron Legacy arps (Reason rebuild, snippet-level): <https://ask.audio/articles/reason-6-create-daft-punk-arpeggiations-in-the-style-of-tron-legacy>; sound team: <https://www.mixonline.com/sfp/oscar-sound-watch-daft-punk-score-and-sfx-blend-together-tron-legacy-383963>
- Cyberpunk 2077 score (ambient, drones; the headline mentions a Soviet-era synth, details not retrieved): <https://www.denofgeek.com/games/cyberpunk-2077-soundtrack-music-listen-free/>
- Hoover: <https://musicradar.com/how-to/how-to-create-a-classic-90s-hoover-sound>, <https://elektronauts.com/t/syntakt-patch-requests-alpha-juno-2-hoover/198696>
- Jupiter-8: <https://articles.roland.com/sound-design-jupiter-8-model-expansion-poly-presets/>
- Ghost in the Shell, "Making of Cyborg" (retail copy only, no technical analysis found): <https://forcedexposure.com/Artists/KAWAI.KENJI.html>

Project docs read: `docs/MANUAL.md` (4, 5), `firmware/src/cyberdeck.c`, `tools/gen_tables.py`; `DoppleDecker/README.md`, `docs/BANK.md`, `docs/RECORDING_GUIDE.md`, `tools/dd_fit.py`; `MLdeck/docs/SPEC.md`, `tools/train_timbre.py`, `firmware/src/mldeck.c`.

---

## 8. Reference audio for the (B) sounds, and legality

**Nothing was downloaded and no audio from any soundtrack, track or commercial pack goes into the repo.** The fits would run on audio kept on your PC outside git.
For films and tracks, listen yourself, do not rip: Blade Runner (1982) Main Titles and End Titles (CS-80 brass), "Tears in Rain" (Rhodes-type melody over a low drone) and the Love Theme (e-piano; the sax is acoustic); Blade Runner 2049 "Sea Wall" and "Wallace" as places to start for the big low end and glassy drones (track names from memory, I did not check which sounds are where; no timestamps);
Drive: "Nightcall" (chorus bass at the first chorus, arp from the start), "Under Your Spell", "A Real Hero"; Ghost in the Shell (1995) main title "Making of Cyborg"; Tron: Legacy "Overture"; Stranger Things main title; Escape from New York main theme; Halloween main theme; Tangerine Dream "Phaedra".

Free, legal references by candidate. Licences are as shown on each Freesound page when fetched (marked "checked") or as a search result said (marked "not re-checked": open the page and confirm the CC0 badge before using it).

| For | Source | Licence | Note |
|---|---|---|---|
| CHOIR | Diboz, choir_aah: <https://freesound.org/people/Diboz/sounds/211494/> | CC0, checked | 7.4 s, resynthesised, meant to be A#3 |
| CHOIR | DiArchangeli, Choir.aif: <https://freesound.org/people/DiArchangeli/sounds/458144/> | CC0, not re-checked | about 13 s of "aah" |
| CHOIR | uair01, chamber choir single notes: <https://freesound.org/people/uair01/packs/4311/> | unknown, **check** | single-note vowels, the best shape if the licence is free |
| GLASS BELL | sgossner, Tubular Bells C5 (VSCO 2 CE): <https://freesound.org/people/sgossner/sounds/374274/> | CC0, checked | one hit, 42 s, MIDI 72 |
| GLASS BELL | Fupicat, Correct Bell: <https://freesound.org/people/Fupicat/sounds/538147/> | CC0, not re-checked | a short digital bell |
| TEARS EP | TheSoundLibrary, Electric Piano (FM): <https://freesound.org/people/TheSoundLibrary/sounds/817013/> | CC0, not re-checked | an FM patch made in FamiStudio, so it is a *reachable* target |
| TEARS EP | RealRhodesSounds, single notes D1 to D6: <https://freesound.org/people/RealRhodesSounds/> | **not shown on the page, check each file** | a real Rhodes, 2005 |
| NOISE RISER | Ihaksi, Noise Riser: <https://freesound.org/people/Ihaksi/sounds/339731/>; deadrobotmusic, white noise build: <https://freesound.org/people/deadrobotmusic/sounds/669969/>; spongemas765, White Noise Sweep: <https://freesound.org/people/spongemas765/sounds/235401/> | CC0, not re-checked | MLdeck gestures are set by hand, these are only to listen to |
| SUB DROP | AlexLane, Synth Bass Drop Impact: <https://freesound.org/people/AlexLane/sounds/475005/>; LordHannes, Bass Boom: <https://freesound.org/s/268084/> | CC0, not re-checked | |
| NIGHT DRONE, REVERB WASH | MieliTietty, dark drone (Nordstage, microKORG, Juno-106): <https://freesound.org/s/728465/>; LookIMadeAThing, Sci-fi Ambient Drone: <https://freesound.org/people/LookIMadeAThing/sounds/534018/> | CC0, both checked | a reference for the mood and the reverb tail, not a one-note fit target |

Avoid: ERH "choir a" and kklab5050 "Glass Bell" (CC-BY, credit needed), sgossner / qubodup "Sci-fi Laboratory Ambience" (CC-BY 3.0). Freesound CC-BY sounds are allowed if you credit them, but the credit would have to ride along with any published bank.

Free synths as a reference source (render one clean note to WAV for private fitting, keep the WAV out of the repo):

- **Vital.** The EULA (<https://vital.audio/eula/>) lets you use it in commercial work and to make sample packs, but forbids redistributing unaltered factory presets, samples, wavetables and LFOs, and does not clearly cover modified ones. A rendered note used only as a local fit target and never shipped is the safe case; I am not a lawyer. A forum answer saying factory content is free to use commercially is a user's opinion, not Vital's.
- **Surge XT.** The code is GPL-3 (<https://github.com/surge-synthesizer/surge>). I could not confirm the licence of the factory patches (no README in the factory patch folder, and each patch has its own licence field), so check the licence field in the patch dialog before you rely on one. Its factory folders include Keys, Pads, Winds, Plucks, FX, Basses, Brass: auditioning there for a Rhodes-like or bell-like starting point is fine; I did not verify any specific patch name.
- **Your own recordings** are always the cleanest: a real Rhodes or e-piano note, a bowed or struck wine glass, a sung "ah" on one pitch, your own noise sweep from any synth you own. `DoppleDecker/docs/RECORDING_GUIDE.md` says how to record (one clean note, 2 to 3 s, no reverb or chorus, peaks between -12 and -3 dB), and `.\dd.ps1 record` is the recorder. The `samples/` folder there already has Kalimba, Moogy_Bass, Lead_Analog and others; I did not check where they came from.

---

## 9. What was checked

Method. `git archive HEAD` of this repo into a scratch directory (so the working tree was never touched), `FACTORY` replaced by one candidate row, `cc -O2 -DDINGHY_HOST` build (the same flags as `tests/run_host.sh`), a script of `T midi 0x90 / 0x80` note lines, then analysis of the WAV in Python (peak, RMS, onset and release times, spectral centroid over time, band energies, DC, left/right balance, clipping).
The scratch scripts are not part of the repo.

| Preset | Peak dBFS | Centroid Hz at the loudest part | Evolution | Band (0 to 150 / 150 to 600 / 0.6 to 2.5k / 2.5 to 8k / above 8k), dB re total |
|---|---|---|---|---|
| CS80 BRASS | -2.9 | 1308 | 1.4k to 3.0k over 1 s | -22 / 0 / -12 / -27 / -60 |
| REESE | -3.8 | 859 | beat 0.71 Hz (pred. 0.62) | -1 / -10 / -14 / -33 / -62 |
| NIGHTCALL | -2.9 | 2575 | 2.1k to 2.8k per note | -1 / -9 / -14 / -22 / -30 |
| MONOLITH | -2.6 | 295 | steady | 0 / -16 / -26 / -55 / -75 |
| NIGHT DRONE | -1.4 | 475 | slow, 3.7 s swell | -1 / -9 / -19 / -50 / -78 |
| VP STRINGS | -2.8 | 1172 | steady, 250 ms attack | -31 / 0 / -10 / -27 / -67 |
| NARROW ARP | -2.3 | 2180 | per note | -3 / -6 / -7 / -16 / -30 |
| BRAAM | -2.8 | 425 | 0.7k to 0.9k bloom | -1 / -8 / -21 / -41 / -70 |
| NYC PULSE | -3.7 | 286 | 230 to 340 per note | 0 / -11 / -25 / -57 / -73 |
| JUP SWEEP | -2.8 | 890 | 0.7k to 2.4k over 1 s | -26 / -1 / -9 / -33 / -66 |
| RISER | -0.9 | 1679 | 0.6k to 1.7k over 4 s | -14 / -1 / -11 / -23 / -46 |
| BERLIN SEQ | -2.5 | 508 | per note | -1 / -7 / -20 / -41 / -63 |
| TEARS BASS | -2.1 | 396 | steady | 0 / -11 / -21 / -53 / -78 |
| DEEP SUB | -3.9 | 107 | steady | 0 / -18 / -48 / -78 / -91 |
| PRO BRASS | -4.0 | 1060 | stabs | -22 / 0 / -11 / -32 / -60 |
| HOOVER | -3.9 | 2426 | PWM motion | -12 / -1 / -8 / -17 / -30 |
| OUTRUN PAD | -2.6 | 1500 | steady | -15 / -1 / -9 / -22 / -50 |
| HERO BASS | -4.8 | 466 | per note | -1 / -9 / -19 / -41 / -67 |

**Verified on the host (all 18):** the row is accepted and renders; no silence, no NaN, no clipped sample, peaks between -0.9 and -4.8 dBFS (the limiter is working; RISER, NIGHT DRONE and TEARS BASS come closest to the ceiling), left and right within 1 dB, the level at the end of each render between -32 and -180 dBFS (BERLIN SEQ, the longest echo, was still ringing at -32; no runaway feedback), and the spectral shape moves the way each recipe says
(brass and JUP SWEEP swell, RISER climbs, NIGHTCALL stays bright, subs stay low). Two numeric checks: the REESE beat (0.71 Hz measured, 0.62 Hz predicted from DETUNE) and the CS80 vibrato (+-13 cents at depth 7).

Overlap with the existing eight, by the sum of absolute knob differences to the nearest one: PRO BRASS to WIRED 262, CS80 BRASS to TYRELL 261, JUP SWEEP to NEON RAIN 263, OUTRUN PAD to NEON RAIN 265, NARROW ARP to WIRED 284, HERO BASS to CHROME BASS 315; all the others are 294 or more away.
These are knob distances, not a sound comparison.

**Not verified:** anything by ear; whether any preset resembles its target (no reference audio was compared); a real-hardware run (nothing was built for the device or flashed, and the on-device CPU load of a 4-voice chord with the delay was not measured); the HOOVER legato pitch drop;
the dependence on key tracking across the whole keyboard (each preset was run only in its range); mutate (HOME) on the new rows; save and reload (blocked by 6.2); other octaves of the same preset. Fixes needed before shipping: choose the storage option in 6.2, add the names, update the manual's section 5 table.

---

## 10. What the Firmware Hub changes (added after review)

Source: <https://thefirmwarehub.com/> (read 2026-10-16). It lists about 50 FM-1 firmwares; the page gives no per-firmware links, so I could not open most of the projects. Descriptions below are the hub's own one-liners, plus what a web search turned up where noted.
The first version of this note treated CYBERDECK's engine as the limit of what the FM-1 can do. The hub shows it is not.

What the hub says exists on this hardware, relevant to the (B) list:

| Need (from section 5) | Already on the hub | Licence / access |
|---|---|---|
| FM electric piano, glass bells, FM bass | **SLOOP** (isod89) has a six-operator FM engine ported from Dexed that imports DX7 SysEx (one voice or a 32-voice bank); v2.5 per its release page adds two engines, 77 sounds, a drum synth, 48 kHz USB audio. Hub entries: **sloopDX** (zvenson, "SLOOP with a DX7 engine inside"), **SLOOP Floyd FM** (four-operator, visual), **SLOOP DX7 Banks** (majnikool, twelve banks), **Melodee** (CZ-1 and Dexed), **Baud Girl FM-1+VA** (FM engines, virtual analog, 64-step sequencer) | GPL-3.0 for the open ones (the hub says so; the SLOOP wiki confirms it for SLOOP). Baud Girl and Groove OS are closed. The licence of the individual DX7 banks was not checked |
| Chorus (Juno pads, Nightcall, VP-330 ensemble) | **FM-106** (PatttF), "Juno-106 emulation" | Repo not found by search, not verified |
| Reverb (the Lexicon 224 tail on everything in Blade Runner) | **GHOULBOX** (Jason Persinger), "dungeon synth with cathedral reverb and hurdy-gurdy" (a Matrixsynth post of 2026-10-10 covers it) | Repo not found by search, not verified |
| Noise, drones, many engines | **Jangada** ("dark drones and ten engines"), **Skiff** ("fourteen engines"), **AMB-1** (generative ambient), **ETCH** (drawn waveforms), **zp12** (12-bit sampler) | not checked |
| Choir / vowels, plucked and struck acoustic models | **ChoralRoot**, **FiMba-1** (kalimba model), **FoMni**, **fm1-chord** | not checked |

What this means for the plan:

1. **(B) is a feature list for CYBERDECK, not a wall.** The cheapest way to get the FM electric piano and bells into CYBERDECK is probably to port SLOOP's FM6 voice (GPL-3.0 into a GPL-3.0 project, credits kept as CLAUDE.md requires) rather than fit patches with DoppleDecker, and a DX7 e-piano or bell patch can be loaded directly instead of being fitted to a recording.
   DoppleDecker's fitting is still the route for a sound that has no DX7 equivalent. That is a reading of the hub, not something I tested: I did not open SLOOP's source, so the size of the port, its CPU cost and whether it fits beside the CYBERDECK voice engine are unknown.
2. **Reverb and chorus are the two features that lift the most (A) presets.** CS80 BRASS, VP STRINGS, NIGHT DRONE, JUP SWEEP and RISER all name a reverb as the missing piece; NIGHTCALL, NARROW ARP and OUTRUN PAD name the chorus. If either exists as open source on the hub (GHOULBOX, FM-106), reading it for the memory and CPU it takes is the next step. CYBERDECK's pool already holds 128 KB of delay lines in 336 KB.
3. **The hub is the right place to look for genre material too.** Dark drones (Jangada), generative ambient (AMB-1), dungeon synth with a cathedral reverb (GHOULBOX) and a Juno emulation (FM-106) are all close to the cyberpunk / Blade Runner brief; hearing them shows what the same box does in other hands.
4. **Section 6 stands as the quick win** (16 subtractive recipes, no engine change), but the storage limit in 6.2 and the missing reverb are now the main constraints, not the sound list.

Suggested next steps, if you want them (none started): (a) read SLOOP's FM6 source and the GHOULBOX / FM-106 sources for size and CPU, and (b) decide whether CYBERDECK gets a reverb, a chorus, or an FM voice first; I would pick the reverb, because it helps more of the 16 presets than the other two.
