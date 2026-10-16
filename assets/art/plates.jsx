// CYBERDECK plates: grayscale fields drawn in EffectCraft. The firmware maps each one through a live neon palette (and
// glitches and cycles it): bright = lines and glow, dark = space. See assets/art/README.md and tools/make_art.ps1.
// Every comp is 240 x 240; the page plates (p0..p7) are rendered at half size, 120 x 120, and drawn 2 x 2 on the screen.
var W = 240, H = 240;                                  // (enum values in the script are one-based: 1 = the first choice)

var seed = 12345;
function rnd() { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; }

function fx(layer, name, props) {                       // add an effect and set its properties by id
    var e = layer.property("Effects").addProperty(name);
    for (var k in props) {
        var p = e, parts = k.split("/");                // "transform/scale": a property inside a group
        for (var i = 0; i < parts.length; i++) p = p.property(parts[i]);
        p.setValue(props[k]);
    }
    return e;
}
function comp(name) { return app.project.items.addComp(name, W, H, 1, 1, 24); }
function solid(c, name, rgb, add) {                     // a layer; add: lights add up instead of covering
    var l = c.layers.addSolid(rgb, name, W, H, 1);
    if (add) l.blendingMode = BlendingMode.ADD;
    return l;
}
function rect(c, x, y, w, h, v) {                       // a box with its top left at x, y
    var l = c.layers.addSolid([v, v, v], "box", w, h, 1);
    l.property("Transform").property("Position").setValue([x + w / 2, y + h / 2]);
    return l;
}
function finish(c, glow, dim) {                         // glow, then pure luminance; dim: scale the brightest value (0..1)
    if (glow) {
        var g = c.layers.addSolid([0.5, 0.5, 0.5], "glow", W, H, 1);
        g.adjustmentLayer = true;
        fx(g, "Glow", {threshold: glow[0], radius: glow[1], intensity: glow[2]});
    }
    var a = c.layers.addSolid([0.5, 0.5, 0.5], "finish", W, H, 1);
    a.adjustmentLayer = true;
    fx(a, "Black & White", {});
    if (dim) fx(a, "Levels", {outWhite: dim});
}
function skyline(c, base, hmin, hmax, wmin, wmax, v) {  // silhouettes standing on y = base
    var x = -4;
    while (x < W) {
        var w = Math.round(wmin + rnd() * (wmax - wmin)), h = Math.round(hmin + rnd() * (hmax - hmin));
        rect(c, x, base - h, w, h + 2, v);
        if (rnd() < 0.12) rect(c, x + w / 2 - 1, base - h - 8, 2, 8, v);     // an antenna
        x += w + (rnd() < 0.25 ? 3 : 0);
    }
}
function floorGrid(c, horizon, cell, hw) {              // a grid floor flying away to the horizon
    var f = solid(c, "floor", [0, 0, 0], true);
    fx(f, "Grid", {width: cell, height: cell, border: 2, color: [1, 1, 1, 1]});
    fx(f, "Corner Pin", {ul: [120 - hw, horizon], ur: [120 + hw, horizon], ll: [-340, H], lr: [580, H]});
    return f;
}
function ramp(c, y0, y1, v) {                           // a vertical glow ramp
    var s = solid(c, "ramp", [0, 0, 0], true);
    fx(s, "Gradient Ramp", {start: [120, y0], startColor: [0, 0, 0, 1], end: [120, y1], endColor: [v, v, v, 1]});
    return s;
}
function sun(c, cy, r, stripes) {
    var s = solid(c, "sun", [0, 0, 0], true);
    fx(s, "Circle", {center: [120, cy], radius: r, color: [1, 1, 1, 1]});
    if (stripes) fx(s, "Venetian Blinds", {completion: 40, direction: 90, width: 7});
    s.property("Transform").property("Opacity").setValue(66);   // dimmer than the grid, so the palette makes it warm
    return s;
}

// ---- boot: a striped sun on the horizon, the skyline, a grid floor
(function () {
    var c = comp("boot");
    solid(c, "bg", [0, 0, 0], false);
    ramp(c, 0, 150, 0.32);
    sun(c, 112, 46, true);
    skyline(c, 150, 8, 58, 8, 20, 0);
    floorGrid(c, 150, 24, 46);
    finish(c, [55, 14, 1.2], 0);
})();

// ---- p0 OSC: a dense city in the rain
(function () {
    var c = comp("p0");
    solid(c, "bg", [0, 0, 0], false);
    ramp(c, 40, 235, 0.6);
    skyline(c, 236, 50, 160, 12, 28, 0.04);
    var win = solid(c, "windows", [0, 0, 0], true);     // lit windows: a grid of small squares, thinned out by noise
    fx(win, "Grid", {width: 8, height: 10, border: 3.2, invertGrid: true, color: [1, 1, 1, 1]});
    fx(win, "Fractal Noise", {noiseType: 1, contrast: 500, brightness: -130, "transform/scale": 40, complexity: 1});
    fx(win, "Levels", {inWhite: 0.3});
    var rain = solid(c, "rain", [0, 0, 0], true);
    rain.property("Transform").property("Opacity").setValue(28);
    fx(rain, "Fractal Noise", {noiseType: 1, contrast: 500, brightness: -120, "transform/uniformScaling": false,
                               "transform/scaleWidth": 4, "transform/scaleHeight": 500, complexity: 1});
    finish(c, [60, 6, 1.0], 0.9);
})();

