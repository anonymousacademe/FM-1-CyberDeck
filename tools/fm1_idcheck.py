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
