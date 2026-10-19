#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Two-part mode: run the host build (build/host/cyberdeck) on tests/layer*.txt and check what it does.
   wsl: tests/run_host.sh tests/layer.txt >/dev/null ; python3 tests/check_layer.py       (needs build/host/cyberdeck)"""
import array, os, re, subprocess, sys, tempfile, wave

HOST = "build/host/cyberdeck"
tmp = tempfile.mkdtemp()


def run(script, out="o", env=None):
    e = dict(os.environ)
    e.update(env or {})
    r = subprocess.run([HOST, os.path.join(tmp, out), script], capture_output=True, text=True, env=e)
    return r.stdout


def parse(txt):
    d = {"on": [], "off": [], "part": [], "out": [], "lay": []}
    for line in txt.splitlines():
        m = re.match(r"\s*(\d+) ms  voice on\s+(\d+)", line)
        if m: d["on"].append((int(m[1]), int(m[2]))); continue
        m = re.match(r"\s*(\d+) ms  voice off\s+(\d+)", line)
        if m: d["off"].append((int(m[1]), int(m[2]))); continue
        m = re.match(r"\s*(\d+) ms  part ([AB])  note\s+(\d+)  voice (\d)", line)
        if m: d["part"].append((int(m[1]), m[2], int(m[3]), int(m[4]))); continue
        m = re.match(r"\s*(\d+) ms  MIDI out ([0-9A-F]+) (\d+) (\d+)", line)
        if m: d["out"].append((int(m[1]), int(m[2], 16), int(m[3]), int(m[4]))); continue
        m = re.match(r"\s*(\d+) ms  layer (on|off|swap)", line)
        if m: d["lay"].append((int(m[1]), m[2]))
    return d


def wav(name):
    w = wave.open(os.path.join(tmp, name + ".wav"))
    a = array.array("h")
    a.frombytes(w.readframes(w.getnframes()))
    return a


T = parse(run("tests/layer.txt"))
lay = {k: [t for t, x in T["lay"] if x == k] for k in ("on", "off", "swap")}
t_on, t_sw, t_off = lay["on"][0], lay["swap"][0], lay["off"][0]
PAT = [45, 0, 45, 0, 48, 0, 45, 0, 43, 0, 43, 45, 0, 50, 0, 48]       # the default line; the first note sounds at the PLAY press
t0 = min(t for t, n in T["on"])
want = [(t0 + 125 * (16 * k + s), PAT[s]) for k in range(6) for s in range(16) if PAT[s] and t_on + 150 < t0 + 125 * (16 * k + s) < t_sw - 200]
got = [(t, n) for t, p, n, v in T["part"] if p == "B" and t_on + 150 < t < t_sw - 200]
same_grid = len(want) == len(got) and all(abs(a[0] - b[0]) <= 15 and a[1] == b[1] for a, b in zip(want, got))
chord1 = [(n, v) for t, p, n, v in T["part"] if p == "A" and 3400 < t < 3600]
chord2 = [(n, v) for t, p, n, v in T["part"] if p == "A" and 6700 < t < 6900]
b_before = {v for t, p, n, v in T["part"] if p == "B" and t_on < t < t_sw}
b_after = {v for t, p, n, v in T["part"] if p == "B" and t_sw < t < t_off}
outs = lambda a, b, s: [(n, v) for t, st, n, v in T["out"] if a <= t < b and st == s]
tail = wav("o")[-2 * 44100 // 4:]

# the arpeggiator in the layer, and MIDI in
A = parse(run("tests/layer_arp.txt", "a"))
arp_b = [(n, v) for t, p, n, v in A["part"] if p == "B" and 1600 <= t < 3100]
a_keys = [(t, n, v) for t, p, n, v in A["part"] if p == "A"]

# a power cycle: the layer comes back
store = os.path.join(tmp, "store.bin")
SA = run("tests/layer_save_a.txt", "sa", {"FM1_HOST_STORE": store})
SB = parse(run("tests/layer_save_b.txt", "sb", {"FM1_HOST_STORE": store}))

# both parts the same patch: the layer is a copy of the keys' sound. The waveform differs (a note lands on another voice, whose oscillators
# are in another phase), so compare the loudness of the whole run: the delay's send and return are right if the energy is the same
run("tests/layer_eq_on.txt", "eqon")
run("tests/layer_eq_off.txt", "eqoff")
a, b = wav("eqon"), wav("eqoff")
rms = lambda x: (sum(v * v for v in x) / max(1, len(x))) ** 0.5
ratio = rms(a) / max(1.0, rms(b))

if "-v" in sys.argv:
    print("want", want)
    print("got ", got)
checks = [
    ("layer on at the SEQ hold, swap at the ARP hold, off at the second SEQ hold", 2000 < t_on < 2300 and 6000 < t_sw < 6300 and 9000 < t_off < 9300),
    ("the sequence goes on, on the grid, through the keys' patch change (2600 ms)", same_grid and len(got) >= 15),
    ("the layer's mono bass plays on voice 3 only", b_before == {3}),
    ("a three-note chord on the keys uses voices 0, 1, 2", sorted(v for n, v in chord1) == [0, 1, 2] and len(chord1) == 3),
    ("the keys' patch change does not stop the layer: its note ends by itself", any(2550 < t < 2700 and n == 45 for t, n in T["off"])),
    ("...and the change cuts the keys' part only (B0, no B1 at 2601 ms)",
     any(2590 < t < 2700 and s == 0xB0 for t, s, n, v in T["out"]) and not any(2590 < t < 2700 and s == 0xB1 for t, s, n, v in T["out"])),
    ("the swap ends the notes of both parts", any(6090 < t < 6110 and s == 0xB0 for t, s, n, v in T["out"]) and any(6090 < t < 6110 and s == 0xB1 for t, s, n, v in T["out"])),
    ("after the swap the keys are the mono patch: the chord shares voice 0", sorted(v for n, v in chord2) == [0, 0, 0]),
    ("...and the layer is poly on the shared voices 1..3", b_after <= {1, 2, 3} and len(b_after) >= 2),
    ("MIDI out: the layer on channel 2 (91), the keys on channel 1 (90)",
     len(outs(t_on + 100, t_off, 0x91)) >= 20 and outs(3400, 3600, 0x90) == [(53, 100), (60, 100), (67, 100)]),
    ("layer off: no part tags, the layer's channel goes quiet", not [1 for t, p, n, v in T["part"] if t > t_off] and not [1 for t, s, n, v in T["out"] if t > t_off + 50 and s in (0x91, 0x81)]),
    ("layer off: the chord plays as before (3 voice-ons)", sum(1 for t, n in T["on"] if 9400 < t < 9600) == 3),
    ("nothing rings at the end", max((abs(x) for x in tail), default=0) < 600),
    ("ARP tap (acts on release), then keys: the arpeggio plays in the layer, not part A",
     arp_b and all(v == 3 for n, v in arp_b) and {n for n, v in arp_b} == {53, 60} and not [1 for t, n, v in a_keys if t < 3450]),
    ("ARP off, MIDI in: part A", [(n, v) for t, n, v in a_keys if t >= 3450] == [(60, 0)]),
    ("a power cycle: the layer is still on (keys tagged part A)", "project saved" in SA and any(p == "A" for t, p, n, v in SB["part"])),
    ("both parts the same patch: as loud as one part (within 8 %, delay send and return included)", 0.92 < ratio < 1.08),
]
bad = 0
for name, ok in checks:
    print(("ok    " if ok else "FAIL  ") + name)
    bad += not ok
print(f"{len(checks) - bad} of {len(checks)} ok   (loudness ratio {ratio:.3f})")
sys.exit(1 if bad else 0)
