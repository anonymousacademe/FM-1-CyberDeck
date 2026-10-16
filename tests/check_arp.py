#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
r"""Check the voice trace of tests/arp.txt (the 'voice on' / 'voice off' lines of the host run).
   .\fm1.ps1 host cyberdeck tests/arp.txt > build\host\arp-trace.txt ; python tests\check_arp.py build\host\arp-trace.txt"""
import re, sys

on, off = [], []
for line in open(sys.argv[1]):
    m = re.match(r"\s*(\d+) ms  voice (on |off)\s+(\d+)", line)
    if m:
        (on if m[2] == "on " else off).append((int(m[1]), int(m[3])))


def ons(a, b):
    return [n for t, n in on if a <= t < b]


def sounding(at):                               # notes started and not released by `at` ms (this script never steals a voice)
    held = set()
    for t, d, n in sorted([(t, 1, n) for t, n in on] + [(t, 0, n) for t, n in off]):
        if t > at:
            break
        (held.add if d else held.discard)(n)
    return sorted(held)


def mean_gap(a, b):
    ts = [t for t, n in on if a <= t < b]
    return (ts[-1] - ts[0]) / (len(ts) - 1) if len(ts) > 1 else 0


checks = [
    ("A: keys up mid-gate, the arp note ends", ons(300, 960) != [] and sounding(1150) == [] and ons(1000, 1200) == []),
    ("B: HOLD latches the held chord only (60)", ons(2000, 2300) != [] and set(ons(2300, 3500)) == {60}),
    ("B: ARP off, the held arp note ends", sounding(3700) == [] and ons(3600, 4000) == []),
    ("C: MIDI 45 still held after the sequencer's 45 ends",
     any(n == 45 and 4600 <= t < 4610 for t, n in off) and 45 in ons(4000, 4020) and ons(4000, 4020).count(45) == 2),
    ("D: 1/4 is one beat (500 ms)", abs(mean_gap(5300, 7400) - 500.0) < 1.0),
    ("D: 1/16T is two thirds of a sixteenth (83.3 ms)", abs(mean_gap(7500, 9700) - 250.0 / 3) < 0.5),
    ("E: OCTAVES 2, UP, low to high", ons(9900, 10400)[:4] == [53, 65, 72, 84]),
    ("end: nothing left sounding", sounding(11450) == []),
]
bad = 0
for name, ok in checks:
    print(("ok    " if ok else "FAIL  ") + name)
    bad += not ok
print(f"{len(checks) - bad} of {len(checks)} ok")
sys.exit(1 if bad else 0)
