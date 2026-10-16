/* SPDX-License-Identifier: GPL-3.0-only */
/* The neon scenes (main loop). Included by cyberdeck.c after the UI code.
 *
 * Every page has a scene: a plate drawn in EffectCraft (assets/art/plates.jsx -> firmware/gen/art_plates.h, a field of 0..255) that
 * the firmware maps through a live neon palette, and on top of it the page's parameters as light: the oscillator wave, the filter
 * curve, the envelope, the LFO, the echoes, a radar, the arpeggio ring, the sequence as a skyline.
 *
 *   OSC CITY  FLT CORRIDOR  ENV RAIN  LFO WAVES  FX GLITCH  GLO RADAR  ARP MESH  SEQ SUNSET        (and BOOT, the loading screen)
 *
 * What moves: the palette ripples (the plate's values shift along the colour ramp), a note flashes it and heats it, grid floors fly
 * towards you (the plate's rows are remapped in perspective, so the lines slide), and a note tears bands of rows sideways with a
 * red/blue split. CRUSH quantises the FX scene's palette. The picture is 120 x 120 cells drawn 2 x 2 (BOOT: 240 x 240), sent as
 * eight strips of 30 rows, one strip per pass of the main loop, so the keys and knobs are read every ~10 ms while a frame goes out. */
#include "art_plates.h"

#define SC_BOOT 8u

/* palette ramps: six stops, black to the brightest. Per theme: what the plate's values turn into */
static const uint8_t TH[8][6][3] = {
    {{0, 0, 0}, {24, 0, 70}, {150, 0, 160}, {255, 50, 110}, {255, 190, 50}, {200, 255, 255}},     /* 0 SUNSET: magenta, orange, ice */
    {{0, 0, 0}, {10, 0, 56}, {96, 0, 150}, {255, 0, 150}, {0, 230, 255}, {255, 255, 255}},       /* 1 NEON: purple, magenta, cyan */
    {{0, 0, 0}, {0, 10, 64}, {0, 64, 170}, {0, 170, 255}, {120, 230, 255}, {255, 255, 255}},     /* 2 ICE */
    {{0, 0, 0}, {0, 24, 20}, {0, 90, 76}, {0, 210, 120}, {150, 255, 170}, {235, 255, 245}},      /* 3 MATRIX */
    {{0, 0, 0}, {44, 10, 0}, {160, 30, 0}, {255, 110, 0}, {255, 215, 60}, {255, 255, 200}},      /* 4 AMBER */
    {{0, 0, 0}, {56, 0, 44}, {210, 0, 64}, {255, 0, 160}, {0, 235, 255}, {255, 255, 255}},       /* 5 GLITCH: red, magenta, cyan */
    {{0, 0, 0}, {0, 34, 12}, {0, 120, 54}, {64, 255, 96}, {196, 255, 140}, {240, 255, 230}},     /* 6 RADAR */
    {{0, 0, 0}, {34, 0, 66}, {110, 24, 210}, {210, 64, 255}, {255, 150, 235}, {255, 235, 255}}   /* 7 VIOLET */
};
static const uint8_t PAGE_TH[9] = {1, 2, 3, 4, 5, 6, 7, 0, 0};     /* OSC FLT ENV LFO FX GLO ARP SEQ BOOT */
static const uint8_t *const PLATE[9] = {PLATE_P0, PLATE_P1, PLATE_P2, PLATE_P3, PLATE_P4, PLATE_P5, PLATE_P6, PLATE_P7, PLATE_BOOT};
/* the grid floors in the plates: the horizon row, the width of the grid at the horizon and at the bottom (all in the 240 x 240
 * art), and kind 1 = a floor, 2 = a floor and the same as a ceiling (the corridor) */
static const struct { uint8_t hz, wt, kind; uint16_t wb; } FLR[9] = {
    {0, 0, 0, 0}, {120, 44, 2, 920}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {142, 120, 1, 920}, {150, 92, 1, 920}};
#define FLR_PERIOD 6554u                        /* one grid cell, in Q16 of the plate's depth coordinate (a tenth) */