// ---- p1 FLT: a corridor of grid, floor and ceiling, a glow at the far end
(function () {
    var c = comp("p1");
    solid(c, "bg", [0, 0, 0], false);
    var fl = floorGrid(c, 120, 24, 22);
    var ce = solid(c, "ceiling", [0, 0, 0], true);
    fx(ce, "Grid", {width: 24, height: 24, border: 2, color: [1, 1, 1, 1]});
    fx(ce, "Corner Pin", {ul: [-340, 0], ur: [580, 0], ll: [120 - 22, 120], lr: [120 + 22, 120]});
    var e = solid(c, "far", [0, 0, 0], true);
    fx(e, "Ellipse", {center: [120, 120], width: 70, height: 36, thickness: 40, softness: 90,
                      insideColor: [1, 1, 1, 1], outsideColor: [0, 0, 0, 1]});
    finish(c, [50, 10, 1.4], 0);
})();

// ---- p2 ENV: rain on dark, a low glow
(function () {
    var c = comp("p2");
    solid(c, "bg", [0, 0, 0], false);
    ramp(c, 110, 240, 0.45);
    var rain = solid(c, "rain", [0, 0, 0], true);
    fx(rain, "CC Rainfall", {drops: 1400, size: 1.2, speed: 4000, wind: 6, color: [1, 1, 1, 1], opacity: 80, transferMode: 1});
    finish(c, [50, 6, 1.0], 0);
})();

// ---- p3 LFO: a CRT, wobbling lines, scanlines
(function () {
    var c = comp("p3");
    solid(c, "bg", [0, 0, 0], false);
    var g = solid(c, "lines", [0, 0, 0], true);
    fx(g, "Grid", {sizeFrom: 3, width: 240, height: 20, border: 2, color: [1, 1, 1, 1]});
    fx(g, "Wave Warp", {waveType: 1, height: 16, width: 110, direction: 90, speed: 0, phase: 0});
    var h = solid(c, "ticks", [0, 0, 0], true);
    fx(h, "Grid", {sizeFrom: 3, width: 20, height: 240, border: 1, color: [0.4, 0.4, 0.4, 1]});
    var s = solid(c, "scan", [0, 0, 0], true);
    fx(s, "Venetian Blinds", {completion: 50, direction: 90, width: 4});
    finish(c, [50, 8, 1.2], 0);
})();

// ---- p4 FX: digital glitch: blocks, bars and noise
(function () {
    var c = comp("p4");
    solid(c, "bg", [0, 0, 0], false);
    var n = solid(c, "blocks", [0, 0, 0], true);
    fx(n, "Fractal Noise", {noiseType: 1, contrast: 300, brightness: -90, "transform/uniformScaling": false,
                            "transform/scaleWidth": 160, "transform/scaleHeight": 14, complexity: 2});
    var m = solid(c, "chunks", [0, 0, 0], true);
    fx(m, "Fractal Noise", {noiseType: 1, contrast: 400, brightness: -80, "transform/scaleWidth": 60,
                            "transform/uniformScaling": true, "transform/scale": 60, complexity: 1, "transform/offset": [200, 90]});
    fx(m, "Mosaic", {horizontal: 12, vertical: 8, sharpColors: true});
    var b = solid(c, "bars", [0, 0, 0], true);
    fx(b, "Venetian Blinds", {completion: 85, direction: 90, width: 18});
    b.property("Transform").property("Opacity").setValue(35);
    finish(c, [50, 5, 1.0], 0.5);
})();

// ---- p5 GLO: a radar: rings, a crosshair, spokes
(function () {
    var c = comp("p5");
    solid(c, "bg", [0, 0, 0], false);
    for (var i = 1; i <= 4; i++) {
        var r = solid(c, "ring" + i, [0, 0, 0], true);
        fx(r, "Ellipse", {center: [120, 120], width: i * 56, height: i * 56, thickness: 2.5, softness: 0,
                          insideColor: [1, 1, 1, 1], outsideColor: [0, 0, 0, 1]});
    }
    var sp = solid(c, "spokes", [0, 0, 0], true);
    fx(sp, "Grid", {sizeFrom: 3, width: 16, height: 240, border: 1, color: [0.6, 0.6, 0.6, 1]});
    fx(sp, "Polar Coordinates", {conversion: 1, interpolation: 100});
    rect(c, 119, 0, 2, 240, 0.9);
    rect(c, 0, 119, 240, 2, 0.9);
    finish(c, [45, 8, 1.3], 0);
})();

// ---- p6 ARP: hexagonal-ish cells with bright edges
(function () {
    var c = comp("p6");
    solid(c, "bg", [0, 0, 0], false);
    var cp = solid(c, "cells", [0, 0, 0], true);
    fx(cp, "Cell Pattern", {cellPattern: 7, size: 30, contrast: 100, disperse: 0.4});
    fx(cp, "Find Edges", {invert: true});
    fx(cp, "Levels", {inBlack: 0.0, inWhite: 1.0, gamma: 2.2});

    finish(c, [50, 8, 1.3], 0.55);
})();

// ---- p7 SEQ: a big sun, mountains, a deep grid floor
(function () {
    var c = comp("p7");
    solid(c, "bg", [0, 0, 0], false);
    ramp(c, 20, 140, 0.3);
    sun(c, 96, 58, true);
    var x = 0;
    while (x < W) {                                     // a jagged ridge
        var w = 30 + Math.round(rnd() * 40), h = 18 + Math.round(rnd() * 32);
        var m = solid(c, "mtn", [0, 0, 0], false);
        fx(m, "Corner Pin", {ul: [x + w / 2, 140 - h], ur: [x + w / 2 + 1, 140 - h], ll: [x - 8, 142], lr: [x + w + 8, 142]});
        x += w - 6;
    }
    floorGrid(c, 142, 24, 60);
    finish(c, [55, 12, 1.1], 0);
})();

writeLn("comps " + app.project.numItems);
