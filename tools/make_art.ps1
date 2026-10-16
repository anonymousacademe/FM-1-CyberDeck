# SPDX-License-Identifier: GPL-3.0-only
# The scene plates, start to finish: EffectCraft draws them (assets/art/plates.jsx), the PNGs land in assets/art/plates/,
# and tools/art2plate.py turns them into firmware/gen/art_plates.h.
#   .\tools\make_art.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$ec = "C:\Program Files\EffectCraft\effectcraft-cli.exe"
$proj = Join-Path $env:TEMP "cyberdeck-plates.ecproj"
$out = Join-Path $root "assets\art\plates"
New-Item -ItemType Directory -Force $out | Out-Null
& $ec --empty script (Join-Path $root "assets\art\plates.jsx") --save-as $proj
if ($LASTEXITCODE) { throw "EffectCraft script failed" }
& $ec --project $proj render-frame --comp boot --time 0 --out (Join-Path $out "boot.png")
foreach ($i in 0..7) {
    & $ec --project $proj render-frame --comp "p$i" --time 0 --max-side 120 --out (Join-Path $out "p$i.png")
}
python (Join-Path $root "tools\art2plate.py") $out (Join-Path $root "firmware\gen\art_plates.h")
