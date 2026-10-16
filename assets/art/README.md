# The scene art

The loading screen and the eight page scenes start as **plates**: grayscale pictures drawn in
EffectCraft (one of the ArtCraft apps). The firmware does the colour: it maps each value of a plate through a live
neon palette (so one plate can be sunset, ice or radar green), ripples the palette, heats it with every note, slides the grid floors in
perspective and tears bands of rows sideways (`firmware/src/scenes.h`). Bright is line and glow, dark is space.

| Plate | Size | Scene |
|---|---|---|
| `boot` | 240 x 240 | the loading screen: striped sun, skyline, grid floor |
| `p0` .. `p7` | 120 x 120 (drawn 2 x 2) | OSC city, FLT corridor, ENV rain, LFO wobbling lines, FX glitch, GLO radar, ARP mesh, SEQ sunset |

## Remake them

```powershell
.\tools\make_art.ps1        # plates.jsx -> EffectCraft -> assets/art/plates/*.png -> firmware/gen/art_plates.h
.\fm1.ps1 host cyberdeck tests/scenes.txt
```

`plates.jsx` is an EffectCraft script (After Effects style): one function per plate, built from solids and effects (Gradient Ramp, Circle,
Venetian Blinds, Grid, Corner Pin, Fractal Noise, CC Rainfall, Wave Warp, Polar Coordinates, Cell Pattern, Glow...). Things learned:

- Effect properties are set by id: `fx(layer, "Grid", {width: 24, border: 2})`; a property inside a group by `"group/id"`. **Enum values are
  one-based** in the script (1 = the first choice).
- Layers that only add light use the Add blend mode (the `solid(c, name, rgb, true)` helper), otherwise each opaque solid covers the one below.
- The grid floors in `boot`, `p1` and `p7` are a Grid through Corner Pin. The firmware slides them by remapping rows with the same
  perspective maths (`FLR[]` and `flr_row()` in `scenes.h`): if you move a horizon or change a corner pin, change that table too.
- Each plate ends with Glow, Black & White and an optional brightness trim (the last argument of `finish`): the dimmer the plate, the better
  the overlays read.