static uint16_t spal[2][256];                   /* the palette: even and odd screen rows (scan lines), byte swapped for the screen */
static uint8_t srow[240];                       /* the plate row to show on each row (the grid floors slide) */
static int8_t sgl[240];                         /* a sideways tear of a row, in plate pixels */
static uint16_t sc_core, sc_glow, sc_dim;       /* the colours of the lines drawn on top (RGB565) */
static uint32_t sc_strip_i, sc_last, sc_level, sc_kick, sc_trig_seen, sc_scroll, sc_rnd = 0x2545F491u;
static uint32_t sc_fade = 256, sc_gl, sc_roll;  /* the loading screen sets these: a fade in, extra tearing, a rolling picture */
static uint32_t sc_touch, sc_touch_ms[4];       /* the knob turned last (1..4) and when */
static const char *sc_msg;                      /* a short message for the top line */
static uint32_t sc_phase;                       /* the LFO scene's running phase, 16 bits per cycle */

static uint32_t sc_rand(void) { sc_rnd ^= sc_rnd << 13; sc_rnd ^= sc_rnd >> 17; sc_rnd ^= sc_rnd << 5; return sc_rnd; }

static uint16_t rgb565v(uint32_t r, uint32_t g, uint32_t b)
{
    r = r > 255u ? 255u : r; g = g > 255u ? 255u : g; b = b > 255u ? 255u : b;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
static uint16_t th565(uint32_t th, uint32_t stop, uint32_t pct)
{
    return rgb565v(TH[th][stop][0] * pct / 100u, TH[th][stop][1] * pct / 100u, TH[th][stop][2] * pct / 100u);
}

/* ---- the palette: a ramp through the stops, rippled by the clock and heated by a note */
static void sc_palette(uint32_t id)
{
    uint32_t i, th = PAGE_TH[id], ph = fm1_ms >> 4, br = 210u + (sc_level >> 2) + (sc_kick >> 2), q = 1;
    if (id == 4u) q = 1u + (((uint32_t)P[P_CRUSH] * 6u) >> 7);       /* CRUSH: fewer colours */
    br = br * sc_fade >> 8;
    for (i = 0; i < 256u; i++) {
        uint32_t j = i, seg, t, c[3], k;
        if (i >= 24u) {
            int32_t jj = (int32_t)i + (SIN8[(i * 3u + ph) & 255u] >> 3) + (int32_t)(sc_kick >> 4);
            j = (uint32_t)(jj < 0 ? 0 : jj > 255 ? 255 : jj);
        }
        if (q > 1u) j -= j % q;
        seg = (j * 5u) >> 8;
        t = (j * 5u) & 255u;
        for (k = 0; k < 3u; k++) {
            int32_t a = TH[th][seg][k], b = TH[th][seg + 1u][k];
            c[k] = (uint32_t)(a + (((b - a) * (int32_t)t) >> 8)) * br >> 8;
        }
        spal[0][i] = swap16(rgb565v(c[0], c[1], c[2]));
        spal[1][i] = swap16(rgb565v(c[0] * 3u >> 2, c[1] * 3u >> 2, c[2] * 3u >> 2));      /* the scan line */
    }
    sc_core = th565(th, 4, 100); sc_glow = th565(th, 3, 55); sc_dim = th565(th, 2, 70);
}

/* the row of the plate that shows on row y of a floor in perspective, when the grid has slid s (Q16 of a depth coordinate):
 * the lines are at equal steps of depth, and a flat floor's rows go as 1 / width, so the depth of a row is found, moved, and
 * mapped back. Integers: every product stays below 2^32 for these plates. */
static uint32_t flr_row(uint32_t y, uint32_t hz, uint32_t n, uint32_t wt, uint32_t wb, uint32_t s)
{
    uint32_t w = wt + (wb - wt) * (y - hz) / (n - hz), v, den, w16, row;
    v = (((w - wt) << 16) / w) * wb / (wb - wt);
    if (v < s) return hz;
    v -= s;
    den = wb * 65536u - (wb - wt) * v;
    w16 = ((wt * wb) << 14) / (den >> 6);
    row = hz + (n - hz) * (w16 - wt * 16u) / ((wb - wt) * 16u);
    return row < hz ? hz : row > n - 1u ? n - 1u : row;
}

/* ---- once per frame: the clock, the level, the palette, the sliding floor, the tears */
static void sc_frame(uint32_t id)
{
    uint32_t dt = fm1_ms - sc_last, n = id == SC_BOOT ? 240u : 120u, sc = 240u / n, r, nb, g, i, th = PAGE_TH[id];
    sc_last = fm1_ms;
    dt = dt < 4u ? 4u : dt > 100u ? 100u : dt;
    if (vis_trig != sc_trig_seen) { sc_trig_seen = vis_trig; sc_kick = 255u; }       /* a note struck */
    else sc_kick -= sc_kick * dt / 400u + (sc_kick ? 1u : 0u);
    g = vis_pk >> 7;
    g = g > 255u ? 255u : g;
    sc_level = (sc_level * 3u + g) >> 2;
    sc_scroll = (sc_scroll + (3u + (sc_level >> 5) + (sc_kick >> 3)) * dt) % FLR_PERIOD;
    sc_phase = (sc_phase + (20u + P[P_LRATE]) * dt) & 0xFFFFu;
    sc_palette(id);

    for (r = 0; r < n; r++) srow[r] = (uint8_t)r;
    if (FLR[id].kind) {                         /* the floor (and the ceiling) slides towards you */
        uint32_t hz = FLR[id].hz / sc, wt = FLR[id].wt / sc, wb = FLR[id].wb / sc;
        for (r = hz + 3u; r < n; r++) srow[r] = (uint8_t)flr_row(r, hz, n, wt, wb, sc_scroll);
        if (FLR[id].kind == 2u)
            for (r = 0; r + hz + 3u < 2u * hz; r++) {
                uint32_t m = 2u * hz - r;
                srow[r] = (uint8_t)(m >= n ? 2u * hz - (n - 1u) : 2u * hz - flr_row(m, hz, n, wt, wb, sc_scroll));
            }
    }
    for (r = 0; r < n; r++) sgl[r] = 0;
    g = (sc_kick >> 1) + (sc_level >> 3) + sc_gl;      /* the tears: a few bands of rows, sideways, the harder the more going on */
    nb = g >> 5;
    if (nb > 6u) nb = 6u;
    if (!nb && (sc_rand() & 31u) == 0u) nb = 1u;       /* and now and then, for nothing */
    for (i = 0; i < nb; i++) {
        uint32_t y0 = sc_rand() % n, h = 2u + sc_rand() % (n / 20u + 1u), a = 2u + sc_rand() % (3u + (g >> 4)), k;
        int32_t o = (sc_rand() & 1u) ? (int32_t)a : -(int32_t)a;
        for (k = 0; k < h && y0 + k < n; k++) sgl[y0 + k] = (int8_t)o;
    }
    if (sc_roll && id == SC_BOOT)                      /* a picture that rolls and settles */
        for (r = 0; r < n; r++) srow[r] = (uint8_t)((srow[r] + sc_roll) % n);
    (void)th;
}

/* one row of the plate into the canvas: x2 = 1 for the 120 wide ones (every pixel twice) */
static void sc_row(uint16_t *dst, const uint8_t *src, uint32_t pr, uint32_t odd, uint32_t n)
{
    const uint16_t *pal = spal[odd];
    int32_t o = sgl[pr], x, sp = (int32_t)(n / 60u), last = (int32_t)n - 1;
    if (!o) {
        if (n == 120u) for (x = 0; x < 120; x++) dst[2 * x] = dst[2 * x + 1] = pal[src[x]];
        else for (x = 0; x < 240; x++) dst[x] = pal[src[x]];
        return;
    }
    for (x = 0; x < (int32_t)n; x++) {                  /* torn: the row slides, red one way and blue the other */
        int32_t xr = x + o + sp, xg = x + o, xb = x + o - sp;
        uint32_t cr = swap16(pal[src[xr < 0 ? 0 : xr > last ? last : xr]]), cg = swap16(pal[src[xg < 0 ? 0 : xg > last ? last : xg]]),
                 cb = swap16(pal[src[xb < 0 ? 0 : xb > last ? last : xb]]);
        uint16_t c = swap16((cr & 0xF800u) | (cg & 0x07E0u) | (cb & 0x001Fu));
        if (n == 120u) dst[2 * x] = dst[2 * x + 1] = c; else dst[x] = c;
    }
}

/* ---- drawing on top, in screen coordinates (the canvas is one strip: cv_oy moves it) */
static void gline(int32_t x0, int32_t y0, int32_t x1, int32_t y1)       /* a line with a glow */
{
    cv_line(x0, y0 - 1, x1, y1 - 1, sc_glow);
    cv_line(x0, y0 + 1, x1, y1 + 1, sc_glow);
    cv_line(x0, y0, x1, y1, sc_core);
}
static void centered(int32_t y, const char *s, uint16_t c) { cv_text(120 - text_w(&FONT_S, s) / 2, y, &FONT_S, s, c); }

static void ov_osc(void)                        /* the oscillators: saw, pulse and sub as they sum; the detuned one beats behind */
{
    int32_t x, py = 0, py2 = 0, mb = P[P_MIX], ma = 127 - mb, thr = (int32_t)P[P_PW] * 2, sub = P[P_SUB], det = P[P_DET];
    uint32_t sh = fm1_ms >> 4;
    for (x = 0; x < 240; x++) {
        uint32_t pf = ((uint32_t)x * 273u + sh * 3u) >> 7, pf2 = pf + (pf * (uint32_t)det >> 9);
        int32_t ph = (int32_t)(pf & 255u), saw = ph - 128, pul = ph < thr ? 127 : -128, sb = ((pf >> 8) & 1u) ? 127 : -128, s, y, y2;
        s = (((saw * ma + pul * mb) >> 7) * 3 >> 2) + ((sb * sub) >> 9);
        y = 112 - ((s * 64) >> 7);
        y2 = 112 - (((int32_t)(pf2 & 255u) - 128) * 40 >> 7);
        if (x) {
            if (det) cv_line(x - 1, py2, x, y2, sc_dim);
            gline(x - 1, py, x, y);
        }
        py = y; py2 = y2;
    }
}

static void ov_flt(void)                        /* the filter's curve: the cutoff, the resonance peak, the 12 dB slope */
{
    int32_t x, xc = (int32_t)P[P_CUT] * 2 * 239 / 255, rh = (int32_t)P[P_RES] * 60 / 127, py = 0;
    for (x = 0; x < 240; x++) {
        int32_t u = x - xc, att = u > 0 ? u * u * 3 / (u + 14) : 0, pk = rh * 256 / (256 + u * u * 6), y = 140 - pk + att;
        y = y > 196 ? 196 : y;
        if (!(x & 1)) cv_rect(x, y + 3, 1, 196 - y, sc_dim);
        if (x) gline(x - 1, py, x, y);
        py = y;
    }
    cv_rect(xc, 26, 1, 24, sc_core);
    cv_rect(xc - 1, 26, 3, 3, sc_core);
}

static int32_t env_y(int32_t x, const int32_t *xp, int32_t ytop, int32_t ys, int32_t yb)
{
    if (x <= xp[1]) return yb - (yb - ytop) * (x - xp[0]) / (xp[1] - xp[0] ? xp[1] - xp[0] : 1);
    if (x <= xp[2]) return ytop + (ys - ytop) * (x - xp[1]) / (xp[2] - xp[1] ? xp[2] - xp[1] : 1);
    if (x <= xp[3]) return ys;
    return ys + (yb - ys) * (x - xp[3]) / (xp[4] - xp[3] ? xp[4] - xp[3] : 1);
}
static void ov_env(void)                        /* the amplitude envelope, with a light travelling along it */
{
    int32_t w[4] = {8 + P[P_ATK] * 7 / 10, 8 + P[P_DEC] * 6 / 10, 34, 8 + P[P_REL] * 7 / 10}, tot = w[0] + w[1] + w[2] + w[3], xp[5], i, x, py = 0;
    int32_t ytop = 56, yb = 192, ys = yb - (int32_t)P[P_SUS] * 136 / 127, pos;
    if (tot > 220) for (i = 0; i < 4; i++) w[i] = w[i] * 220 / tot;
    xp[0] = 10;
    for (i = 0; i < 4; i++) xp[i + 1] = xp[i] + w[i];
    for (x = xp[0]; x <= xp[4]; x++) {
        int32_t y = env_y(x, xp, ytop, ys, yb);
        if (!(x & 1)) cv_rect(x, y + 3, 1, yb - y - 3, sc_dim);
        if (x > xp[0]) gline(x - 1, py, x, y);
        py = y;
    }
    for (i = 1; i < 4; i++) cv_rect(xp[i], ytop - 6, 1, yb - ytop + 6, sc_dim);
    cv_rect(0, yb + 1, 240, 1, sc_dim);
    pos = (int32_t)(fm1_ms / 7u % (uint32_t)(xp[4] - xp[0] + 50));
    if (pos <= xp[4] - xp[0]) { int32_t y = env_y(xp[0] + pos, xp, ytop, ys, yb); cv_rect(xp[0] + pos - 2, y - 2, 5, 5, C_WHITE); }
}

static void ov_lfo(void)                        /* the LFO: its shape, depth, and how many cycles fit */
{
    int32_t x, py = 0, cyc = 240 - (int32_t)P[P_LRATE] * 17 / 10, amp = (int32_t)P[P_LDEPTH] * 84 / 127, dp = 65536 / (cyc < 20 ? 20 : cyc);
    uint32_t shape = (uint32_t)P[P_LSHAPE] >> 5;
    static const char *const DST[3] = {"CUTOFF", "PITCH", "PWM"};
    cv_rect(0, 112, 240, 1, sc_dim);
    for (x = 0; x < 240; x++) {
        uint32_t p = ((uint32_t)x * (uint32_t)dp + sc_phase) & 0xFFFFu;
        int32_t v, y;
        if (shape == 0u) v = p < 32768u ? (int32_t)p / 128 - 128 : 384 - (int32_t)p / 128;      /* triangle */
        else if (shape == 1u) v = (int32_t)p / 256 - 128;                                         /* saw */
        else if (shape == 2u) v = p < 32768u ? 127 : -127;                                        /* square */
        else v = (int32_t)(((p >> 11) * 2654435761u >> 24) & 255u) - 128;                          /* sample and hold */
        y = 112 - (v * amp >> 7);
        if (x) { if (shape >= 2u && y != py) cv_line(x, py, x, y, sc_core); gline(x - 1, py, x, y); }
        py = y;
    }
    cv_text(8, 24, &FONT_S, DST[(uint32_t)P[P_LDEST] * 3u >> 7], sc_core);
}

static void ov_fx(void)                         /* the echoes: ping-pong taps that die away; DLY MIX sets the first one's height */
{
    int32_t k, x, h = 20 + (int32_t)P[P_DMIX] * 58 / 127, sp = 10 + (int32_t)P[P_DTIME] * 18 / 127, ratio = (int32_t)(((uint32_t)P[P_DFB] * 113u) >> 7);
    cv_rect(0, 112, 240, 1, sc_dim);
    cv_rect(14, 112 - 78, 7, 156, C_WHITE);     /* the dry sound */
    if (!P[P_DMIX]) return;
    for (k = 1; k < 40; k++) {
        x = 14 + k * sp;
        if (x > 230 || h < 3) break;
        if (k & 1) { cv_rect(x, 112 - h, 6, h, sc_core); cv_rect(x + 1, 112 - h - 2, 4, 1, sc_glow); }       /* left, above */
        else { cv_rect(x, 113, 6, h, sc_glow); cv_rect(x + 1, 113 + h + 1, 4, 1, sc_dim); }                  /* right, below */
        h = h * ratio / 128;
    }
}

static void ov_glo(void)                        /* a radar: a sweeping arm, a blip for every sounding voice */
{
    uint32_t a = (fm1_ms >> 2) & 255u, v;
    int32_t i;
    for (i = 6; i >= 0; i--) {                  /* the arm and its trail */
        uint32_t aa = (a - (uint32_t)i * 3u) & 255u;
        cv_line(120, 112, 120 + ((SIN8[(aa + 64u) & 255u] * 100) >> 7), 112 + ((SIN8[aa] * 100) >> 7), i ? (i > 3 ? sc_dim : sc_glow) : sc_core);
    }
    for (v = 0; v < NV; v++)
        if (voice[v].gate || voice[v].stage != ST_OFF) {
            uint32_t an2 = ((uint32_t)voice[v].note * 7u) & 255u;
            int32_t r = 26 + ((int32_t)voice[v].vel * 60 >> 7), bx = 120 + ((SIN8[(an2 + 64u) & 255u] * r) >> 7), by = 112 + ((SIN8[an2] * r) >> 7);
            cv_rect(bx - 2, by - 2, 5, 5, voice[v].gate ? C_WHITE : sc_core);
        }
}

static void ov_arp(void)                        /* the chord as a ring of lights; the note that is sounding burns */
{
    uint32_t n = an, i, rot = fm1_ms >> 5;
    int32_t px = 0, py = 0;
    if (!n || !arp_cmd) { if ((fm1_ms >> 9) & 1u) centered(104, arp_cmd ? "HOLD KEYS" : "ARP OFF", sc_core); return; }
    for (i = 0; i <= n; i++) {
        uint32_t a = (i * 256u / n + rot) & 255u, k = i % n;
        int32_t x = 120 + ((SIN8[(a + 64u) & 255u] * 72) >> 7), y = 112 + ((SIN8[a] * 72) >> 7);
        if (i) cv_line(px, py, x, y, sc_dim);
        if (i < n) {
            if (alist[k] == arp_cur) { cv_line(120, 112, x, y, sc_glow); cv_rect(x - 4, y - 4, 9, 9, C_WHITE); }
            else cv_rect(x - 2, y - 2, 5, 5, sc_core);
        }
        px = x; py = y;
    }
}

static void ov_seq(void)                        /* the pattern as a skyline against the sun */
{
    uint32_t n = seq_len, s, mn = 255, mx = 0, w, x0, curs = playing ? seq_step : 255u, rc = rec_arm && !playing ? rec_cursor : 255u;
    if (n < 1u) n = 1u;
    if (n > 32u) n = 32u;
    w = 224u / n;
    x0 = (240u - n * w) / 2u;
    for (s = 0; s < n; s++) { uint32_t v = pat[s] & 127u; if (v) { if (v < mn) mn = v; if (v > mx) mx = v; } }
    for (s = 0; s < n; s++) {
        uint32_t v = pat[s] & 127u;
        int32_t x = (int32_t)(x0 + s * w), h = v ? (mx > mn ? 10 + (int32_t)((v - mn) * 56u / (mx - mn)) : 34) : 2;
        uint16_t c = s == curs ? C_WHITE : s == rc ? C_MG : (pat[s] & 128u) ? C_YE : v ? sc_core : sc_dim;
        if (s == curs) cv_rect(x - 2, 136 - h - 4, (int32_t)w + 3, h + 8, sc_glow);
        cv_rect(x, 136 - h, (int32_t)w - 1, h, c);
    }
}

static void (*const OVERLAY[8])(void) = {ov_osc, ov_flt, ov_env, ov_lfo, ov_fx, ov_glo, ov_arp, ov_seq};

/* ---- the top line and the four knobs at the bottom */
static const char *const SHORT[NP] = {
    "MIX", "PULSE", "DETUNE", "SUB",  "CUTOFF", "RESO", "ENV", "DRIVE",  "ATK", "DEC", "SUS", "REL",  "RATE", "DEPTH", "DEST", "SHAPE",
    "CRUSH", "MIX", "TIME", "FDBK",  "GLIDE", "MODE", "TUNE", "VELO",  "MODE", "OCTS", "RATE", "GATE",  "BPM", "SWING", "LEN", "XPOSE"};

static void hud_top(uint32_t pg)
{
    char s[24], bpm[8];
    int32_t x, rx;
    uint32_t j = 0;
    const char *arp = arp_cmd == 2u ? "HLD" : "ARP", *p;
    cv_rect(0, 0, 240, 16, BG);
    cv_rect(0, 1, 3, 14, rgb565v(40u + (sc_kick >> 1), 0, 120u + (sc_kick >> 1)));          /* the left edge lights with every note */
    itoa3(60 + P[P_BPM], bpm);
    rx = 236 - text_w(&FONT_S, bpm) - (arp_cmd ? text_w(&FONT_S, arp) + 8 : 0) - (playing || want_play ? 17 : 0)
        - (rec_arm ? text_w(&FONT_S, "REC") + 8 : 0);
    if (sc_touch) {                             /* the knob, and what it is now: where the eye already is */
        uint32_t idx = pg * 4u + sc_touch - 1u;
        p = SHORT[idx];
        while (*p) s[j++] = *p++;
        s[j++] = ' ';
        fmt_val(idx, s + j);
        cv_text(8, 0, &FONT_S, s, C_YE);
    } else if (sc_msg) cv_text(8, 0, &FONT_S, sc_msg, C_MG);
    else {
        x = cv_text(8, 0, &FONT_S, TAB[pg], C_CY) + 8;
        p = PNAME[cur];
        while (*p && j < 20u) s[j++] = *p++;
        if (edited) s[j++] = '*';
        s[j] = 0;
        if (x + text_w(&FONT_S, s) < rx) cv_text(x, 0, &FONT_S, s, C_PU);
    }
    x = cv_text(rx, 0, &FONT_S, bpm, C_TX) + 8;
    if (arp_cmd) x = cv_text(x, 0, &FONT_S, arp, C_CY) + 8;
    if (playing || want_play) x = cv_text(x, 0, &FONT_S, ">", C_CY) + 8;
    if (rec_arm) cv_text(x, 0, &FONT_S, "REC", C_MG);
}

static void hud_bottom(uint32_t pg)             /* the four knobs of the page, as segments */
{
    uint32_t k;
    cv_rect(0, 210, 240, 30, BG);
    for (k = 0; k < 4u; k++) {
        int32_t x = (int32_t)k * 60 + 4;
        uint32_t hot = sc_touch == k + 1u, v = P[pg * 4u + k], i, lit = (v * 10u + 126u) / 127u;
        cv_text(x, 214, &FONT_S, SHORT[pg * 4u + k], hot ? C_YE : C_CY);
        for (i = 0; i < 10u; i++) cv_rect(x + (int32_t)i * 5, 230, 4, 5, i < lit ? (hot ? C_YE : (k & 1u ? C_MG : C_CY)) : C_GR);
    }
}

/* ---- one strip (30 rows) of the picture; call it every pass of the main loop. boot: the loading screen's text on top. */
static void sc_boot_overlay(uint32_t e, uint32_t p, const char *stage)
{
    int32_t lx = 120 - text_w(&FONT_L, "CYBERDECK") / 2, jx = 0, w;
    uint32_t x;
    if (sc_gl > 8u) jx = (int32_t)(sc_rand() % (sc_gl >> 3u)) - (int32_t)(sc_gl >> 4u);
    cv_text(lx + 5 + jx, 10, &FONT_L, "CYBERDECK", C_MG);                 /* chromatic aberration: magenta, cyan, white on top */
    cv_text(lx - 5 - jx, 10, &FONT_L, "CYBERDECK", C_CY);
    cv_text(lx, 10, &FONT_L, "CYBERDECK", C_WHITE);
    centered(52, "NEON SYNTH FOR THE FM-1", C_YE);
    cv_rect(20, 206, 200, 30, BG);
    centered(208, stage, C_TX);
    cv_rect(20, 224, 200, 10, C_PU);
    cv_rect(21, 225, 198, 8, BG);
    w = (int32_t)(p * 196u / 100u);
    for (x = 0; x < (uint32_t)w; x++) cv_rect(22 + (int32_t)x, 226, 1, 6, rgb565v(255u - x, 40u + x, 200u - (x >> 1)));
    (void)e;
}

static void sc_step(uint32_t id, uint32_t e, uint32_t prog, const char *stage)
{
    uint32_t lr, boot = id == SC_BOOT, n = boot ? 240u : 120u, rows = boot ? 30u : 15u, pr0;
    if (sc_strip_i == 0) sc_frame(id);
    lcd_sync();                                 /* the last strip may still be on its way out of cv_px */
    cv_w = 240; cv_h = 30;
    pr0 = sc_strip_i * rows;
    for (lr = 0; lr < rows; lr++) {
        uint32_t pr = pr0 + lr;
        const uint8_t *src = PLATE[id] + (uint32_t)srow[pr] * n;
        if (boot) sc_row(cv_px + lr * 240u, src, pr, pr & 1u, n);
        else {
            uint16_t *d = cv_px + (lr * 2u) * 240u;
            sc_row(d, src, pr, 0, n);
            sc_row(d + 240, src, pr, 1, n);       /* the second row of each pair is the scan line */
        }
    }
    cv_oy = -(int32_t)(sc_strip_i * 30u);
    if (boot) sc_boot_overlay(e, prog, stage);
    else {
        if (sc_strip_i <= 6u) OVERLAY[id]();
        if (sc_strip_i == 0u) hud_top(id);
        if (sc_strip_i == 7u) hud_bottom(id);
    }
    cv_oy = 0;
    cv_blit(0, sc_strip_i * 30u);
    sc_strip_i = (sc_strip_i + 1u) & 7u;
}
