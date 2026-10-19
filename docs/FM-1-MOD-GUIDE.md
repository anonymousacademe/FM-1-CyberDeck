# FM-1 Firmware Mod Guide

**For an agent (or a person) starting a new firmware project for the M-VAVE FM-1 pocket synth.** State of the scene as of **2026-10-16**. It is two weeks old, it moves daily, and much of it is documented by other agents' prose, so this guide labels how well each fact is known.

Read sections 1 to 3 before you write any code. Sections 4 to 8 are reference. Section 9 lists what has not been done yet. Section 10 is the kickoff checklist and a brief template.

**Contents**

0. [How to read the evidence labels](#0-how-to-read-the-evidence-labels)
1. [The ten things to know first](#1-the-ten-things-to-know-first)
2. [Safety and recovery](#2-safety-and-recovery-read-before-you-touch-flash)
3. [The device: hardware, memory, float, flash map](#3-the-device)
4. [Choosing a base](#4-choosing-a-base-decision-table)
5. [Toolchain, packages, installing, fast loops, the hub](#5-toolchain-packages-installing-and-fast-loops)
6. [The landscape: what exists](#6-the-landscape)
7. [Techniques people have worked out](#7-techniques-people-have-worked-out)
8. [Things older docs got wrong, and what we did not expect](#8-things-older-docs-and-assumptions-got-wrong-and-what-we-did-not-expect)
9. [What has not been done, and what is under-explored](#9-what-has-not-been-done-and-what-is-under-explored)
10. [Kickoff checklist, milestones, a brief to paste](#10-starting-a-new-project-kickoff-checklist)
- Appendices: [A firmware catalogue](#appendix-a-all-52-hub-firmwares-catalogue-of-2026-10-16-sorted-by-installs) / [B sources](#appendix-b-the-sources-to-read-first-all-public) / [C numbers](#appendix-c-numbers-cheat-sheet) / [D glossary](#appendix-d-glossary) / [E identity check script](#appendix-e-check-an-identity-against-the-live-hub-tested-2026-10-16) / [F our workspace](#appendix-f-our-workspace-the-fm-1-projects-folder) / [G method and limits](#appendix-g-method-limits-and-how-to-refresh)

---

## 0. How to read the evidence labels

| Label | Meaning |
|---|---|
| **[checked]** | We checked it ourselves in this research session: ran a command, compiled something, or read the live source/API. Date is 2026-10-16 unless noted. |
| **[source]** | A research agent read it in a named repo, document or page and the document says it. It is second-hand to us. The repo or path is given where it fits. |
| **[claim]** | Marketing, a forum post, a README promise, or someone's recollection. Not verified. |
| **[guess]** | Inference. Treat as a question to test, not a fact. |

Rules of thumb: the scene's own documentation goes stale in days (section 8 lists examples), nothing was measured on a device by us, and the hub tests no firmware. **Where this guide and a live repo disagree, trust the repo and tell the human.**

How this guide was made: the hub's machine-readable catalogue (52 firmwares) was fetched live; five research agents read the source, issues and release notes of about 80 repositories (nothing was built or run from them); we then re-checked the claims the guide leans on hardest (marked [checked]). Appendix G lists the limits.

---

## 1. The ten things to know first

1. **The chip is more capable than older docs say.** It has a hardware single-precision FPU (`float` works if you pass `-mcpu=r3 -mfprev1 -ffp-contract=off`), a second CPU core, and runs at 360 MHz. Our own `RULES.md`/`CLAUDE.md` ("no float, no FPU, one core") and Dinghy's docs describe Dinghy's default compiler flags, **not the chip**. See 3.3 and 7.4. **[checked]** for the FPU (section 3.3); **[source]** for the second core and clock.
2. **`double` and 64-bit division still do not work.** `a / b` on 64-bit values has no runtime helper. Floats compile to inline FPU code only with the flags above. **[checked]** compile test; ETCH shipped two 64-bit divides in the audio path and crashed after two notes (fixed in its V1.36) **[source]**.
3. **Recovery comes first, because nothing validates your firmware for you.** The hub tests nothing. Package integrity is a CRC-16 with no signature. An install cut off at the wrong moment can leave a unit that no on-device rescue can revive: the boot code does not restart the staged loader from flash on a cold boot, so only a small RP2040 "Transporter" dongle fixes it. Build or borrow the dongle, or at least ship a **boot stub** and never touch the protected flash ranges (section 2).
4. **Never flash a device unless the human asked you to.** Several of the most careful projects state the same rule: the owner flashes by hand, the agent never installs firmware, and the first flash is an unmodified build with a proven round trip back to the stock firmware.
5. **The platform bases are in flux.** Felucca (the original base) went to maintenance mode on 2026-10-11 **[checked]**: its author posted that new features, issue replies and PR reviews are not guaranteed, and points newcomers to `felucca-bsp`, which cannot be built standalone yet. SLOOP is the most-installed base (1,707 of about 7,000 hub installs). M-VAVE itself published an open SDK on 2026-10-13 **[checked]**. Pick your base deliberately (section 4).
6. **Package identities collide.** Twelve hub firmwares report `FM-1_900`; three of our own identities collide with hub firmwares (950, 940, 920). There is no registry. Check the live catalogue before you pick one (10.2).
7. **The audio deadline differs by base.** Felucca 1.x and several newer firmwares render **128-frame** half buffers (2.9 ms); SLOOP, sloopDX, Salt and Dinghy use **256 frames** (5.8 ms). The true sample rate is **44,117.6 Hz** (24 MHz / 544), not 44,100. **[source]**
8. **Float is available but not free.** Melodee's float baseline missed about 40 DMA deadlines a second until it added caching, admission control, SRAM-resident audio code and a voice cap. Measure on a device. **[source]**
9. **There is no analogue audio input, no MIDI out jack, no USB host, no SD slot.** Sampling happens in a browser and is uploaded. USB audio into the FM-1 exists in one test version and in M-VAVE's SDK. See 3.1. **[source]**
10. **Most of the scene is AI-built, openly.** At least a third of the repos have mostly Claude-authored histories, several carry `CLAUDE.md`/`AGENTS.md` and hand-off documents. The social contract is: AI use is fine if you own the result, test on a real unit, keep the credits and the GPL source, and say what was tested on hardware. "AI slop" is the insult people use. **[source]**

---

## 2. Safety and recovery (read before you touch flash)

### 2.1 What actually bricks a unit, as seen in the wild

| Failure | Seen in | Lesson |
|---|---|---|
| An install interrupted after a few hundred of about 1,167 requests: new and old app sectors mixed, staged loader intact, unit hangs before app code | Transporter guide (Quixotic7); Felucca issue #61 | The boot code (SPL) does not start the staged loader from the flash record on a cold boot; only a RAM record works. **"Resume" cannot rescue a half-written app.** A boot stub in the app plus the dongle can. **[source]** |
| Black screen / mask-ROM `WL82 UBOOT1.00` disk after updating | Felucca #61, Melodee 1.3.0/1.3.1 | Often a crash twice at boot. Felucca's two-failed-boots path goes straight to the ROM bootloader (UBOOT), which needs a computer tool or the dongle. SLOOP and Dinghy have an app-level USB rescue (`recovery.c`) that is safer. **[source]** |
| Clock retune (PLL change) | Melodee (320 MHz) | Failed before LCD and USB came up on some units, then fell to UBOOT. **Never retune the PLL.** **[source]** |
| Divide-by-zero trap changed between Felucca releases | Felucca 1.0.x (#61) | The compiler hoists divides ahead of guards. A project that worked on 0.9 reset at boot on 1.0. **Always guard divisors.** **[source]** |
| Custom or modified update loaders | ETCH, FM-106, CS-1, OberVave 1.1 | "Screen flashes, never enters update mode"; "no installer gets another firmware back on". **Test the update path in both directions before releasing.** **[source]** |
| Build that does not link | fm1-chord notes | The unit keeps the previous build and you test the wrong code. **Check the build output for the final "app" line, not only for errors.** **[source]** |
| Core 1 misreads RAM | Jangada, X0X issue #10 | Unit-dependent; see 7.4. Every implementation validates jobs and falls back to one core. **[source]** |
| Battery brown-out during an update | one unproven report | Charge first. **[claim]** |

### 2.2 Never write these flash ranges

| Range | What | Why |
|---|---|---|
| `0x00000-0x03FFF` | Boot head: the SPL (`uboot.boot`), `isd_config.ini` (chip key, system clock) | No software can repair damage here. No installer or rescue tool writes it. |
| `0xFF000` | `key_mac` (chip key) | Never touch. |
| `0xE9000` | Stock Bluetooth pairing store | Also holds data needed for BLE. Avoid. |
| around `0xE7E68` | Radio calibration ("RF trim") on at least one unit | Without it Bluetooth cannot start (`NO RF TRIM IN FLASH`). |
| `0xE0000-0xE4FFF` | Staged update loader and the update record at `0xE4F00` | The platform's. |
| SMK-37 only: `0xC1000` (`USRTRIM`) | Factory calibration | Never write. |

The app area is `0x04000-0x92FFF`; only the update loader writes it. Dinghy's `FL_RANGE_OK` guard allows only the staging area and the settings sector until you widen it consciously. Widen it to a range you have checked against the table in 3.4 and against the hub firmwares you might be installed over.

### 2.3 The recovery ladder (know it before the first flash)

1. **Hold OCT- while switching on** (Felucca-family and Dinghy): USB rescue with the updater only. Felucca additionally accepts OCT- and OCT+ together for 5 seconds, or the SysEx "soft key" `F0 22 24 35 7D F7`. **[source]**
2. **Reinstall from the browser** (hub, Chrome or Edge), from `fm1_install.py`, or "return to official V15" (the hub accepts only M-VAVE's exact V15 file; its SHA-256 starts `db1642b2`).
3. **Mask-ROM UBOOT (`WL82 UBOOT1.00` disk):** use a computer tool: the hub's `fm1_rescue.py` (Linux, root), `MvaveFM1Unbricker` (Windows admin PowerShell or Linux; backs up the full flash first), or M-Flasher. These write only `0x4000..0x92FFF`, only changed sectors, and read back. **[source]**
4. **No USB device appears at all:** only the **Transporter** (Seeed XIAO RP2040 plus a USB-C pigtail, wired D+ and D- and ground, **VBUS not connected**) can send the USB_KEY signal and talk to the mask ROM. Build it before you need it. Alternatives exist (a Pico dongle, a Flipper Zero tool, JieLi's own updater dongle). **[source]**

Tooling details are in section 5.

### 2.4 Rules for your firmware's own safety

- **Leave the OTA/updater on.** Without it nothing can install over your firmware except the dongle.
- **Add a boot guard.** After two crashes in 30 seconds, go to a safe mode or to the USB rescue, not to a dead screen. ChoralRoot (`docs/BOOT-SAFETY.md`) validates the rest of the app from a stub in the first app sector before jumping to it. **[source]**
- **Do not touch `PB1`** (the battery divider). Held low for 8 seconds it is a long-press reset.
- **Save only when silent.** A 4 KiB sector erase stalls everything for 20 to 50 ms (the sound drops). MAKU erases ahead of time into alternating sectors so the user's press never causes an erase. **[source]**
- **Never trust data read back from flash** without a magic number, a length and a CRC. Many firmwares share the same flash window (3.4) and will find each other's records.
- **Plan for a unit that differs:** flash part ID, core-1 RAM reads, clock retune, missing RF trim, and the MIDI port's name all vary between units. **[source]**
- **Keep the first release boring:** unmodified build, install, verify, round trip to V15, then your changes. State in the README what was tested on a real unit.

---

## 3. The device

### 3.1 What is on the board (and what is not)

| Resource | Facts | Status |
|---|---|---|
| SoC | JieLi **AC791N** (WL82 family), CPU **pi32v2** (Blackfin-derived; MAC 16x16 to 64, saturating and packed ops, zero-overhead loops). Board silkscreen `DX7 MB V07`. About $60-80, shipped June 2026. | **[source]** |
| Cores | **Two.** Stock runs the OS, UI, USB and BLE on core 0 and renders the FM voices on core 1. Open firmwares start core 1 themselves (7.4). | **[source]** |
| Clock | **360 MHz** (PLL 540 MHz, system = 2/3; bus 180, LSB 60 MHz, flash controller 90 MHz). The SDK's datasheet says 320 MHz max. Do not retune. | **[source]** (measured by three projects) |
| FPU | Single-precision, with `-mcpu=r3 -mfprev1 -ffp-contract=off`. | **[checked]** compile test (3.3) |
| RAM | 578 KB SRAM total; 32 KB instruction and 32 KB data cache (nominal). The "96 KB RAM + 336 KB pool" split is the **linker script's** choice (Felucca 1.5 moved it to 72/360 KB); PIGEON re-split it; JieLi-SDK builds see about 511 KB. | **[source]** |
| Flash | 1 MiB SPI NOR, JEDEC id `0x856014` (a Puya part). XIP runs code straight from it. About 100,000 erases per sector. | **[source]** |
| Screen | 1.54" IPS, **240x240 RGB565**, ST7789V-class on SPI1. **No reset line and no tearing-effect line.** The panel holds the picture; firmware keeps no frame buffer (it draws strips). Backlight on/off only. | **[source]** |
| Keys | **27 keys (F3-G5) + 14 buttons**, an 11x6 diode matrix behind two 74HC595s, scanned at 10 kHz. 7 encoders (no push switches) decoded in software. Single-colour LEDs (PLAY has a second green LED, REC is red). | **[source]** |
| **Velocity and aftertouch** | **Exist.** The keys are carbon-pill rubber domes; two matrix rows (`PA7`, `PA8`) are ADC inputs, so a weak driven row reads the pill as a resistance. About 20 of 27 keys directly; the top 7 get velocity but no aftertouch. Shipped in CTL-1 and ChoralRoot. See 7.5. | **[source]** (and older READMEs saying "no velocity" are out of date) |
| Audio out | External I2S codec on ALNK0, 24-bit stereo. **True rate 44,117.6 Hz.** The half-buffer DMA interrupt is the audio clock. Mono speaker amp, muted by the headphone jack. | **[source]** |
| **Audio in** | **None.** No microphone, no line in ("no connected analogue input" per M-VAVE's own SDK headers). | **[source]** |
| USB | USB-C, full-speed **device only**. Stock: MIDI + audio (2 out / 2 in). Felucca-family: MIDI + optional serial console + audio **to** the computer. USB audio **from** the computer: exists in JIANT's test builds and in M-VAVE's SDK; ChoralRoot says it cannot. USB is polled from a 10 kHz timer (no USB interrupt). | **[source]** |
| MIDI jacks | One 3.5 mm TRS **input** (UART1 on `PH8`, 31,250 baud, DMA ring). **No MIDI out jack**, and no known TX pin. | **[source]** |
| Bluetooth LE | On the die. Works in stock, in M-VAVE's SDK, in Hortator's separate build and in CTL-1's private build. Costs: about +127 KB code, binary-only libraries (a GPL problem), a radio calibration block, long controller-task runs. 7.9. | **[source]** |
| Wi-Fi | On the die; the board has a wire antenna pad only. **Nobody has run it.** | **[source]** |
| SD card | The chip has SDIO, the board has **no slot**. | **[source]** |
| Debug | No JTAG/SWD, no UART header, no test pads, no recovery button. | **[source]** |
| Power | 3.7 V 2000 mAh Li-Po. Updates run on battery even with USB attached: charge first. | **[source]** |
| Sibling devices | **SMK-37 Pro/Elite, MKE-P37** (same chip, same loader, 37 velocity keys, 16 RGB pads, different flash part and pins). **VIBE FM Mk1**: a hardware announcement (sign-up list); no specs. | **[source]** |

### 3.2 Memory at a glance

| | Size | Notes |
|---|---|---|
| App image (XIP) | **581,564 B** (about 568 KiB) | Flash is the binding limit. Real firmwares sit at 566,028 B (Rainbow) and 573 KB (a few others). Forks drop samples to fit. |
| RAM `.data/.bss` | 96 KB (Felucca 1.5+: 72 KB) | ORBIT ships at 97,064 of 98,304 B. SLOOP freed 38 KB by moving buffers into unused SRAM. |
| Pool (big zeroed buffers) | 336 KB (Felucca 1.5+: 360 KB) | Delay lines, reverbs, sample rings. |
| Stacks | 24 KB user + 8 KB supervisor | Measured high-water: 1.4 KB and 0.9 KB. Generous. Core-1 stacks are tiny (about 2.8 KB). |
| Unused SRAM | **`.lowram` 29.5 KB** at `0x01C00BD0..0x01C07F00` (device-tested by SLOOP); a 28 KB cache-RAM window at `0x01F28000` (not validated) | Free memory nobody else is using. |
| `.ram_text` | 24 KB at `0x01C00000` | Code that must run with flash off (the flash driver, the core-1 wait loop). |

### 3.3 Float: how to turn it on, how to check it (we did)

Default Dinghy/Felucca flags (`-target pi32v2 -Os ...`) turn every float operation into a call to a soft-float routine (`__addsf3`, `__mulsf3`, `__floatsisf`, `__fixsfsi`) that the platform does not link, so the build fails. We compiled a three-line float file with the JieLi clang 4.0.1 in WSL: **[checked]**

- With `-Os`: undefined symbols `__addsf3 __fixsfsi __floatsisf __mulsf3`, and `call` instructions.
- With `-Os -mcpu=r3 -mfprev1 -ffp-contract=off`: no undefined symbols, no calls; the arithmetic is inline.

Recipe used by X0X, Melodee, AMB-1, CTL-1, FiMba, Tuna and others **[source]**:

- Compiler flags `-mcpu=r3 -mfprev1 -ffp-contract=off`. The last one means no fused multiply-add, so the host (PC) build computes the same samples as the device.
- Disassemble with `-mcpu=r3 -mattr=+fprev1`, or the FPU opcodes print as unknown bytes.
- **Single precision only.** Fail the build if a soft-double routine (`__*df*`) appears in the symbols. Melodee builds with `-Werror=double-promotion`. A `1.0` literal is a double: write `1.0f`.
- **No libm.** Write your own `exp2f`, `sinf`, `tanhf`, `sqrtf` (X0X's `fastmath.h` and Tuna's `tuna_libm.c` are the references, with stated accuracy).
- The FPU shares the integer registers, so a big loop can run out of registers and spill. Float moved Melodee's engines from "fits" to "marginal at 4-8 voices". Bubba measured float 1.5x **faster** than its 64-bit-integer biquad.
- Disable floating-point exception traps on both cores (X0X does). NaN, Inf and out-of-range conversions are the unmeasured cases (the QEMU emulator faults on them).
- 64-bit `/` and `%` are still missing. 32-bit divides are fine.

### 3.4 Flash map: who uses what

| Range | Use | Who |
|---|---|---|
| `0x04000-0x92FFF` | The app (written only by the update loader) | everyone |
| `0x93000-0xE8FFF` | Stock V15 key/value store | stock |
| `0x97000-0xE0000` | Projects, samples, presets, patterns | **almost every Felucca-family firmware**, each with its own layout (X0X from `0x97000`; TRIBE a log from `0x97000`; Bubba `0x97000` and `0xC0000`; BEETLE `0x97000-0xDF000`; MC-1 `0x99000-0xAAFFF`; zp12 `0xC4000...`; PIGEON `0xA0000-0xDC000` plus a reclaimed `0x60000-0x92000`; WaveLoop `0xC8000-0xDB000`) |
| `0xE0000-0xE4FFF` | Staged update loader (5 x 4 KiB); update record at `0xE4F00` | the platform |
| `0xE5000, 0xE6000` | Felucca's autosave A/B (also MAKU's "likes") | Felucca, MAKU |
| `0xE9000` | Stock Bluetooth pairing store | stock |
| `0xEA000-0xFBFFF` | Stock factory and user patches (custom firmware overwrites the factory banks) | stock, sample rooms (zp12) |
| `0xFC000-0xFEFFF` | Felucca-family "globals" (Dinghy settings at `0xFC000`; SLOOP at `0xFE000`) | everyone |
| `0xFF000` | `key_mac` | **never** |

Consequences: installing one firmware over another leaves the other's records in flash. All Felucca-family firmwares share one record magic (`FELU`, `0x554C4546`); Dinghy uses its own (`DNGY`). A firmware must treat anything with a wrong magic, length or CRC as empty. An "owner tag" convention (16 bytes naming the family) was proposed by the SMK-37 porting kit and is used by nobody else yet. **[source]**

Power-safe storage patterns in use: A/B sector pairs with a commit header written **last** (Felucca's `storage.c`: magic, type, slot, sequence, length, CRC-32; the newest valid sequence wins); two full copies with a generation counter and CRC (zp12, X0X, MC-1, BEETLE); a log of 512-byte records with garbage collection and a kept spare sector (TRIBE). Polly leaves bytes `0xF00-0xF07` of every data sector at `0xFF` so the update loader never mistakes a data sector for an update record. **[source]**

---

## 4. Choosing a base (decision table)

There is no single obvious base any more. Pick on purpose.

| Base | What it gives you | Watch out for | Verdict |
|---|---|---|---|
| **Dinghy** (zvenson) | A small documented template: update path, USB rescue, MIDI, USB audio, keys, LEDs, screen, settings, build checks. One 193-line app (`dinghy.c`). Our workspace and `fm1.ps1 new` use it. | Docs are stale on float and identities; no second-core HAL; a sine starter, not a framework. SysEx frames are the platform's (see below). | **Best for a clean, understandable start.** First change: add the FPU flags if you want float. |
| **foma** (konsumer) | A Dinghy-like API (`setup/loop/audio`) plus a Docker toolchain and `./fm1 dev` that serves a live WebAssembly emulator, installer and rescue page on localhost. | Docker-only; still states "no FPU". Newer and less proven. | Good if you want a browser dev loop immediately. |
| **X0X / FoMni line** (Charles Vestal) | The most engineered modern lineage: a `plat.h` hardware interface with a device implementation and a host simulator, scenario scripts, a WASM emulator, float DSP with a `fastmath.h`, a second core, MIDI clock, a screen-reader protocol. AMB-1, HYP-1, CTL-1, Polly, FiMba, FuMi, MIN-1 and Fart are built on it. | We have not read it ourselves. Float and core-1 hazards come with it. GPL. | **Best if you want float, dual core and a test culture today.** |
| **SLOOP** (isod89) and its forks | The most installed groovebox base: app-level USB rescue, drum synth, song/arranger, `.lowram`, a second core (2.7), a DX7 engine in the forks. | A squashed-history fork: nobody merges upstream (isod89 merges no PRs; forks hand-sync). Big. | Fork one of its forks, not SLOOP, if you want that feature set. |
| **Felucca** (Hügelton) | The original: one compilation unit, HAL, loader, A/B storage, SysEx editor protocol, a very large test suite. | **Maintenance mode** since 2026-10-11. Felucca's two-failed-boots path goes to UBOOT (needs a dongle). Windows build not documented. GitHub ZIP downloads miss the engines submodule. | Use for reading and for its tests; hesitate to depend on it. |
| **felucca-bsp** (Hügelton, MPL-2.0) | The HAL split out; chip layer, board code per device, loader, a safe hardware-test image. The author tells newcomers to build on it. | "WIP, cannot be built standalone yet; licences under review." | Watch it; do not start on it. |
| **M-VAVE `fm1_open`** | The vendor's own open SDK (released 2026-10-13): GPLv3 app with an SDK linking exception, plus a **proprietary binary engine** (FM1-devices-only redistribution), Apache-2.0 boot files and the stock V15 image. APIs for BLE (peripheral and central), classic Bluetooth audio, USB audio in, TRS MIDI, user flash. Windows build. | An RTOS/SDK programming model, different from Felucca/Dinghy. A binary engine you cannot replace. Status "fluid": a community list said it was taken down; it was live on 2026-10-16 **[checked]**. Nobody has discussed how it combines with Felucca-lineage GPL code. | Evaluate for BLE/USB-audio ideas, not as a default. |
| **JieLi AC79 SDK from scratch** (Bubba, fm1-nes) | An RTOS with about 511 KB RAM and the vendor's stack. | Heavy; Bubba is the one finished example. The first milestone for the SDK route is a serial-console app that can return to UBOOT on command. | Only for projects that need the radios or an RTOS. |

**SysEx and the app:** Dinghy's API says SysEx belongs to the platform and never reaches the app. That is the default, not a law. The platform's `ota.c` takes received SysEx frames from two hooks the firmware supplies (`ota_frame_get` / `ota_frame_done`), and several firmwares use the same channel for their own commands: Felucca's editor protocol (`F0 7D 46 4C <cmd> ... F7`, 1,237 lines of spec), zp12's sample upload (`F0 7D 'Z' 'P' ...`), Polly's flash bank loader and screen mirror, the FoMni family's screen reader (`F0 7D 53 52`). **[source]** and the hooks are visible in Dinghy's `ota.c` header **[checked]**. If your instrument needs a computer-side editor, use this path (7.12).

**A default recommendation for our workspace:** start from Dinghy (via `fm1.ps1 new`), add the FPU flags only if the design needs them, copy the second-core HAL only after a one-core version works and is measured, take the testing ideas from the X0X/Felucca lines (host simulator with scenario scripts, golden renders, a static cost budget), and keep Felucca-lineage code at arm's length until its status settles.

---

## 5. Toolchain, packages, installing, and fast loops

### 5.1 Toolchain and SDK files

- **Compiler:** JieLi's pi32v2 clang 4.0.1 (a closed fork). Linux x86-64 download: `https://pkgman.jieliapp.com/s/linux-toolchain` (Dinghy's `tools/get_toolchain.sh` puts it in `~/.jieli/toolchain`; dated tarballs include `20250324.1`, recommended by one project, and `20260730.1`). Windows installer: `https://pkgman.jieliapp.com/s/win-toolchain`. macOS builds run it in a `linux/amd64` Docker container. **[source]** Licence: the vendor's page says no licence file is needed; **no redistribution grant was found**, so everyone treats it as an external prerequisite (konsumer's Docker image that downloads it is an unresolved question). **[source]**
- **In our workspace** the toolchain is installed in WSL Ubuntu at `~/.jieli/toolchain` and `clang 4.0.1` runs there **[checked]**; builds and host tests run in WSL, flashing runs on Windows Python.
- **SDK files:** Felucca-family builds need only **three binary files** from the JieLi AC79 SDK (tag `AC79NN_SDK_V1.2.1_2023-12-13`, Gitee `Jieli-Tech/fw-AC79_AIoT_SDK`, Apache-2.0): `cpu/wl82/tools/uboot.boot` (the SPL), `cpu/wl82/tools/cfg_tool.bin`, `cpu/wl82/cfg/eq_cfg_hw.bin`. Their SHA-256 values are pinned in `tools/build.py`. They are packed into every `.fwsc`. M-VAVE now ships the same boot files under Apache-2.0 in `fm1_open/sdk/tools/pack/`, together with the stock V15 image. **[source]**
- **Flags every project uses:** `-target pi32v2 -Os -ffunction-sections -fno-builtin`, plus the FPU flags (3.3) if you use float. The register map is public: `include_lib/driver/cpu/wl82/asm/WL82.h` (1,330 lines) in the SDK.
- **Where to read about the chip:** `kagaimiq.github.io/jielie` (the ISA in `cpu/pi32v2.md`, firmware container formats, the USB_KEY procedure), `ip2k/lunar-modulator` (the best hardware write-up), `AL-255/FM-1-RE` (stock firmware analysis), `cicloid/awesome-fm-1` (the best index of everything), `Keitark/fm1-nes` (the SDK route). **[source]**

### 5.2 The `.fwsc` package and the identity

- A `.fwsc` is M-VAVE's own updater format. It holds two programs: the **update loader** (`ota.bin`) and the **image** (boot head plus app area). Integrity is a **CRC-16 only, no signature**, which is what made custom firmware possible. The app area is encrypted with the chip key `0x980F`. Dinghy's `tools/fm1pkg_make.py APP.bin OTA.bin OUT.fwsc --product FM-1_9NN --sdk DIR` builds it. **[source]**
- Packages are fixed-size containers: 51 of the 52 hub packages are 609,649-610,990 B; the stock V15 package is 699,956 B (and Bubba, which keeps M-VAVE's loader, is the same size). Your code's size barely changes the file size. **[checked]** from the catalogue.
- **Identity** is a product string woven into the first 20 blocks of the package (maximum 20 characters). A running unit reports it over USB and the loader reports `ota-FM-1_900` while updating. The hub uses it only to mark which card is "on your FM-1". Two firmwares with the same identity break nothing, but the hub cannot mark a card. `FM-1_015` is the official V15: do not use it. The hub's identity check accepts `FM-1_` followed by 3 to 8 digits.
- **Schemes in use:** Felucca's `--release X.Y` gives `FM-1_9` plus two digits (so every 1.0.x is `FM-1_910`). Several families encode the version in seven digits: a two-digit project prefix, then major, two-digit minor, two-digit patch (X0X 1.1.0 is `FM-1_9010100`; AMB-1 1.2 is `FM-1_7010200`). Prefixes already used: 30, 50, 60, 70, 80, 90. **[source]**
- **Resume of an interrupted install works** only for loaders the installer recognises: Felucca's (`FM-1_9NN` style) and M-VAVE's. **[source]**

### 5.3 How an install works (so you can reason about failures)

1. A browser (Chrome or Edge only; Web MIDI with SysEx) or `fm1_install.py` (Python 3.12 with `mido` and `python-rtmidi`; Python 3.13 has no `python-rtmidi` wheel) finds the FM-1 and sends the handshake `F0 00 32 45 00 00 00 40 7F F7`; the running firmware answers with its identity.
2. Upgrade command `F0 22 24 35 7F F7`. **Step 1:** the *running firmware* pulls parts of the package (512-byte reads), stages the package's own update loader at flash `0xE0000-0xE4FFF` (marker `FELUCCA-LOADER-1`), writes a 112-byte update record at RAM `0x01C7FD88` and resets. The SPL runs the loader; the FM-1 re-enumerates as `ota-FM-1_9NN`.
3. **Step 2:** the loader pulls the whole image and writes the app area (`0x4000..0x92FFF`) in 4 KiB sectors, then the host answers a request with `success`; the unit restarts into your firmware. About a minute.
4. Why two steps: the unit cannot overwrite the program it is running, so every package carries its own loader.
5. **Do not unplug while it says Writing.** An install cut off here is the dangerous case (2.1).

**[source]** for all of it; the SysEx dialect is M-VAVE's "M-UPGRADE" (`F0 pack7(00 59 cmd len24 body chk) F7`). `FM-1-RE/tools/fm1_ota.py` is a byte-exact reimplementation with protocol tests.

### 5.4 The hub, and getting listed

`https://thefirmwarehub.com` is the registry and the installer. **[checked]** (catalogue fetched 2026-10-16 12:31 UTC):

- 52 firmwares by 43 authors; 6,994 installs in total; 12 added on 2026-10-08, 13 on 10-13 and 9 on 10-16. Kinds: 28 synth, 26 groove, 9 chords, 7 other, 6 drums, 2 dev. 14 have a browser editor and 32 a "play in the browser" simulator. 26 are marked beta (version below 1.0, or a name saying beta/alpha/rc/test).
- The catalogue is public JSON at `/catalog.json` (fields: `id kind name author repo ident what editor play added loader vintage stars version date pkg product size sha256 release source mirror beta installs`, plus an `smk37` block). Read it to see what exists and which identities are used.
- The hub **polls each listed repo's GitHub releases several times a day**, takes the newest release, **mirrors the `.fwsc` byte for byte, pins its SHA-256**, and the browser re-checks the hash before writing. A failing release is not offered; the last good one stays. **It does not test that a firmware works, sounds good or preserves data.**
- **To be listed:** it makes music; the source is public under a licence that allows passing it on (GPL-3.0 expected); a `.fwsc` is attached to a GitHub release (or there is a web installer); the package carries an update loader and an `FM-1_` identity. Submit through the hub's "Submit a firmware" form. Closed-source firmwares are not mirrored (Baud Girl's FM-1+VA and a paid "Groove OS" exist as link cards).
- Other directories: `fm1-editor.com/firmware`, `stanleygurvich/fm1-firmware-database`, `wero1414/atlas-fm-1`, `cicloid/awesome-fm-1`, and a Piano & Synth collection page that lists 61 firmwares.
- **The hub operator also authors firmwares** (Dinghy, zp12, sloopDX, OberVave) and announces the VIBE FM Mk1 hardware. Observation only.

### 5.5 Fast loops (nobody has hot reload; these are the nearest)

| # | Loop | Turnaround | Proves | Cannot prove |
|---|---|---|---|---|
| 1 | **Host build against a fake FM-1**: compile your app with `cc` and a stub platform; script key presses, knob turns and MIDI from a text file; get a WAV and PNG screenshots. (Dinghy `tests/host.c`; the same idea in X0X, SLOOP, FiMba, CTL-1, Felucca. Our projects use it: `fm1.ps1 host`.) | seconds | Logic, drawing, DSP numerics (bit-exact if built with `-ffp-contract=off`), regressions | Interrupt timing, flash stalls, USB, chip quirks, cache behaviour |
| 2 | **Static cost budget from the disassembly** (Felucca's `tests/target_budget.py`: instructions in each function's loops, x4 per nesting level, divides x9, +10 % tolerance); Dinghy's build also fails on image/RAM/pool size, ROM-address constants, register access outside `hal/`, and calls inside `.ram_text` | build time | A blow-up before it reaches a unit | Real cycles (a cache miss costs about 470) |
| 3 | **Browser WebAssembly of your firmware sources** with a stub HAL: Emscripten (Felucca `web/emu`, GHOULBOX, CS-1), `zig cc` wasm32 with an AudioWorklet (Jangada Studio, Salt), or `zig cc` plus `wasm-opt --asyncify` so a main loop that never returns still runs (OberVave, zp12). 32 of 52 hub cards have one. | seconds | The same sources with a simulated platform; ideal for users to hear it | The chip; USB; real timing |
| 4 | **CPU-level emulators running the real `.fwsc`:** `simonjohansson/fm1-emulator` (QEMU-based; macOS arm64 and Linux x86-64 only; no USB-MIDI; "8 ns per instruction" so **not a load profiler**); `joakimeriksson/fm1-emu` (Rust; both cores; runs as a browser page or an AU/VST3/CLAP plugin; claims 47 firmwares boot); `shcherbakov7/m-wave-fm1-browser-emulator` (Rust/WASM). The hub tells authors to boot their release in the QEMU emulator before publishing. The two main emulators **disagree** about whether AMB-1, TRIBE, DUELL and FM-106 boot. | minutes | The package boots; screens and input paths work | CPU load; Windows support is unproven |
| 5 | **On-device diagnostics:** the optional USB serial console (`status` prints `audio_max_us`, `cpu_pct`, crash records), the crash screen (vector, PC and return address; look the PC up in `build/*.dis`), bespoke consoles (Bubba `traza.py`, Hortator BENCH, X0X PERF test, PIGEON diagnostics command 97) | per install | Real timing and stalls | Needs the flash loop |
| 6 | **RAM-run through the ROM bootloader:** `fm1t.py ramrun IMAGE.bin` (needs the Transporter dongle) loads an image into RAM and jumps; flash is never written. Felucca's linker keeps `0x01C06000..0x01C08000` for this. Nobody documents a full app run this way. | per run | The safest way to try risky hardware experiments | Needs a RAM-linked build and the dongle |
| 7 | **Flash and try:** web installer or `fm1_install.py`, about a minute. | a minute | Everything | A bad boot path can still need the dongle |

**foma** (`./fm1 dev` serves a live emulator, installer and rescue page from one command) is the nearest thing to a one-command loop. **[source]** For the cost of a unit in CPU terms, the common estimate is "host instructions per sample": Felucca counts about 1.7 % of real time per 100 host instructions per sample (DUELL calibrated: 420,000 host instructions per 128-frame half equals 100 %, and a device reads 10-25 % lower). MIN-1 estimates CPU from retired instruction counts. **These are estimates; only a few projects have device-measured numbers (7.2).**

### 5.6 Tools and sources worth knowing

- **Recovery tools:** `kagaimiq/jl-uboot-tool` (MIT; talks to the mask ROM; Linux and Windows, no macOS; uploads a 24 KB flash loader to RAM first), `Quixotic7/MvaveFM1Unbricker` (restore plus a mandatory double backup; refuses unless the chip key is `0x980F` and the flash id `0x856014`), `kurogedelic/FM-1-transporter` (the RP2040 dongle firmware and `fm1t.py`: `info`, `dump` in about 3 s, `write`, `ramrun`), M-Flasher (Rust GUI and CLI for Windows, macOS and Linux), the hub's `rescue/fm1_rescue.py` (Linux, root). `fm1-bootloader` (an open rebuild of the SPL that adds key entry to download mode) is **not tested on hardware**. **[source]**
- **Port and companion kits:** `genkigenki/fm1-to-smk37` (move an FM-1 firmware to the SMK-37 keyboard: board layer as commits, an `AGENTS.md` playbook for a coding agent, "keep the FM-1 build byte-identical"); `konsumer/fm1buddy` (an external USB-host box for the FM-1 and an SD-card firmware switcher).
- **Where hardware facts live:** the HAL header comments in Felucca/Dinghy (`fm1_input.h`, `fm1_lcd_hw.h`, `fm1_flash.h`), `fm1-to-smk37/docs/01-hardware.md`, `felucca-bsp` issues #1 and #2, `lunar-modulator/docs/01-hardware.md`. Hardware facts live in prose, not datasheets: expect gaps and cite what you measured.

---

## 6. The landscape

**What exists** (52 hub firmwares; full table in Appendix A):

| Family | Firmwares | What it shows |
|---|---|---|
| **Felucca and direct forks** | Felucca, Salt, WaveLoop, Felucca 8 Patterns, Skiff, JIANT, Rainbow Mode, MAKU, Tuna, GHOULBOX, NOIZ DEMONS, Hortator, DUELL, Jangada (with SLOOP parts), Melodee (a rewrite of its design) | One compilation unit, A/B storage, an editor protocol; forks keep a pinned import, a patch or a hand-synced copy. |
| **SLOOP and its forks** | SLOOP, sloopDX, SLOOP DX7 Banks, Floyd FM, winampFM, zp12, ORBIT, Mycelium (a content pack), Vector37 | Groovebox, DX7 engine and banks, drum synth, song mode; the biggest user base. |
| **X0X / FoMni line** (Charles Vestal's platform) | X0X, FoMni, FiMba, FuMi, Polly, CTL-1, AMB-1, HYP-1, MIN-1, Fart Machine One, TRIBE | A host simulator, scenario scripts, a WASM emulator, float, dual core, a screen-reader protocol. |
| **Dinghy-based** | FM-106, CS-1, Frodhes, OberVave, (Dinghy itself), fomajam via `foma` | Small documented bases; effects-heavy synths. |
| **From scratch on the JieLi SDK** | Bubba Machine, `fm1_open` (M-VAVE) | An RTOS, about 511 KB RAM, BLE and USB-audio stacks. |
| **ChoralRoot family** | ChoralRoot, CoralFlower, PurpleMonkey | Velocity, dual core, a boot stub, a device manager, designer mock-ups. |
| **Closed** | FM-1+VA (Baud Girl), Groove OS | Link cards only. |

**By kind:** 28 synths; 26 grooveboxes; 9 chord instruments; 7 other; 6 drum machines; 2 dev kits.

**Who made what with what:** Charles Vestal five firmwares; zvenson four plus Dinghy; Quixotic7 two plus a recovery tool. SLOOP has the most installs (1,707), then Felucca (727), JIANT (606), X0X (477), GHOULBOX (414), ChoralRoot (251). **[checked]** from the catalogue.

**Firmwares worth reading, and what each proves** (one line each; more in Appendix A):

| Firmware | Why read it |
|---|---|
| **X0X** (Vestal) | The reference for float DSP (`fastmath.h`), dual core with bit-identical output, a load guard that sheds work above 92 % block load, event-sliced sequencing, scenario-script tests. |
| **Melodee** (keremimo) | The most careful memory and CPU engineering: audio graph in SRAM (`.audio_text`, 323 resident functions), weighted voice budget, 70 scales, FM6/CZ/Prophet engines in float; also the clock-retune failure story. |
| **Jangada** (zednaked) | Dual core (AUTO/OFF/BOOST) with the supply-voltage fix; a browser Studio compiled from the firmware C with a native-versus-WASM bit-for-bit test. |
| **ChoralRoot** (Quixotic7) | Velocity and aftertouch from the keys, dual core with a single-core fallback package, boot-safety stub, a web device manager, about 35 design documents. |
| **zp12** (zvenson) | A sampler that never samples on the device: browser recording, SysEx upload, 12-bit samples played in place from flash, a sequencer in the audio interrupt, A/B 40 KiB records. |
| **Polly** (Vestal) | Bring-your-own-ROM (a QY70 bank decoded from the user's own ROM into 254 KB of flash), SysEx screen and panel mirroring, an undo journal, a GM synth from 200 KB of samples in flash. |
| **GHOULBOX, CS-1, FM-106, Tonitor, AMB-1** | Five reverbs and two chorus/tape designs with stated costs (7.6). |
| **Tuna** | A Marbles port running inside the audio interrupt at a quarter rate, its own libm, melody statistics tuned on 3,486 public-domain scores. |
| **Bubba Machine, JIANT** | The two firmwares that probe the hardware hardest: SDK-based with a CPU lab; USB audio IN and 10-channel stems. |
| **Hortator, CTL-1** | The only BLE-MIDI builds, and the licensing knot they hit (7.9). |
| **PIGEON** | MP3 decoded in the main loop with libmad; re-partitions RAM/pool; a 496 KiB sample bank. |
| **Rainbow Mode, PurpleMonkey** | Children's instruments; hand-off documents listing numbered bugs; LPC speech in 6.7 KB. |
| **MAKU** | UI rules written for the next firmware ("screen is a map of the knobs", fixed LED semantics), erase-ahead storage. |
| **FuMi, CoralFlower** | Platform-integrity guards (a script diffing the protected paths against a pinned upstream), designer-first specs. |
| **FOMAJAM / foma, fm1buddy** | A one-command dev loop, a fixed MIDI CC map, an ADPCM drum ROM with a flash budget, an external USB-host box. |

**How the scene works:** about a third of the repos are three to nine days old; one human author per repo, openly assisted by coding agents; many carry honest "not yet verified on hardware" sections. Hardware validation lags code by days. CPU numbers are mostly estimates.

---

## 7. Techniques people have worked out

Each item names where to read the code. Everything here is **[source]** unless marked.

### 7.1 Timing and the audio interrupt

- **Audio is a DMA double buffer.** `app_audio(int32_t *out, uint32_t n)` fills `n` stereo frames, Q15 (±32767), added to a zeroed buffer. The half-buffer interrupt **is** the clock. Budget: 5.8 ms for 256 frames (2.9 ms for 128); aim for under half. The platform adds the MASTER knob, a clip, the USB audio tap and the DAC (6 dB under full scale). Inside the interrupt: no flash, no screen, no `platform_poll()`, no waiting.
- **Run the sequencer inside the audio path, with one clock.** Every serious sequencer here does it: zp12 calls `sq_block()` before each 32-frame render with a Q16 tick accumulator (96 PPQ); X0X/TRIBE/HYP-1 slice the render at the next event (`k = seq_until_event(n); render k; seq_advance(k)`) so a step lands on an exact sample; Hortator carries the remainder of each division (5512, 5513, 5512, 5513) so nothing drifts against anything else or an external clock; MC-1 renders slices of 16 frames that never cross a step; Bubba counts 96 PPQN from the audio sample counter.
- **UI edits reach the sequencer through a request ring drained at a block start** (zp12 `sq_post`, X0X's command queue). The main loop never edits a structure the interrupt is reading. Share through `volatile`, one writer each; a 32-bit store is atomic, a struct is not.
- **External MIDI clock (24 PPQN)** is handled as a pulse stream with averaging (Hortator averages tempo over 4 beats) and falls back to the internal clock when pulses stop.
- **Higher-priority interrupts nest:** the 10 kHz timer (keys, USB poll, millisecond counter) is **above** the audio interrupt, so it runs inside your render time. Count it in your budget.
- **Control rate:** envelopes, filter coefficients and LFOs once per 16 or 32 frames is plenty.

### 7.2 CPU budgets and how to measure

- **Measure on the device.** `fm1_ticks()` (a free-running 24 MHz counter) before and after; divide by 24 per microsecond. Show it on screen (CYBERDECK and most of the others have a CPU meter that reports the worst of the last N blocks).
- **Device-measured numbers that exist** (so you can calibrate): X0X 43 to 32 % of a core and worst case 115 to 81 % with drums on core 1; SLOOP 31 to 23 %; Jangada two cores about 21 % versus one core about 31 %; Frodhes eight notes with every effect about 4,800 of 5,800 µs; Melodee five Prophet voices 2,141 µs (of 2.9 ms); the UI alone costs about 30 % of a core in Bubba (9-10 ms per frame). The "heavy case" regime is 40-85 % average with peaks 15-20 points higher.
- **Estimate in CI:** host instruction counts per sample (a Linux perf counter, valgrind callgrind with a tolerance of +8 to +25 %), a static loop-cost scan of the disassembly (7.15), and golden renders. These catch regressions; they do not give absolute load.
- **Reading a "CPU %" claim:** check whether it is device-measured, host-estimated or emulator-estimated. The QEMU emulator's timing is a fixed policy of 8 ns per instruction and is not a measurement.

### 7.3 Voices and admission control ("polyphony" is a budget)

Everybody ended up with a shared voice pool plus a shedder rather than a promise of N voices:

| Firmware | Policy |
|---|---|
| Felucca | Two half-buffers in a row above 85 % of the block fade one voice (keeps the lowest note of POLY and the MONO lead). |
| X0X | `engine_load` guard: block above 92 % or average above 85 % drops the 303s' 2x oversampling and shortens 808 tails; restores after 3 s under 65 %. |
| Melodee | Weighted budget of 16 points (Prophet 5-8 per track, FM6 8-16, CZ 5-8, SID 4, drum 8), a 5-slot cap, admission control. |
| DUELL | A voice cap predicted from a per-engine cost table; "part FX off" saves up to 11 % CPU. |
| ETCH | Back-off 8, then 6, 4, 3, 2 voices on missed deadlines. |
| Bubba | Block above 85 % fades the quietest voice in 2 ms. |
| JIANT | LOAD FULL / LITE / ECO swaps heavy parts for light relatives. |
| Skiff | Each part protects one voice; two halves above 85 % fade one. |
| Hortator | Eight voices over eight tracks; "heavy" models count 2. |

Rules that recur: fade a stolen voice over 2-6 ms (no clicks); steal the oldest released voice first; latch drone voices so other tracks cannot steal them (GHOULBOX); tell the user when you shed.

### 7.4 The second core

Shipping in X0X, Melodee, Jangada, SLOOP 2.7, Felucca 1.6 (off by default, beta), CS-1, FM-106, JIANT, TRIBE, sloopDX 4.0 and ChoralRoot. Rejected or off: Frodhes, DUELL.

**Start sequence** (X0X's `hal/fm1_cpu1.h` and Felucca's copy are the references): write the entry address to RAM word `0x01C7FFF8`; hold bit 3 of `0x10008`; set bit 3 and clear bit 1 of `C1_CON` (`0x1EEE004`). Core 1 boots through the mask ROM, so the program-counter guard windows must be open until it reports in. **Its idle loop must live in RAM** (`.c1_text`) so an idle core never fetches from flash while core 0 is writing flash. Core 1 stack: tiny (X0X 2,816 B, deepest use 828 B) but `usp` needs a real stack. Do not start it before audio is initialised; hold it before any reboot, update or loader entry.

**Hazard (unit-dependent, cause unproven):** on some units core 1 reads words that are not in RAM (`0x00200000`), more often under load (Jangada measured 40 bogus jobs in 13 s idle). **Every implementation** validates each job (function, argument and a job-number check word), counts bad ones, and falls back to one core ("barred" until power-off). The fix that helped most units: M-VAVE's own firmware runs the core supplies at VDC14 1.60 V and SYSVDD 1.38 V while Felucca-derived firmwares ran 120-200 mV lower; raising them to 1.50 V/1.38 V (Jangada) or 1.60/1.38 (X0X 1.0.9) before starting core 1 fixed most units.

**How to use it well:** hand core 1 independent jobs per block through a single-slot mailbox (reverb buses on the previous block's sends; whole voice groups), not tightly synchronised work. Bubba got 1.83x with two independent jobs but 123 % load with tasks that synchronised several times per block. **Keep output bit-identical between one core and two** and test both (SLOOP's method: a host double of core 1 with `CORES=1/2`, `CHAOS` and `CPU1_TIMEOUT_AT` switches). Ship a single-core package as fallback (ChoralRoot does). Core 0 must never turn the flash off while a core-1 job is outstanding.

### 7.5 Velocity and aftertouch (the "no velocity" keys have velocity)

Solved by a measurement trick; no hardware change. The key pill's resistance falls with force. Matrix rows `PA7`/`PA8` are ADC channels 0/1; sample the row with a weak (about 2.4 mA) driven level and it reads like an ohmmeter. Velocity is the lowest driven reading in the first two driven samples after touch, mapped on a log curve (about 0.7; a 604-note test gave quartiles 40/59/74). Aftertouch is a lean against the key's own rest reading. The top 7 keys (`PA6`, no converter) are probed through `PA10` and get velocity only. About 20 of 27 keys directly.

Constraints: the ADC conversion must be started from the 10 kHz scan tick (about 60 µs), so the main loop must stop calling `fm1_adc_read`; the cost is about 1 µs per tick (about 1 % of core 0); the note is delayed by 8.8-13 ms; **units differ by about 120 counts at the firm end**, so floor and curve need a setting (CTL-1 learns a per-key floor and ceiling as running quantiles). Read `choralroot/docs/VELOCITY.md`, `ctl/firmware/src/app/sense.c`, `fm1-velocity` and `fm1-monk`. Felucca users have asked for it as a modulation source (discussion #270). Out of 15 chord/controller firmwares only two ship it.

### 7.6 DSP techniques people reused

| Area | What is in use (where) |
|---|---|
| **Oscillators** | PolyBLEP with a Q15 residual (Felucca `dsp.c`; OberVave's variant has no divide); mip-mapped band-limited single-cycle tables chosen by phase increment (ETCH 512 points x 6 levels; Vector37 10 octave bands, 50.8 KB); Walsh sums (Jangada); half-rate voices with linear up-interpolation (Tonitor); modal resonators with exact poles (Frodhes); precomputed per-key tables for physical models. |
| **Filters** | Simper TPT state-variable filter almost everywhere; transistor ladder with saturated feedback (Jangada, FM-106 IR3109); SEM morph (OberVave); formant and comb filters; four-pole models in float (Melodee). |
| **Nonlinearities** | 256-point interpolated tanh soft clip (Felucca, in nearly every fork); round-toward-zero multiplies and a DC blocker to stop integer limit cycles (CS-1 `mulq15r`, GHOULBOX `hl_mul`). |
| **Reverbs** | GHOULBOX Dattorro in the delay buffer (a 32,768-sample ring, zero extra RAM, mono); Tonitor plate (85 KB int16, stereo taps); OberVave plate (about 550 host instructions per sample); Vector37 half-rate plate; AMB-1 float plate (about 139 KB) plus a 5.6 s tape loop (124 KB); CS-1 8-line Householder network with octave shimmer; Jangada room/spring/plate aliasing one 9.9 KB buffer set; DUELL six types with a low cut (20-450 Hz). Rule from several: low-cut the reverb input; keep tanks mono unless taps are cheap. |
| **Chorus, tape** | FM-106's MN3009 BBD emulation (1,024-sample lines, antiphase LFO, Hermite read); GHOULBOX tape (0.5 Hz wow, 7 Hz flutter); Jangada tape/hum/dust/duck/crush. |
| **Granular and wavetable** | Tonitor (0.56 s half-rate mu-law buffer, 6 grains); Jangada GRAIN; ETCH drawn wavetables; DUELL 29 tables x 19 frames and A/B morph. |
| **Tuning** | Melodee 70 scales (Scala `.scl`, hundredths-of-cent tables, degree-addressed notes); GHOULBOX per-track temperaments; FuMi's tunings measured from two recordings. |
| **Generative** | Tuna's Marbles port; AMB-1's seeded engine; Jangada's Euclidean rhythms and cellular automata; MAKU's "in-key by construction" (notes can only be scale degrees); MIN-1's process models. |
| **FM and DX7** | SLOOP/sloopDX run a six-operator engine ported from Dexed and import DX7 SysEx (a bank or a voice); Melodee has FM6/CZ/Prophet in float. DX7 `.syx` is the only patch format shared across firmwares. |
| **Speech** | PurpleMonkey's LPC-10 player: 33 words in 6.7 KB, a ten-stage lattice filter, 12.5 ms frames. |
| **Drums** | 808/909 models (X0X, FOMAJAM), drum ROMs as 4-bit ADPCM within a flash budget (FOMAJAM: 430 KiB), SoundFont export of an engine (FuMi). |

Fixed-point notes from the repos: 32-bit divides are fine in the sample path; avoid 64-bit divides; the whole tradition is Q15 levels and 32-bit phase accumulators, now with float as an option.

### 7.7 Where code runs: SRAM versus flash (a finding we have not applied)

Code and constants run from flash through a 32 KB instruction cache. **Random reads of flash constants cost about 1,306 ns versus 58 ns from RAM (about 470 cycles per miss); sequential reads are almost free.** DUELL found that moving hot functions into SRAM cut missed halves two to four times (`tools/hot_fns.py`, `DUELL_HOTRAM=3`); Melodee put its whole audio graph in SRAM (`.audio_text`, 323 functions, checked at link time). Counter-evidence: MC-1 tried `.dsp_text` and reverted after a device regression and noise, and Bubba found RAM versus flash made no difference for hot loops. **Conflicting results: measure.** Also: `-O2` for hot units (X0X's `O2_UNITS`; Bubba's `-O2` saved 25 % CPU) instead of the family default `-Os` is under-used. In an SDK-based build the default left only 4 KiB of effective data cache; `CONFIG_DYNAMIC_SDRAM_ONOFF_ENABLE` gave 32 KiB and 6-10x faster random reads (Bubba; untested on bare-metal firmwares). Our own CYBERDECK, TRIPDECK and MLdeck do not do any of this yet.

### 7.8 Storage and data safety

- **Records:** magic + format version + length + CRC-32; reject anything out of range and start from defaults (FoMni's `project_valid`, FiMba: "add enum values at the end"). Add an owner tag.
- **Save only when silent;** or erase ahead (MAKU alternates two sectors and erases the spare in advance).
- **Two copies with a generation counter** survive a power cut (zp12's 2 x 40 KiB project records).
- **Where firmwares keep bigger data:** user samples read in place through XIP (zp12), IMA-ADPCM slots (X0X), a range-coded bank written over SysEx with a directory written last (Polly), a log-structured store (TRIBE). A firmware that widens the write guard must say so, and must not cross the update record, the Bluetooth store or `key_mac`.
- **Back up before you install over another firmware** and offer the user a backup (ChoralRoot's installer does one). The hub warns that switching firmware "may overwrite" data; Felucca-family firmwares can read each other's records, others cannot.
- **Do not reuse another firmware's spare settings bytes** without an audit (Rainbow and Melodee both use Felucca's spare bytes).

### 7.9 Bluetooth (BLE MIDI works; the cost and the licence are the issue)

Works today in stock, in M-VAVE's SDK, in Hortator's separate `-bluetooth` build (a BLE central that remembers a device; DAW-mixer mode) and in CTL-1's private build. The controller and host are binary-only libraries from the Apache-2.0 SDK (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, two crypto libraries). Hortator's LTO link with an LE-only, no-TWS configuration is a 424 KB image versus 297 KB without BLE (about +127 KB), a 13.8 KB heap (10.8 KB high-water), pool 322 KB. The controller runs as an RTOS task with runs up to 18.7 ms at start and connect, **which crashed CTL-1's GM synth until audio was muted during start-up**. The radio calibration block ("RF trim") must be present in flash. **Licence:** a GPLv3 binary that links the binary-only libraries cannot ship with complete corresponding source; CTL-1's author refuses to distribute the Bluetooth build and has asked Felucca's copyright holders for a section 7 permission; Felucca's author cites an unresolved Japanese radio-law question for the FM-1's wireless functions. **Plan for this before you promise BLE.**

### 7.10 Samples, USB audio and audio in

- **There is no way to sample live on the device.** The sampler pattern is: record in the browser (`getUserMedia`: microphone, interface or the computer's line-in), trim and resample in the page, send over SysEx into flash, play in place via XIP (zp12: 12-bit, 2 samples in 3 bytes, 26 or 27.5 kHz; 96 KB free, 2.5 s, or 6.3 s with an extra bank room). Variants: PIGEON's off-line `.kob` bank (indexed MP3 or IMA ADPCM, decoded in the main loop into a ring the interrupt reads); X0X's three 80 KiB ADPCM slots; MC-1's 16 baked instrument excerpts; Fart Machine One's importer taking 1-27 WAVs. Bubba records **its own mix** on the device (a biquad and decimator to 16 kHz 8-bit in a 48 KiB arena), which is resampling, not input.
- **USB audio is more flexible than Dinghy's API says.** Seen in the field: 44.1 and 48 kHz stereo via a fixed polyphase FIR (ORBIT, Felucca `uac_fir.h`); clock-locked stereo with a "FULL" level independent of MASTER (X0X); ChoralRoot's 6-channel master + chord + bass stems; JIANT's IN+OUT (the host's audio comes **into** the FM-1 through a USB audio OUT endpoint with a ring and ring-fill-controlled interpolation, in a test version) and 8/10-channel stems (an owner had called that infeasible). Android passes only some channels to apps. macOS 13-15 hides the audio device when the serial console is present (Felucca #67, still open).
- **Content you cannot redistribute:** Polly's bring-your-own-ROM bank, FuMi's SoundFont export, FOMAJAM's ROM-derived drums built at build time from a pinned download. Mycelium ships factory drum samples whose distribution rights were not verified inside a GPL repo; BEETLE's full provenance audit is the opposite. Document the provenance of every sample.

### 7.11 MIDI

- **USB-MIDI** in and out everywhere. **TRS MIDI in** (UART1, DMA ring polled from the timer, repackaged as USB-MIDI packets so the engine cannot tell sources apart): X0X, Hortator, ORBIT, MC-1, Skiff, Mycelium, HYP-1, PIGEON, JIANT, CS-1, FM-106, Melodee, DUELL, Salt. The TRS jack is input only; one X0X user confirmed a DIN-to-TRS Type A cable works.
- **Clock:** in and out over USB and in on TRS in many; start/stop; follow with averaging.
- **CC maps are ad hoc.** Felucca's standard (71-75, 91/93/94) is reused widely; FOMAJAM's `CC.md` is the most complete fixed map (channels 1-9 tracks, 10 selected track, 16 song/FX); ORBIT has 16 persistent MIDI-learn slots; JIANT CC16-23. Felucca has MIDI learn; few others do.
- **Roles beyond sound:** a MIDI "brain" that follows chords (Polly, FoMni), a controller (CTL-1: velocity, aftertouch, 4 CC banks, Euclidean drums, program change), a remote panel for a browser instrument (HYP-1 on channel 16; PIGEON via SysEx with a 1.5 s lease), per-part output channels (Tuna).
- **Not seen in any surveyed firmware:** MPE, USB-host MIDI (only the external fm1buddy box), a MIDI out jack (the hardware has none).

### 7.12 Editors, protocols and SysEx

- **Web MIDI with SysEx permission** (Chrome or Edge; secure context or localhost). The app receives SysEx frames through the two hooks mentioned in section 4 (`ota_frame_get` / `ota_frame_done`).
- **Frame header convention:** `F0 7D 46 4C <cmd> ... F7` for Felucca-family editors (7D is the non-commercial ID); forks add their own command numbers (collisions already exist: 34-38 and 74-77 mean different things in different forks). One request at a time; "a frame sent early is dropped whole"; 7-bit packed data or 14-bit biased values; **parameter ids move between releases** (Felucca's `P_COUNT` went 69, 89, 91, 99, 104, 111): "always take parameter offsets from INFO". INFO returns tagged capability blocks; Salt's rule is "negotiate, do not commandeer an existing command".
- **Other dialects:** zp12 `F0 7D 'Z' 'P' ...` (HELLO, READ, ERASE, WRITE with a guarded range and read-back, REBOOT, ...); FoMni/Polly/CTL-1 screen reader `F0 7D 53 52` (the device sends the screen's own text; nothing is sent unless a reader says HELLO); Polly's screen-and-panel mirror (RLE RGB565 frames over SysEx, with remote play and a test session); SLOOP's proposed "PANEL push".
- **Editors as the same code:** Jangada Studio and OberVave's simulator compile the firmware's C to WASM; zp12's editor previews through the same C-derived code. ETCH's simulator is a JS re-implementation (not firmware code).
- **Serial console:** the optional USB CDC console accepts commands (`status`, `uboot yes`; fm1-chord adds `hc key K 0|1` and `hc enc ROLE STEPS` to play a real unit from a script).

### 7.13 Screen, graphics and LEDs

- **No frame buffer.** Draw strips (240 x 24-72) into a small canvas and DMA them. Felucca-family firmwares draw **only what changed**; fast ones diff in tiles (CoralFlower: an indexed 240x240 framebuffer in the pool, 16x16 tile hashing, only changed tiles sent; PurpleMonkey: 16 px tiles, at most 84 of 225 per frame; MAKU: per-quadrant offscreen with a signature hash; MC-1: 8-row strips).
- **Speed:** SPI baud = 60 MHz / (BAUD + 1). Dinghy and Felucca use BAUD 4 (12 MHz): a full screen takes 85 ms (77 ms by arithmetic). **20 MHz is shipping (ChoralRoot), and 60 MHz BAUD (about 44 MHz effective, about 21 ms per frame) is shipping in AMB-1.** Measured: 85/70/54/39 ms at 12/15/20/30 MHz. The panel has no reset or tearing line, so a 72-row strip written at 12 MHz tears. The panel datasheet limit is quoted as 16 MHz by one project and about 62 MHz by another; both ran on real units. **[source]**
- **Our own approach** (CYBERDECK scenes and TripDeck): a 120 x 120 field drawn 2 x 2 in eight 30-row strips, one strip per main-loop pass, a live palette, a plate drawn off-device (EffectCraft) and baked as 8-bit data (see Appendix F).
- **LEDs are white (REC red, PLAY orange/green):** creative uses so far are key-follow (FiMba lights ringing tines), "light the next key" for lessons (Rainbow, MIN-1), scale roots glowing (CTL-1), breathing layers (MAKU). **LED current couples into the audio output** (MC-1 pops on PLAY/STOP; JIANT noticed its idle glow LEDs taking turns).
- **Colour on the real panel is unresolved** (ChoralRoot #6: "beautiful in the emulator, not on the screen"). Test on a unit.
- **UI rules worth copying** (MAKU's `docs/UI_RULES.md`, "for building another FM-1 firmware from MAKU"): the screen is a map of the physical knob layout; no menus; at most three parameters per mode; colour only through palette tokens with a contrast check; fixed LED semantics (lit = on, breathing = layer open, dim glow = pressable); never force a redraw on a knob turn (flicker).

### 7.14 Playing: how people map 27 keys (16 white, 11 black)

- **Omnichord strum plate** (FoMni: 16 white keys strum, black keys choose chords, ENV/LFO as modifiers; fm1-chord; Rainbow STRUM).
- **Chord block plus roots** (ChoralRoot: chord types on the left, roots D4-G5; CoralFlower: Top and Side layouts and 27 scales with Lock).
- **One key is a chord of the key** (MIN-1 EASY; CTL-1: black keys are the triad of each degree; FiMba: thumb-roll chords).
- **Scale-locked by construction** (MAKU degrees; PurpleMonkey's pentatonic where every note fits every chord; FuMi's honsu and scales).
- **Instrument-faithful geometry** (FiMba's 17-tine kalimba layout; FuMi's ST-50 rows with measured just tuning).
- **Modifier buttons while holding a key** (FoMni: ENV is minor, LFO is 7th, both m7); sprung pitch bend on OCT-/OCT+ (FuMi); MASTER as live expression (FuMi).
- **Accessibility and children:** Rainbow (picture-per-mode, a volume cap, lessons that light the keys), PurpleMonkey (a one-year-old's instrument; update mode needs three buttons so a child cannot trigger it), the screen-reader protocol in three firmwares.

### 7.15 Testing (the culture has converged)

Golden render hashes of every preset (`tests/golden.txt`, Jangada, DUELL, Felucca); host CPU baselines with a tolerance; a static loop-cost scan of the disassembly; **an LLVM-IR scan that fails the build on forbidden operations** (Tonitor `tests/check_target_ir.sh`); a script that fails the build when a soft-double symbol appears (X0X, AMB-1, FuMi); a **platform-integrity guard** diffing protected paths against a pinned upstream commit (FuMi `tools/guard_platform.sh`); golden screen renders of every screen in every palette with clipped-text checks (Felucca); soak and fuzz tests (Mycelium 20,000 random UI inputs, ORBIT 10-minute soak, Felucca fuzzers on storage); spectral tests against a reference (FM-106 versus a plugin, Vector37 musical metrics); on-device meters; hardware-in-the-loop scripts that play notes and capture the unit's own USB audio (fm1-chord `probe_device.py`, Frodhes `hil_scan.py`); a standard "scenario script" language per project (`tap`, `spin`, `key`, `expect`, `shot`). **Hardware validation still lags**: several firmwares say plainly that parts have not been run on a unit. Copy that honesty.

### 7.16 Distribution

Per-repo GitHub Pages hosting an emulator and a Web MIDI installer (12 of the 15 chord/controller repos); a "return to official V15" button; backup and restore on install; a web device manager (ChoralRoot); SoundFont attached to releases (FuMi); a release workflow that publishes when `VERSION` changes (FiMba); a public build and a private build with a child's name from a repo secret (Rainbow); port kits for the SMK-37.

### 7.17 Working with AI agents in this scene

What the successful repos do (the pattern repeats):

- Keep a **`CLAUDE.md` or `AGENTS.md`** that says what is frozen (Frodhes: "don't edit platform files"), what must be tested on hardware, who flashes (the human), and the hardware facts. FuMi's `00-START-HERE.md` is "the current truth" and every other note defers to it.
- Write **hand-off documents** with numbered bugs and gotchas (Rainbow: mono presets swallow chords, vibrato scale, loudness by ear not peak; PurpleMonkey's brief).
- Keep a **design log with the human's words** (ChoralRoot's HISTORY/HANDOFF, OberVave's CLAUDE.md).
- Use a **multi-review gate** for risky changes (Frodhes `docs/model/reviews/`), per-feature specs and task plans (Polly, GHOULBOX `docs/superpowers`), and a **platform guard** so an agent cannot drift the HAL.
- Put **evidence labels and dates** on documentation, because the scene's docs go stale in days (section 8).
- Felucca accepts AI-assisted PRs that are small, understood, tested on a real unit, with the tests green. Its author asks that the human owns the design and the responsibility.

---

## 8. Things older docs and assumptions got wrong (and what we did not expect)

The scene's documentation, including ours, is stale in places within days. Treat this table as the "do not repeat these" list.

| Older belief (where it appears) | What the evidence says | Label |
|---|---|---|
| "No float, no double, there is no FPU" (our `RULES.md` and `CLAUDE.md`, Dinghy, foma, Tonitor's IR test) | A single-precision FPU exists and ships in X0X, Melodee, AMB-1, CTL-1, FiMba, Tuna, Bubba and others. The rule is true of Dinghy's compiler flags only. We compiled a test: soft-float calls without the flags, inline code with them. Doubles and 64-bit divide still fail. | **[checked]** |
| "The chip is single core" (several READMEs) | Two cores; shipping in at least eleven firmwares; unit-dependent hazard (7.4). | **[source]** |
| "The system clock is 240 MHz" (a research repo, second-hand) | 360 MHz, measured by three projects. | **[source]** |
| "Audio runs at 44.1 kHz" | 44,117.6 Hz (24 MHz / 544). | **[source]** |
| "The audio buffer is 256 frames" (Dinghy) | Felucca 1.x, WaveLoop, Melodee, DUELL and zp12's ISR use 128 frames; SLOOP-line and Dinghy use 256. | **[source]** |
| "96 KB RAM, 336 KB pool" (Dinghy) | The linker's choice; Felucca 1.5 moved to 72/360; PIGEON re-split it; SDK builds see about 511 KB. | **[source]** |
| "The keys have no velocity or aftertouch" (FiMba, fm1-chord notes, many READMEs) | Velocity and aftertouch via ADC row sensing; shipped in CTL-1 and ChoralRoot (7.5). | **[source]** |
| "SysEx never reaches the app" (Dinghy API) | It is the platform's by default; the app can receive frames through the `ota_frame` hooks, and editors, sample uploaders, mirrors and readers all do. | **[checked]** hooks in `ota.c`; **[source]** the uses |
| "USB audio is output only" (Dinghy API, ChoralRoot manual, zp12 notes) | Host audio **into** the FM-1 exists in JIANT's test builds and in M-VAVE's SDK (`fm1_usb_audio_read`). | **[source]** |
| "Felucca's author is active; build on Felucca" | Maintenance mode since 2026-10-11; newcomers are pointed to `felucca-bsp`, which cannot be built standalone yet. | **[checked]** discussion #230 dates |
| "The vendor has no open firmware" | M-VAVE published `fm1_open` on 2026-10-13: GPLv3 app with a linking exception plus a proprietary binary engine, Apache-2.0 boot files and the stock V15 image. A community list said it was taken down; it was live on 2026-10-16. | **[checked]** repo exists, pushed 2026-10-16 |
| "Bluetooth does not work in forks" (Rainbow hand-off) | Works in Hortator's separate build and CTL-1's private build; costs and licence in 7.9. | **[source]** |
| "Identities 900, 91N, 92N, 93N, 97N, 98N, 99N are taken" (our `CLAUDE.md`) | 12 firmwares report 900; three of our own (950, 940, 920) collide with hub firmwares; 960 and 908 are free. The list is out of date and there is no registry. | **[checked]** with the live catalogue |
| "The hub tests firmwares" (user assumption) | It checks the identity, the loader and the hash only. | **[source]** hub KB page |
| "Everything boots in the QEMU emulator the hub recommends" | The hub's KB says AMB-1, TRIBE, Bubba, DUELL and FM-106 do not boot there; the other main emulator's table lists them as running. Neither is a profiler. | **[source]** |
| "`felucca-bsp` says the second core never ran" | Four-plus firmwares ship it. | **[source]** |
| "GammaWave draws an FFT spectrogram" | There is no FFT in the source; the bars come from sample differences and the filter curve. | **[source]** |

**More things we had not considered** (from the survey, not corrections):

- **A flash write is also a sound problem.** Every sector erase freezes the interrupts for tens of milliseconds. Designs that avoid it (erase ahead, manual save, RAM-only loops) were reached independently by five firmwares.
- **Units differ.** Flash part, core-1 RAM reads, clock tolerance, RF calibration and the MIDI port name vary. A claim of "works" means "works on the author's unit". Say which.
- **There are other devices on the same chip.** The SMK-37 family has 15 ports through a porting kit, the VIBE FM Mk1 is announced with "officially supported open firmware". A board layer and a compile-time board flag with a byte-identical baseline for the FM-1 is the proven way to be portable.
- **The USB identity is shared.** Every Felucca-family firmware uses the same test vendor/product ID (`1209:0001`), so Windows and macOS keep the port name of the first firmware they saw; installers must find the unit by more than its name.
- **Some "voices" are really DSP time.** Quote a CPU number for a stated load, not a voice count.
- **Credits and licences are part of the product.** GPL-3.0-only lineage, MPL-2.0 and MIT islands (felucca-bsp, the engines submodule), Apache-2.0 boot files and binary radio libraries, a vendor engine with a "devices only" grant, and unverified sample rights in at least one content pack.

---

## 9. What has not been done, and what is under-explored

Ranked within each group by how much it would change what a new firmware can do. "Nobody" means nobody in the roughly 80 repositories surveyed on 2026-10-16; the scene is two weeks old, so check again before you start. **Difficulty and risk are our judgement [guess].**

### 9.1 Hardware nobody has used (or has used only once)

| # | Opportunity | What is known | Who asked / evidence | Difficulty / risk | First experiment |
|---|---|---|---|---|---|
| 1 | **Host audio into the FM-1 (USB audio OUT endpoint)** as a real feature: effects processor, vocoder, sidechain from a DAW, resampling a phone's audio into the sampler, the "vocal or guitar FX" the VIBE page promises | The hardware path exists: JIANT's test builds ("IN+OUT" and "STEMS+IN", ring plus ring-fill-controlled interpolation) and M-VAVE's SDK read it. No release firmware ships it. | zp12 TODO ("sampling in over USB"), SLOOP #123/#125 | Medium. Mixing and latency work; the Android app side is the hard part (JIANT wrote a probe app). | Read JIANT's UAC code; add an input ring to a one-voice Dinghy app and loop the host audio back to the output; measure latency over USB. |
| 2 | **Live sampling** (nobody samples live from an input: there is none) | Resampling the device's own mix works (Bubba). With item 1, a host stream can fill a RAM ring (up to about 336 KB of pool) and be committed to flash while silent. | Felucca #250 "resample into a user slot"; zp12 TODO | Medium. Flash erase stalls audio: stage in RAM. | A one-bar resample of the output into a user slot, written in silent moments. |
| 3 | **Velocity and aftertouch used as instruments, not just as an option** | Solved (7.5), but only 2 of 15 chord/controller firmwares ship it; the top 7 keys have no aftertouch; polyphonic aftertouch out is partial; MPE-style per-note pressure over USB does not exist. | Felucca #270 (velocity/aftertouch as modulation), #260 (MPE) | Low to medium. A shared library would help: calibration per unit, curve, per-key state. | Port `sense.c` into a Dinghy app with a per-unit calibration screen; measure unit-to-unit variance. |
| 4 | **MPE and per-note expression out** | Not found anywhere. | Felucca #260 | Medium; needs item 3 and a channel-per-note allocator. | Send per-note pressure as channel pressure on rotating channels. |
| 5 | **Fast display** (20-60 MHz SPI) for 30-60 frame-per-second visuals | Shipping in ChoralRoot (20 MHz) and AMB-1 (60 MHz baud). No tearing line; strips tear at 12 MHz. Panel limit disputed. | none requested; CoralFlower measures 13 fps at 12 MHz | Low. Test on several units. | Raise baud in steps and watch for artefacts on the screen and the audio (SPI DMA load). |
| 6 | **Free SRAM, cache RAM and D-cache configuration** | 29.5 KB `.lowram` device-tested; a 28 KB cache-RAM window not validated; the D-cache effect (7.7) measured in one SDK build only. | none; conflicting results (DUELL and Melodee versus MC-1) | Low risk, valuable. | Move a hot DSP loop into RAM and measure the missed-deadline rate on a unit. |
| 7 | **Hardware blocks:** the EQ block (`0x1E000`), hardware LED PWM (MCPWM), the quadrature decoder, FFT, AES/SHA/CRC/RNG accelerators | Melodee has opt-in EQ and LED-PWM code ("not auditioned"); the others are only listed in the SDK. | none | Unknown | LED PWM first: it removes the LED current coupling into the audio output. |
| 8 | **Wi-Fi** | Nobody has run it; the board has a wire antenna pad; whether the RF front end for Wi-Fi is fitted is unknown; the emulator models it as inert. | none | **High**: regulatory (Felucca's author cites an unresolved Japanese radio-law question), binary libraries, RAM. | Do not start here. Ask first. |
| 9 | **Power:** battery meter, low-power modes, sleep, auto-off | The battery divider is read (`PB1`, threshold values from stock); nobody measured draw or implemented sleep. [not found] | none | Medium | Log battery volts against play time on a unit. |
| 10 | **Clock above 360 MHz; larger flash by chip swap** | A boot-time retune to 320 MHz bricked units; a flash swap is a board mod (`felucca-bsp` tests it on a host only). | none | High | Do not retune. |
| 11 | **Encoders with push** | The encoders have no switches; the quadrature decoder block is unused. | complaints in reviews | Hardware limit | Use button chords. |

### 9.2 Musical and software areas that are thin

| # | Opportunity | Evidence | First step |
|---|---|---|---|
| 12 | **Wavetable and multi-model oscillators** (Plaits/Braids-style), **vocoder**, **granular beyond a few engines**, **modal/physical models beyond a handful** | Only DUELL (29 tables), ETCH (drawn), a Melodee PR, Tonitor, Jangada, Frodhes and FiMba touch these; no vocoder; no multi-model oscillator. | A single-oscillator Plaits-like model set in float with a CPU budget table. |
| 13 | **On-device machine learning** | We found none among the surveyed repos except Tuna's corpus-tuned generators (training happens off-device) **[guess: limited to what we read]**. Our MLdeck runs a trained decoder on the device (see Appendix F). | Write up and publish MLdeck's pattern (a tiny integer decoder at control rate, DSP does the audio). |
| 14 | **Reverb and chorus with measured on-device costs**, and shared implementations | Five reverbs and two chorus designs exist, with host-instruction costs only; "awful reverb" is a recurring complaint (Felucca #176). | Benchmark GHOULBOX's Dattorro, CS-1's FDN and Tonitor's plate on one unit with one load and publish the numbers. |
| 15 | **Sequencer paradigms:** event-list sequences instead of fixed 64-step arrays, per-step conditions and probability everywhere, polymeter, tracker, live-coded patterns | Felucca refuses 128 steps for memory (#214); sloopDX has 128 and a shared pool; Jangada has Euclid and cellular automata; BEETLE is a tracker; per-track MIDI channels were added in Felucca 1.6; users ask for 8 tracks (Bubba #1, ORBIT #2). | A packed event-list sequencer (zp12's 32-bit event word is a start). |
| 16 | **A common patch/project format and an owner tag** | Each firmware has its own format; only Felucca-lineage ones read each other; DX7 `.syx` is the only shared patch format; the SMK-37 kit proposes a 16-byte owner tag; nobody lists what a firmware leaves behind. | Publish a one-page "FM-1 data etiquette" (magic, owner tag, version, CRC, ranges) and a tool that lists the foreign records in flash. |
| 17 | **Standard CC map, MIDI learn, MPE, USB-host MIDI** | Felucca has MIDI learn; ORBIT has 16 learn slots; maps are ad hoc; FOMAJAM's fixed map is the most complete; nobody has MPE; USB host needs an external box (`fm1buddy`). | Adopt FOMAJAM's `CC.md` as a starting standard. |
| 18 | **Firmware choosers and loadable engines** | A power-up chooser was refused (flash full); the Felucca 2.0 plugin experiment ran engines from flash through a function pointer on a real unit and was shelved ("most engine code is 1-4 KB; samples cost more flash; RAM is the problem"). | Revive the experiment with a 3 KB engine and a relocation table. |
| 19 | **Flash is the binding limit; compression** | Images sit near 566-573 KB of 581 KB. ADPCM, mu-law, range-coded banks and RLE art exist but no shared decoder library. | A small shared decompression layer (LZ4 exists in the loader; ADPCM in FOMAJAM/X0X). |
| 20 | **Instruments as controllers and remote panels** | HYP-1 and PIGEON turn the panel into a controller for a browser instrument; Polly mirrors screen and panel; CTL-1 is a controller with sounds. No DAW control-surface protocols (Mackie/HUI), no OSC. | A Mackie/HUI-compatible fader bank over USB-MIDI. |
| 21 | **Education, accessibility and utility** | Screen reader (3 firmwares), kids' toys (2), a course and an ear trainer (2); thin otherwise: no tuner-first, metronome-first or theory-trainer firmware; high-contrast and large-type only in Tonitor. | An ear-training firmware using the velocity keys. |
| 22 | **Visuals as part of the instrument** | Scenes (TripDeck, CYBERDECK), a plant that grows with your chords, "living beings" (JIANT), animated pets; LEDs only follow notes. | Use the LED matrix as a data display (bar graphs, a spectrum from the audio tap). |
| 23 | **Content:** preset libraries, DX7 banks, sample packs with clean licences, SoundFont export | SLOOP DX7 Banks, Mycelium (rights unverified), FuMi's SoundFont, BYO-ROM (Polly); our own preset research note exists. | A provenance-first content pack format (licence and source per sample). |

### 9.3 Tooling and scene gaps

| # | Gap | Evidence |
|---|---|---|
| 24 | **No identity registry** (collisions exist and grow) | 12 firmwares on `FM-1_900`; our 950, 940, 920 collide. The script in Appendix E checks the live catalogue. |
| 25 | **No device-measured CPU profiler** and no shared cost methodology | Four bespoke consoles; mostly estimates. `fm1_ticks()` (24 MHz) is enough to build one; a standard `status` over the serial console would help everyone. |
| 26 | **No hot reload on hardware** | The nearest is foma's save-to-browser loop; RAM-run through UBOOT needs the dongle and nobody runs a full app that way. |
| 27 | **Emulator fidelity and Windows support** | The two emulators disagree on four firmwares; neither models timing; Windows is unproven for both. |
| 28 | **Windows story** | Felucca's own BUILDING.md does not cover Windows; SLOOP uses WSL; M-VAVE's SDK is Windows-only; recovery tools need admin PowerShell. Our workspace runs builds in WSL and flashing on Windows Python. |
| 29 | **Safe update paths** | The SPL will not restart a staged loader on a cold boot, so a boot stub plus verified write order is the real fix (ChoralRoot `BOOT-SAFETY.md`); few firmwares have it. |
| 30 | **Cross-device portability** | The SMK-37 kit is a template: board layer as commits, a byte-identical baseline, an agent playbook. Nobody has done a third device. |
| 31 | **Toolchain licence** | No redistribution grant; Docker images that download it exist; unresolved. |
| 32 | **Radio libraries and the GPL** | CTL-1 refuses to ship BLE; Hortator ships a separate build; unresolved. |
| 33 | **The vendor's SDK** | `fm1_open` is live and unexamined by the community; how its binary engine combines with GPL lineages is unclear. |

### 9.4 Where our own projects are different (and where they have not caught up)

- **Unusual in this scene:** MLdeck's on-device learned decoders (timbre, saturation curve, resonator banks) trained on the PC; DoppleDecker's idea of fitting an FM patch against the real integer engine with a PC tool; TripDeck's and CYBERDECK's baked-art-plus-live-palette scenes.
- **Not yet done by us:** FPU flags, a second core, audio code in SRAM, a boot stub, velocity, an owner tag, a device-measured CPU number, a real-unit validation of the new scenes. See Appendix F.

---

## 10. Starting a new project: kickoff checklist

### 10.1 Ask the human first

1. **Which device(s)** do you have (FM-1; SMK-37 Pro/Elite/MKE-P37?) and is a **recovery path** ready: the stock V15 `.fwsc`, a verified flash backup, and ideally a built Transporter dongle?
2. **What kind of instrument** is it, and what is the one thing it does that none of the 52 firmwares do (Appendix A, section 9)?
3. **Who flashes?** (Default: the human, by hand; the agent never flashes without being asked.)
4. **Which base?** (Section 4.) Default: Dinghy via the workspace's `fm1.ps1 new <name>`.
5. **Float, second core, BLE?** Each is a decision with hazards; default to none for the first release.
6. **Licence and credits:** GPL-3.0-only; keep the Felucca/Hügelton, SLOOP/isod89 and Dinghy/zvenson credits for anything built on their platform; list third-party parts with their licences; document the source of every sample.
7. **Publishing?** (A public GitHub repo, a `.fwsc` on a release, the hub listing.) Check the repo and its history for personal information before pushing.

### 10.2 Pick an identity

Run the checker in Appendix E against the live catalogue. Prefer a unique seven-digit identity (two-digit project prefix, major, two-digit minor, two-digit patch; the hub accepts `FM-1_` plus 3-8 digits) over a crowded three-digit one, so that "resume an interrupted update" and the hub's "on your FM-1" mark work. Never use `FM-1_015`.

### 10.3 Milestones (each ends with something you can check)

| M | Goal | Done when |
|---|---|---|
| **M0** | **Verify the toolchain and the recovery path** with no code changes | `clang` runs; the pinned SDK files match their SHA-256; you can name the stock V15 file and its hash; the human has a recovery plan (2.3). Optional: a float compile test (3.3). |
| **M1** | **Host build runs.** Your app compiles for the PC against a fake platform and writes a WAV and PNG screens from a script | `fm1.ps1 host <name> tests/<script>.txt` produces sound and screenshots. |
| **M2** | **Unmodified first flash** of the starter firmware, with a round trip back to V15 | The **human** flashes; the unit runs; you read its identity with `--info`; the human confirms the way back works. |
| **M3** | Your instrument in host mode: sound, UI, scripted tests, a golden render | Tests pass; the build's checks pass (size, no ROM addresses, no register access outside `hal/`, no calls in `.ram_text`). |
| **M4** | **Budget it**: a static cost scan and an on-device CPU meter | A CPU number measured on a unit with a stated worst-case load. |
| **M5** | **Persistence and safety**: records with magic/length/CRC/owner tag, saves only when silent, a boot guard | Pull the power at random moments in a soak test; the unit always comes back. |
| **M6** | Flash your build (human) and play it | The README states exactly what was tested on a real unit and what was not. |
| **M7** | Optional hardware features, one at a time (float, second core, velocity, faster LCD, BLE) behind a build flag with a byte-identical baseline | Each has a measured benefit and a one-line fallback. |
| **M8** | Release: tag, `.fwsc` on a GitHub release, source, credits, licences, a browser emulator if you can, hub listing | `fm1_install.py --info` reports your identity after install; the hub's rules are met. |

### 10.4 Do and do not (for the agent)

**Do:** read the repo's own `CLAUDE.md`, `RULES.md`, `API.md` first; run the host build before every device build; check the build output for the final "app" line; keep platform files frozen and diff them against a pinned upstream; label what you know ([checked], [source], [claim], [guess]) in your own notes; write down the hardware facts you measure; keep a hand-off document for the next session.

**Do not:** flash without being asked; widen the flash write guard without checking section 3.4; touch `0x0000-0x3FFF`, `0xFF000` or (SMK-37) `0xC1000`; retune the PLL; use `double` or 64-bit division; call `fm1_adc_read` from the main loop if you use the key-sensing ADC; save in a loop or while a voice sounds; reuse another firmware's identity, record magic or SysEx command numbers without negotiation; put names, e-mail addresses, machine paths or local timestamps with a time zone into a public repo.

### 10.5 A brief to paste into the new project's `CLAUDE.md`

```markdown
# <NAME>: FM-1 firmware (identity FM-1_<ID>)

Read docs/FM-1-MOD-GUIDE.md sections 1-3 first. Evidence labels: [checked] [source] [claim] [guess].

## Hard rules
- Never flash a device unless the user asks in this conversation. The user flashes by hand by default.
- The first flash is the unmodified starter, with a proven round trip to the stock V15 file.
- Never write flash 0x00000-0x03FFF or 0xFF000. Keep the update path (FELUCCA_OTA) on. Do not retune the PLL.
- No `double`, no 64-bit `/` or `%`. float only with: -mcpu=r3 -mfprev1 -ffp-contract=off, own libm, fail the build on soft-double.
- app_audio() is an interrupt: <2.9 ms per 256 frames target (128-frame bases: <1.5 ms). No flash, screen, platform_poll() or waiting.
- Save only when silent. Records: magic + version + length + CRC + owner tag; anything else is empty.
- Platform files (hal/, loader/, usb.c, ota.c, ...) are frozen: diff against the pinned upstream before every build.

## Workflow
change code -> host test (WAV + PNG, seconds) -> build (checks) -> user flashes -> measure on the unit.
Report in the README exactly what was tested on a real unit.

## Facts (add yours with a label and a date)
- <hardware facts measured by you>

## Decisions log (with the user's words)
- <date>: <decision>
```

### 10.6 Where to look when you are stuck

| Question | Look at |
|---|---|
| How does the platform work? | Dinghy `docs/API.md`, `RULES.md`; Felucca `BUILDING.md`; `felucca-bsp` |
| Hardware facts | Section 3; `ip2k/lunar-modulator/docs/01-hardware.md`; `fm1-to-smk37/docs/01-hardware.md`; HAL header comments |
| Float / DSP in float | X0X `fastmath.h` and `tools/build.py`; Melodee `AUDIO_PERFORMANCE.md` |
| Second core | X0X `firmware/hal/fm1_cpu1.h`; SLOOP `DUALCORE.md`; Jangada `BUILDING.md` |
| Velocity | ChoralRoot `docs/VELOCITY.md`; CTL-1 `firmware/src/app/sense.c` |
| Flash safety | ChoralRoot `docs/BOOT-SAFETY.md`; Felucca `src/storage.c`; zp12 `sp_store.c` |
| Samples and uploads | zp12 `sp_link.c`, `web/zp12link.js`; Polly `bank_rx.c`; PIGEON `docs/KO_STORAGE.md` |
| A browser simulator of your firmware | zp12 `sim/`; OberVave `docs/sim/`; Jangada `web/studio/`; foma |
| UI rules | MAKU `docs/UI_RULES.md`; CoralFlower `docs/SPEC.md`; Rainbow hand-off |
| Recovery | MvaveFM1Unbricker `TRANSPORTER-GUIDE.md`; `FM-1-transporter`; hub KB |
| Everything else | `cicloid/awesome-fm-1` |

---

## Appendix A: All 52 hub firmwares (catalogue of 2026-10-16, sorted by installs)

Facts (identity, version, installs, stars, kind) come from the live catalogue **[checked]**. The base and the last column come from the research agents' reading of each repository **[source]**. Installs and stars change daily.

| Firmware | Author | Base | Kind | Version | Installs | Stars | Identity | What it shows |
|---|---|---|---|---|---|---|---|---|
| SLOOP (`sloop`) | isod89 | Felucca fork (squashed history) | groove | 2.7 | 1707 | 507 | `FM-1_900` | App-level USB rescue, drum synth, song/arranger, .lowram, dual core 2.7; most installed; merges no PRs. |
| Felucca (`felucca`) | Leo Kuroshita (Hügelton Instruments) | the base | groove, dev | 1.6.1 | 727 | 500 | `FM-1_916` | One compilation unit, HAL, loader, A/B storage, SysEx editor protocol, huge test suite; maintenance mode since 2026-10-11; 2nd core beta in 1.6. |
| JIANT FM (`jiant`) | juanjiant-bit | Felucca 1.1.5 + SLOOP songs | synth, groove | 0.7.4-de2a4cb | 606 | 125 | `FM-1_900` | Dual core; USB audio IN+OUT and 8/10-channel stems (test build); 'living beings' visuals; about 78k lines. |
| X0X (`x0x`) | Charles Vestal | X0X line (own platform lineage) | groove | 1.1.0 | 477 | 169 | `FM-1_9010100` | Reference for float (fastmath.h), dual core with bit-identical output, load guard, event-sliced sequencer, scenario tests. |
| GHOULBOX (`ghoulbox`) | Jason Persinger | Felucca 1.4.1 | synth, groove | 2.1 | 414 | 48 | `FM-1_900` | Dattorro reverb in the delay buffer (zero extra RAM); per-track temperaments; browser emulator. |
| ChoralRoot (`choralroot`) | Quixotic7 | Felucca fork | chords | 0.24-beta1 | 251 | 58 | `FM-1_920` | Velocity and aftertouch from the keys; dual core with single-core fallback package; boot stub; web device manager; 6-channel USB audio. |
| AMB-1 (`amb`) | Charles Vestal | X0X/FoMni line | synth | 1.2 | 239 | 54 | `FM-1_7010200` | Generative ambient; float plate reverb (~139 KB) plus tape loop; 60 MHz LCD baud; screen-reader protocol. |
| sloopDX (`sloopdx`) | zvenson | SLOOP | groove, synth | 4.0 | 196 | 23 | `FM-1_940` | DX7 (Dexed) engine, DX7 SysEx import, 128 steps; 3-way merges with SLOOP. |
| FoMni (`fomni`) | Charles Vestal | X0X line | chords | 0.4 | 192 | 60 | `FM-1_8000400` | Omnichord strum plate; plat.h hardware interface + host simulator + scenario scripts; screen reader; TRS in. |
| Melodee (`melodee`) | keremimo | rewrite of Felucca's design | synth, groove | 1.4.1 | 159 | 94 | `FM-1_91041` | FM6/CZ/Prophet float engines; 70 scales; audio graph in SRAM; weighted voice budget; clock-retune failure lesson. |
| OberVave (`obervave`) | zvenson | Dinghy | synth | 1.4.2 | 127 | 13 | `FM-1_954` | Polyphonic analog-style synth; browser simulator via zig cc + wasm-opt asyncify; CLAUDE.md design log; SMK-37 port. |
| Skiff (`skiff`) | illuminus5 | Felucca 1.1.5 | groove, synth | 1.2 | 123 | 10 | `FM-1_712` | Six tracks, fourteen engines; 75k lines. |
| zp12 (`zp12`) | zvenson | SLOOP 2.3 | drums | 2.4.2 | 120 | 30 | `FM-1_974` | Browser sampling to SysEx to flash; 12-bit in-place playback; sequencer in the ISR; A/B 40 KiB records; WASM sim. |
| FiMba-1 (`fimba`) | jadamsowers | FoMni | synth, chords | 0.6.1 | 114 | 19 | `FM-1_8000601` | Kalimba physical model in float; built openly with Claude; scenario DSL. |
| Fart Machine One (`fart`) | adamcharleswilsonmusic-code | FoMni | other | 1.0.0 | 108 | 16 | `FM-1_701` | 11 samples on 27 keys; provenance documents; no web installer. |
| Rainbow Mode (`rainbow`) | joshbartlettband-web | Felucca 1.1.5 | other | 1.1.5-rb | 107 | 15 | `FM-1_911` | Children's instrument; hand-off doc with 11 numbered bugs; K-weighted loudness levelling; public and private builds. |
| FM-106 (`fm106`) | PatttF | Dinghy | synth | Beta4 | 107 | 8 | `FM-1_907` | Juno-106 emulation (MN3009 BBD chorus); 6 voices across both cores. |
| Hortator (`hortator`) | DEADACTIVE | Felucca drum fork | drums | 0.16.0 | 98 | 18 | `FM-1_900` | Eight-track drum machine; BLE-MIDI in a separate build; BENCH console. |
| MAKU (`maku`) | okumurari | Felucca fork | groove, synth | 1.0 | 93 | 21 | `FM-1_910` | Ambience machine; UI rules for the next firmware; erase-ahead storage; AGENTS.md shared brain. |
| SLOOP DX7 Banks (`sloopbanks`) | majnikool | SLOOP | groove, synth | 2.5-b28-test | 86 | 2 | `FM-1_925` | Twelve DX7 banks in SLOOP (flash 0xEF000-0xFAFFF). |
| fm1-chord (`chord`) | math0ne | Felucca 1.0.1 | chords | 0.3 | 86 | 10 | `FM-1_903` | HiChord-style machine; probe_device.py (play notes in, measure USB audio); spec trail; hardware findings doc. |
| Tonitor (`tonitor`) | Nuno Zimas | Felucca/SLOOP platform | synth, other | 2026.10.16 | 74 | 8 | `FM-1_700` | LLVM-IR forbidden-op scan; low-vision UI; half-rate voices; plate reverb. |
| FuMi-1 (`fumi`) | cartesive | FoMni | synth, chords | 1.0.7 | 72 | 10 | `FM-1_7010007` | Platform-integrity guard script; measured tunings; SoundFont export; audition bench. |
| WaveLoop FM-1 (`waveloop`) | ELI7VH | Felucca 1.0.5.2 | synth, groove | 1.0.5.2-midi23 | 65 | 7 | `FM-1_910` | Felucca played from an Arturia MiniLab 3; diagnostics command. |
| SLOOP Floyd FM (`floyd`) | René Bohne | SLOOP 2.4 | synth | 2.4 | 64 | 2 | `FM-1_924` | Visual four-operator FM on SLOOP. |
| Bubba Machine (`bubba`) | Erbubar23 | JieLi SDK (M-VAVE loader) | groove, drums, synth | 0.7.0-beta | 62 | 7 | `FM-1_003` | From scratch on the SDK; float; dual core via IRQ; ~511 KiB RAM; CPU lab; records its own mix on device. |
| Polly (`polly`) | Charles Vestal | FoMni + Sonivox EAS | groove, chords | 0.4.3 | 49 | 14 | `FM-1_3000403` | Arranger; bring-your-own-ROM QY70 bank in 254 KB of flash; SysEx screen/panel mirror; undo journal. |
| ORBIT (`orbit`) | dspaudio | SLOOP | groove, synth | 0.7.1 | 47 | 20 | `FM-1_900` | OP-1-style four tracks; 44.1/48 kHz USB audio via FIR; RAM at 99 %. |
| Felucca [Salt] (`salt`) | Chance Roth (ChanceTheMaker) | Felucca 0.9 import | synth, groove | 0.9-salt14 | 45 | 6 | `FM-1_909` | Browser Studio with the C DSP in WASM; pinned Felucca import. |
| Jangada (`jangada`) | zednaked | Felucca + SLOOP parts | synth, groove | 1.0 | 45 | 29 | `FM-1_910` | Dual core AUTO/OFF/BOOST with the supply-voltage fix; Studio compiled from firmware C with a bit-parity test. |
| DUELL (`duell`) | OSCILL/R | Felucca snapshot | synth | 0.2.20-beta | 42 | 11 | `FM-1_901` | 128-frame budget; CPU safeguards; hot functions moved to RAM. |
| winampFM Synth (`winampfm`) | Hericdk | SLOOP 2.6 copy | groove, synth | 0.12 | 41 | 7 | `FM-1_900` | Winamp-style UI; 3D WASM simulator of the whole firmware. |
| TRIBE (`tribe`) | apiel | X0X | groove, drums | 0.1-beta | 35 | 8 | `FM-1_9000100` | Ten tracks, any engine on any track; log-structured flash. |
| HYP-1 (`hyp1`) | RedtailWorks | AMB-1 (X0X family) | groove | 1.5.5 | 32 | 11 | `FM-1_7010505` | Float hypnotic techno/acid; the panel as a remote for a browser instrument. |
| NOIZ DEMONS (`noizdemons`) | SPIKE the Percussionist | Felucca fork | synth | 666 | 32 | 1 | `FM-1_900` | Felucca-based noise synth. |
| CS-1 (`cs1`) | twochannels | Dinghy | synth | 2026.10.15 | 31 | 3 | `FM-1_950` | 8-line FDN reverb with octave shimmer; tape delay; dual core. |
| CoralFlower (`coralflower`) | FicusFicusov | ChoralRoot v0.21 | chords | 0.2 | 28 | 0 | `FM-1_930` | Indexed framebuffer with tile hashing; designer mock-up spec; plant visual. |
| MYCELIUM (`mycelium`) | Mandalars | SLOOP content pack | groove | 1.0-test | 25 | 2 | `FM-1_900` | Content pack (drum samples with unverified rights). |
| ETCH (`etch`) | SikkenWorks | Felucca HAL, own OTA | synth | 1.38 | 21 | 7 | `FM-1_1038` | Drawn wavetables; two 64-bit divides in the audio path crashed it (fixed in V1.36); reflash lock-in reports. |
| Felucca 8 Patterns (`felucca8`) | echoe | patch on Felucca 1.5.1 | synth, groove | 1.5.1_and_sequences | 19 | 4 | `FM-1_900` | Eight sequences per track by dropping user samples. |
| Dinghy (`dinghy`) | zvenson | template | dev | 0.3 | 10 | 33 | `FM-1_993` | Documented minimal base (sine); docs stale on float and identities. |
| PIGEON (`pigeon`) | The Shrimp Blimp | Felucca | drums, groove | 0.3 | 10 | 3 | `FM-1_900` | MP3 decoded in the main loop (libmad); re-split RAM/pool; 496 KiB bank. |
| CTL-1 (`ctl`) | Charles Vestal | FoMni + Sonivox EAS | chords, other | 0.9.2 | 8 | 8 | `FM-1_6000902` | MIDI controller with sounds; velocity sensing; a private BLE build. |
| Vector37 (`vector37`) | genkigenki | SLOOP board layer | synth | 0.2.2 | - | 5 | `FM-1_700` | Mip-mapped band-limited tables; its repo builds only the SMK-37 version. |
| MC-1 (`mc1`) | dspaudio | ORBIT HAL + own core | groove | 1.0.1 | - | 3 | `FM-1_900` | Own core on ORBIT's HAL; device CPU counters; baked instrument excerpts. |
| BEETLE (`beetle`) | Joppe Sikken (SikkenWorks) | ETCH HAL | groove | 1.01d_RC | - | 0 | `FM-1_2101` | Tracker with six lanes; provenance audit; flash probe BLANK/EXISTING/FOREIGN. |
| MIN-1 (`min1`) | Reytian | AMB-1 | chords, other | 0.2.1 | - | 2 | `FM-1_5000201` | Minimalist ensemble; estimates CPU from retired instructions; identity scheme. |
| PurpleMonkey (`purplemonkey`) | Quixotic7 | ChoralRoot tree | other | 0.11 | - | 3 | `FM-1_927` | Toddler instrument; LPC speech (33 words, 6.7 KB); tile-budgeted animation. |
| Pocket GammaWave (`gammawave`) | ShadowTune (GammaWave Audio) | JIANT | synth, groove | 1.0-GW | - | 0 | `FM-1_981` | Genre generator; the 'FFT' spectrogram has no FFT. |
| Tuna (`tuna`) | delirioux | Felucca + engines submodule | other | 1.0-b77b159 | - | 0 | `FM-1_900` | Marbles port in the audio ISR at a quarter rate; own libm; tuned on 3,486 public-domain scores. |
| Frodhes (`frodhes`) | Leonardo Mirabal | Dinghy | synth | 0.9.0 | - | 0 | `FM-1_958` | Multi-review gate; hardware-in-the-loop scans; CLAUDE.md. |
| FOMAJAM (`fomajam`) | konsumer | foma template | drums, groove | 0.1.2 | - | 0 | `FM-1_961` | foma template; fixed CC map; ADPCM drum ROM within a flash budget; companion fm1buddy box. |

---

## Appendix B: The sources to read first (all public)

**Platforms and templates:** `hugelton/Felucca` (README, `BUILDING.md`, `CONTRIBUTING.md`, wiki, discussions #230 status, #166 AI disclosure, #159 plugin experiment, #270 velocity), `hugelton/Felucca-Engines` (MIT), `hugelton/felucca-bsp` (MPL-2.0, in progress), `isod89/sloop-fm1` (`DUALCORE.md`, `RAM.md`), `zvenson/dinghy` (`docs/API.md`, `RULES.md`, `MAKE-IT-YOURS.md`), `konsumer/foma` and `konsumer/fm1buddy`, `charlesvestal/fm1-x0x` (build flags, `fastmath.h`, `hal/fm1_cpu1.h`, scenario tests), `genkigenki/fm1-to-smk37`, `m-vave/fm1_open` (the vendor's SDK).

**Hardware and reverse engineering:** `ip2k/lunar-modulator` (`docs/01-hardware.md`), `AL-255/FM-1-RE`, `kagaimiq/jielie`, `kagaimiq/jl-uboot-tool`, `kagaimiq/ghidra-jieli`, `Keitark/fm1-nes` (`GETTING_STARTED.md`), `cicloid/awesome-fm-1` (the index), the JieLi SDK on Gitee (`Jieli-Tech/fw-AC79_AIoT_SDK`, register map `WL82.h`).

**Recovery:** `Quixotic7/MvaveFM1Unbricker` (`TRANSPORTER-GUIDE.md`), `kurogedelic/FM-1-transporter`, `jvitkauskas/m-flasher`, `jvitkauskas/fm1-bootloader` (untested on hardware), the hub's `rescue/fm1_rescue.py`.

**Emulators:** `simonjohansson/fm1-emulator`, `joakimeriksson/fm1-emu`, `shcherbakov7/m-wave-fm1-browser-emulator`.

**The hub:** `https://thefirmwarehub.com` (the catalogue at `/catalog.json`; the knowledge base at `/kb/`; the SMK-37 page at `/smk37/`; the VIBE page at `/vibe/`).

**Firmwares that teach the most** (section 6): X0X, Melodee, Jangada, ChoralRoot, zp12, Polly, CTL-1, Hortator, Bubba Machine, JIANT, MAKU, FuMi, Tuna, GHOULBOX, CS-1.

**Community channels:** Felucca GitHub Discussions and Issues; each firmware's issues; the subreddit `r/MVaveFM1` (not read by us); KVR, Elektronauts and Gearspace threads on the FM-1; a Hacker News discussion of the stock firmware analysis; reviews on Synth Anatomy, Synthtopia and Piano & Synth.

## Appendix C: Numbers cheat sheet

| What | Value |
|---|---|
| CPU | pi32v2, two cores, **360 MHz** system clock; free-running timer 24 MHz (`FM1_TICKS_PER_US` = 24) |
| Audio | 24-bit I2S, **44,117.6 Hz**; half buffer 128 frames (2.9 ms) or 256 frames (5.8 ms) depending on base; full scale Q15 ±32767; DAC 6 dB under full scale |
| Timers | 10 kHz tick (key scan, USB poll, ms counter) at higher priority than the audio interrupt |
| App image | 581,564 B (0x8DFBC) at XIP `0x02000120` |
| RAM / pool / stacks | 96 / 336 / 24+8 KB (Dinghy, SLOOP); 72 / 360 (Felucca 1.5+); `.ram_text` 24 KB; `.lowram` 29.5 KB free |
| Flash | 1 MiB; sector 4 KiB; ~100k erases; erase stalls 20-50 ms |
| Protected | `0x0-0x3FFF`, `0xFF000`, SMK `0xC1000` |
| Screen | 240x240 RGB565; SPI baud 60 MHz/(BAUD+1); full frame 85/70/54/39 ms at 12/15/20/30 MHz |
| Matrix | 11 columns x 6 rows; 27 keys + 14 buttons; 7 encoders; ADC: MASTER `PB6` (ch 4), battery `PB1` (ch 3), key rows `PA7`/`PA8` (ch 0/1) |
| USB | full-speed device; VID:PID `1209:0001` (Felucca-family, shared); loader `1209:0002`; stock `4C4A:C755`; OTA `4D4A:4155`; mask-ROM `4C4A:8057` |
| MIDI | USB-MIDI; TRS **in** only (UART1 RX `PH8`, 31,250 baud) |
| Package | `.fwsc` about 610 KB (stock V15 699,956 B); CRC-16, no signature; chip key `0x980F`; flash id `0x856014` |
| Rescue | OCT- at power-on (updater only); soft key SysEx `F0 22 24 35 7D F7`; Transporter for no USB at all |
| Identity | `FM-1_` plus 3-8 digits, max 20 characters; `FM-1_015` is official V15 |
| Hub (2026-10-16) | 52 firmwares, 43 authors, 6,994 installs |

## Appendix D: Glossary

- **SPL / `uboot.boot`:** the second-stage boot code in the flash head; it starts your app or the update loader. Never overwritten.
- **UBOOT (mask ROM):** the chip's built-in USB download mode (a disk named `WL82 UBOOT1.00`). Cannot be erased. Entered with the USB_KEY signal (a Transporter), a SysEx soft key, or by falling in when the flash has no valid image.
- **XIP:** execute in place: flash is mapped into the address space and the code (and `const` data) runs from it through the cache.
- **`.fwsc` / UFW:** M-VAVE's update package format; **update loader / OTA:** the small program inside each package that writes the new image.
- **Identity:** the product string in the package (`FM-1_9NN...`).
- **Pool:** the large zeroed RAM region for delay lines, reverbs and sample rings.
- **HAL / `hal/`:** the only place allowed to touch hardware registers (the build enforces it).
- **Half buffer:** one half of the audio DMA double buffer; the interrupt fills one half while the other plays.
- **Plate:** (our term) a grayscale picture made off-device and baked into flash, coloured live by a palette.
- **Felucca-family:** firmwares that share Felucca's HAL, record magic (`FELU`) and update loader.
- **Transporter:** the RP2040 USB dongle that can revive a unit with no USB device at all.

## Appendix E: Check an identity against the live hub (tested 2026-10-16)

The hub answers 403 to Python's default user agent, so the script sets one. Many catalogue entries carry the catch-all pattern `FM-1_\d{3,8}`; the script ignores that. Tested: `FM-1_960` and `FM-1_908` free; `FM-1_950` (CS-1), `FM-1_940` (sloopDX) and `FM-1_920` (ChoralRoot) collide; `FM-1_900` has twelve users. A copy is in `tools/fm1_idcheck.py`.

```python
#!/usr/bin/env python3
r"""Check candidate FM-1 package identities against the live hub catalogue (no registry exists; this is the next best thing).
   python3 fm1_idcheck.py FM-1_4010000 FM-1_960
   A candidate COLLIDES when a hub firmware has exactly that product string, or declares a specific ident pattern that
   matches it. The catch-all pattern FM-1_\d{3,8} that many entries carry by default is ignored."""
import json, re, sys, urllib.request

CAT = "https://thefirmwarehub.com/catalog.json"
GENERIC = r"FM-1_\d{3,8}"
req = urllib.request.Request(CAT, headers={"User-Agent": "fm1-idcheck/1.0"})   # the hub answers 403 to the default Python agent
data = json.load(urllib.request.urlopen(req, timeout=20))
fws = data["firmwares"]
for cand in sys.argv[1:] or ["FM-1_960"]:
    hits = []
    for f in fws:
        pat = f.get("ident") or ""
        same = f.get("product") == cand
        try:
            specific = bool(pat) and pat != GENERIC and re.fullmatch(pat, cand) is not None
        except re.error:
            specific = False
        if same or specific:
            hits.append(f'{f["id"]} (product {f.get("product")})')
    shape_ok = re.fullmatch(r"FM-1_\d{3,8}", cand) is not None and cand != "FM-1_015"
    verdict = "FREE" if shape_ok and not hits else "COLLIDES" if hits else "BAD SHAPE"
    print(f"{cand}: {verdict}" + ("" if not hits else "  <- " + "; ".join(hits)))
```

## Appendix F: Our workspace (the FM-1 Projects folder)

**Workflow** (the workspace's `CLAUDE.md` has the details): `fm1.ps1 new <name>` copies pristine Dinghy into a new project with its own git repo; `fm1.ps1 host <name> tests/<script>.txt` runs the real firmware code on the PC (WAV and screenshots, seconds); `fm1.ps1 build <name>` cross-compiles in WSL (the build runs the brick-protection checks); `fm1.ps1 install <name>` flashes from Windows Python over USB-MIDI (only when the user asks); `fm1.ps1 info` reads the connected unit's identity. Git must not convert line endings (`core.autocrlf false`). Python 3.12 on Windows (3.13 has no `python-rtmidi` wheel).

**Our projects and what each demonstrates:**

| Project | What it is | Notable technique | Identity |
|---|---|---|---|
| **CYBERDECK** | Four-voice subtractive synth, arpeggiator, 32-step sequencer, mutate; v1.0 plus fixes, flashed and identified on a unit | Engine in the audio ISR, event queue from the main loop, host tests with scripts; a **neon scene for every page** built from EffectCraft plates plus live overlays (the filter curve, the envelope, the echoes, a radar, the arp ring, the pattern as a skyline) and a loading screen; a preset research note | `FM-1_960` (free) |
| **TRIPDECK** | CYBERDECK-based synth with five psychedelic scenes and sound effects | Scenes on every page, shared knobs; a 120x120 field drawn 2x2 in strips | `FM-1_908` (free) |
| **MLdeck** | Per-parameter motion plus **on-device learned decoders** (timbre, saturation curve, resonator bank) trained on the PC | Tiny integer networks at control rate; the DSP does the audio | `FM-1_920` (**collides with ChoralRoot**) |
| **GrooveDeck** | Drum machine (16 synthesized drums, 16-step sequencer, patterns, kits) | Adds a second 4 KB flash sector for project data | `FM-1_950` (**collides with CS-1**) |
| **DoppleDecker** | An FM synth plus a PC tool that fits an FM patch to an audio snippet, using the same integer engine as the firmware | Parity between the PC library and the device; a bank format (`.ddb`) that the firmware cannot load yet | `FM-1_940` (**collides with sloopDX**) |

**Corrections this guide suggests for our own documents (not applied; the human decides):**

1. `RULES.md` and `CLAUDE.md`: replace "no float / no FPU" with the nuance in 3.3 (float works with the FPU flags; `double` and 64-bit division do not), and add the second core, the 360 MHz clock and the real audio rate.
2. `CLAUDE.md`: replace the "taken identities" list with "check the live catalogue" (Appendix E); record the three collisions and decide whether to renumber.
3. `API.md`: note that SysEx frames can reach the app through the `ota_frame` hooks; that USB audio into the device exists in the field; that the half-buffer length differs by base (128 vs 256).
4. Add the recovery section (2.3), the boot-stub rule (2.4) and the Transporter to the workspace notes.
5. Mention that Felucca is in maintenance mode and where `felucca-bsp` and `fm1_open` stand.
6. Consider a CI-style script that fails a build on soft-double symbols (if FPU flags are adopted) and a platform-integrity guard like FuMi's.

**Things we have not done yet that this survey says matter:** the FPU flags, a second core, audio code in SRAM, a boot stub, velocity, an owner tag for flash records, a device-measured CPU number for the scenes, validation of the scenes on a real unit.

**Publishing a repository:** a public push publishes every commit's author name and e-mail and the commit timestamps' time zone. Scan the working tree **and the history** for names, e-mail addresses, local paths and machine details first; rewrite commit identities before the first push if needed (we did this for CYBERDECK, using a neutral author and UTC time stamps, after verifying that the file tree was byte-identical).

## Appendix G: Method, limits, and how to refresh

**What was done (2026-10-16).** The hub's catalogue (`/catalog.json`, updated 2026-10-16 12:31 UTC) listed 52 firmwares with repositories. Five research agents each read a group of repositories (the platform bases and forks; grooveboxes, drums and samplers; synth engines and DSP; chord, controller and toy firmwares; hardware, tooling and community), through `gh` and shallow clones, and wrote notes labelled [source], [claim] or [guess]. **Nothing from those repositories was built, run or imported.** We read the reports and re-checked the claims this guide leans on most: the hub catalogue and its fields; the FPU (a compile test with the JieLi clang 4.0.1 in WSL: soft-float calls without the flags, none with them); that Dinghy's `ota.c` gets SysEx frames through two hooks the firmware supplies; the Felucca maintenance announcement date (2026-10-11, last updated 2026-10-16); that `m-vave/fm1_open` exists and was pushed on 2026-10-16 (a release V12 on 2026-10-14); that the velocity document and the key-sensing code exist; the identity collisions.

**Not read or not verified:**
- Reddit (`r/MVaveFM1`) and Gearspace content (the fetch tool could not read them); the official M-VAVE product page (rendered by JavaScript).
- X0X, Melodee and Jangada source beyond the build flags, the HAL files and the documents cited; the contents of `Felucca-Engines` and `Felucca-WebApp`; `fm1_open`'s binary engine; the hub's sync and mirror script (not public).
- Nothing was measured on a device by us in this survey. Device measurements quoted here are the named authors'.
- The exact AC791N variant, the codec and amplifier parts, the charger IC, whether the Wi-Fi RF path is fitted, the TRS jack's polarity, why the core-1 and RF-trim problems differ between units.
- Whether `fm1_open` will stay public, and what its licence means for Felucca-lineage code.

**Conflicts between sources (resolve before relying on either):**
- USB audio **into** the FM-1: JIANT's test builds and M-VAVE's SDK say yes; ChoralRoot's manual says no.
- Whether AMB-1, TRIBE, Bubba, DUELL and FM-106 boot in an emulator: the QEMU emulator's hub page says no; the other emulator's table says yes.
- Audio code in SRAM versus flash: DUELL and Melodee measured a gain; MC-1 reverted after a regression; Bubba saw no difference.
- Panel SPI limit: 16 MHz (one project's reading) versus about 62 MHz (another); both ran at higher speeds on real units.
- Jangada's voice count (8 or 12 in different documents).
- Hub descriptions sometimes disagree with repo READMEs (Jangada's engine count; Melodee's "bit-exact Dexed" after its float conversion).

**To refresh this guide:** re-fetch `https://thefirmwarehub.com/catalog.json` (counts, identities, new firmwares, new releases); `gh api repos/hugelton/Felucca/discussions` for platform news; `gh api repos/m-vave/fm1_open` for the vendor SDK; re-run the compile test in 3.3 with your toolchain; re-run the identity check (Appendix E); then update the date at the top and the evidence labels.
