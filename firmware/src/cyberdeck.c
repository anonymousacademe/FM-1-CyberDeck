/* SPDX-License-Identifier: GPL-3.0-only */
/* CYBERDECK: a four-voice subtractive synth for the M-VAVE FM-1, neon and rain.
 * Built on Dinghy (zvenson), the platform is Felucca (Leo Kuroshita, Hugelton Instruments) and SLOOP (isod89).
 *
 *   voice     saw + detuned pulse (PWM) + sub square -> resonant state-variable filter -> drive -> amp envelope
 *   filter    own decay envelope (shares DECAY), 50 % key tracking, velocity sensitivity
 *   LFO       tri / saw / square / sample&hold -> cutoff, pitch or pulse width
 *   FX        bit crusher, dark ping-pong delay (tape-style time smearing)
 *   modes     poly (4 voices) or mono-legato with glide
 *
 *   EDIT SEL ENV LFO FX GLO   the OSC FLT ENV LFO FX GLO pages, four knobs each (KNOB1..4)
 *   ALGORITHM / SELECT        cutoff / resonance, on every page
 *   PRESETS                   the 8 patches;  SAVE keeps your edits in that slot (flash)
 *   HOME                      panic;  OCT- OCT+ octave;  MASTER volume (platform)
 *   USB-MIDI in               notes (velocity), CC 74 cutoff, 71 reso, 73 attack, 72 release, 1 LFO depth, 91 delay mix
 *
 * Integers only: no float, no 64-bit division (docs/RULES.md). tests/host.c runs this file on a computer. */
#include "platform.c"
#include "tables.h"

#ifndef FM1_TICKS_PER_US
#define FM1_TICKS_PER_US 24u
#endif

/* ---- parameters: 6 pages x 4 knobs, each 0..127 */
enum { P_MIX, P_PW, P_DET, P_SUB,
       P_CUT, P_RES, P_ENVAMT, P_DRV,
       P_ATK, P_DEC, P_SUS, P_REL,
       P_LRATE, P_LDEPTH, P_LDEST, P_LSHAPE,
       P_CRUSH, P_DMIX, P_DTIME, P_DFB,
       P_GLIDE, P_MODE, P_TUNE, P_VELO,
       NP };
static volatile uint8_t P[NP];

#define NPRESET 8u
static const uint8_t FACTORY[NPRESET][NP] = {
    /*        MIX PW DET SUB | CUT RES ENV DRV | ATK DEC SUS REL | LRT LDP LDS LSH | CRU DMX DTM DFB | GLD MOD TUN VEL */
    /* NEON RAIN   */ { 50, 64, 20, 30,   70, 30, 76, 20,   85, 90, 80,100,   25, 30, 10,  0,    0, 50, 70, 60,    0,  0, 64, 40},
    /* CHROME BASS */ { 20, 50, 10,100,   42, 70,110, 60,    0, 62, 20, 40,    0,  0, 10,  0,    0,  0, 60, 20,   40,100, 64, 60},
    /* KANEDA      */ { 60, 40, 35, 20,   80, 40, 90, 50,    2, 70, 90, 50,   90, 25, 64,  0,    0, 40, 80, 55,   20,100, 64, 50},
    /* ZAIBATSU    */ { 64, 90, 12, 80,   36, 90,100, 80,   30,100, 40,105,   20, 70, 10,  0,   20, 70,100, 80,    0,  0, 64, 30},
    /* GLITCH ICE  */ { 80, 30,  5,  0,   60, 80,110, 40,    0, 50,  0, 40,  105, 60, 10,100,   70, 60, 40, 70,    0,  0, 64, 50},
    /* TYRELL      */ { 40, 64, 25, 40,   55, 25,100, 30,   55, 80,100, 70,   85,  8, 64,  0,    0, 45, 95, 55,    0,  0, 64, 50},
    /* WIRED       */ { 30, 64, 40, 20,   50, 60,105, 25,    0, 58,  0, 50,    0,  0, 10,  0,    0, 55, 60, 65,    0,  0, 64, 60},
    /* DATA MOSH   */ {100, 70, 90, 70,   45,100, 80,100,    0, 90, 90, 30,   70, 90, 10, 64,   90, 30, 50, 40,   30,100, 64, 40},
};
static const char *const PNAME[NPRESET] = {"NEON RAIN", "CHROME BASS", "KANEDA", "ZAIBATSU",
                                           "GLITCH ICE", "TYRELL", "WIRED", "DATA MOSH"};
static const char *const LABEL[NP] = {
    "MIX", "PULSE W", "DETUNE", "SUB",
    "CUTOFF", "RESO", "ENV AMT", "DRIVE",
    "ATTACK", "DECAY", "SUSTAIN", "RELEASE",
    "RATE", "DEPTH", "DEST", "SHAPE",
    "CRUSH", "DLY MIX", "DLY TIME", "FEEDBACK",
    "GLIDE", "MODE", "TUNE", "VELOCITY"};
static const char *const TAB[6] = {"OSC", "FLT", "ENV", "LFO", "FX", "GLO"};

/* ---- the sound (runs in the audio ISR: keep it short, no flash, no screen) */
#define NV 4u
#define ONE (1 << 23)                           /* envelope 1.0 (Q23) */
enum { ST_OFF, ST_ATK, ST_DEC, ST_REL };

typedef struct {
    volatile uint32_t inc_t;                    /* target phase step (main -> ISR) */
    volatile uint8_t gate, trig, vel, note;
    volatile uint8_t stage;                     /* ISR writes, main reads */
    uint8_t seen;
    uint32_t age, inc, pa, pb, ps;              /* phase steps / accumulators */
    int32_t env, fenv, ic1, ic2;
} voice_t;
static voice_t voice[NV];

#define SCOPE_N 2048u
static int16_t scope[SCOPE_N];
static volatile uint32_t scope_w, cpu_pct;

#define DLEN 32768u
static int16_t dlL[DLEN] __attribute__((section(".pool")));
static int16_t dlR[DLEN] __attribute__((section(".pool")));

static inline int32_t sclip(int32_t v)          /* soft clip, 1.5v - 0.5v^3 */
{
    int32_t t;
    if (v > 32767) return 32767;
    if (v < -32767) return -32767;
    t = (v * v) >> 15;
    return (v * ((98304 - t) >> 1)) >> 15;
}

static void app_audio(int32_t *out, uint32_t n)
{
    static uint32_t lph, rnd = 0x1234567u, dw, crush_n;
    static int32_t lfo, sh, dt_cur = 20000, lpL, lpR, held;
    static uint32_t cpu_max, cpu_blk;
    uint32_t t0 = fm1_ticks(), b, v, i;

    for (b = 0; b < n; b += 16u) {
        int32_t mono[16], m, dtt;
        uint32_t prev_lph = lph, shape = P[P_LSHAPE] >> 5, dest = (uint32_t)P[P_LDEST] * 3u >> 7;
        uint32_t mono_mode = P[P_MODE] >= 64u;
        int32_t ldep = P[P_LDEPTH];

        /* LFO, once per tick */
        lph += LFO_INC[P[P_LRATE]];
        if (lph < prev_lph) { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; sh = (int32_t)(rnd >> 16) - 32768; }
        {
            int32_t s = (int32_t)lph >> 16;
            lfo = shape == 0u ? ((s < 0 ? -s : s) << 1) - 32767 : shape == 1u ? s : shape == 2u ? (s < 0 ? -32767 : 32767) : sh;
            lfo = (lfo * ldep) >> 7;            /* depth applied: +-32512 */
        }

        for (i = 0; i < 16u; i++) mono[i] = 0;

        for (v = 0; v < NV; v++) {
            voice_t *V = &voice[v];
            uint32_t st = V->stage, inc, incb, pa, pb, ps, wph, a1, a2, a3, g, k, gk, den;
            int32_t env, ei, de, fenv, ic1, ic2, velg, ci, pwq, dg, sublvl, mixa, mixb;
            uint32_t gate = V->gate;

            if (V->trig != V->seen) {           /* a new note */
                V->seen = V->trig;
                if (st == ST_OFF) { V->pa = V->pb = V->ps = 0; V->ic1 = V->ic2 = 0; }
                st = ST_ATK;
                V->fenv = ONE;
                if (!mono_mode || P[P_GLIDE] == 0u) V->inc = V->inc_t;
            }
            if (st == ST_OFF) continue;
            if (!gate && (st == ST_ATK || st == ST_DEC)) st = ST_REL;

            /* control rate: glide, envelopes */
            inc = V->inc;
            if (mono_mode && P[P_GLIDE])
                inc = (uint32_t)((int32_t)inc + (int32_t)(((int64_t)((int32_t)(V->inc_t - inc)) * GLIDE_R[P[P_GLIDE]]) >> 16));
            else
                inc = V->inc_t;
            V->inc = inc;

            env = V->env;
            {
                int32_t pe = env, sus = (int32_t)P[P_SUS] * 66052;
                if (st == ST_ATK) {
                    env += (int32_t)(((int64_t)(ONE + ONE / 3 - env) * RATE16[P[P_ATK]]) >> 24);
                    if (env >= ONE) { env = ONE; st = ST_DEC; }
                } else if (st == ST_DEC) {
                    env += (int32_t)(((int64_t)(sus - env) * RATE16[P[P_DEC]]) >> 24);
                    if (sus < (1 << 12) && env < (1 << 12)) st = ST_OFF;
                } else {
                    int32_t d = (int32_t)(((int64_t)env * RATE16[P[P_REL]]) >> 24);
                    env -= d < 1 ? 1 : d;
                    if (env < (1 << 12)) { env = 0; st = ST_OFF; }
                }
                ei = pe;
                de = (env - pe) >> 4;
            }
            V->env = env;
            V->stage = (uint8_t)st;
            fenv = V->fenv;
            fenv -= (int32_t)(((int64_t)fenv * RATE16[P[P_DEC]]) >> 24);
            V->fenv = fenv;

            /* oscillator steps with the LFO */
            if (dest == 1u) inc += (int32_t)(((int64_t)inc * lfo) >> 18);
            incb = inc + (uint32_t)(((uint64_t)inc * P[P_DET] * 26u) >> 16);
            pwq = ((int32_t)P[P_PW] - 64) * 3;
            if (dest == 2u) pwq += lfo >> 8;
            pwq = pwq > 240 ? 240 : pwq < -240 ? -240 : pwq;
            wph = 0x80000000u + (uint32_t)(pwq * (1 << 23));
            mixb = P[P_MIX]; mixa = 127 - mixb;
            sublvl = P[P_SUB];

            /* filter coefficients (Simper SVF) */
            ci = (int32_t)P[P_CUT] * 2 + (int32_t)(((int64_t)(fenv >> 8) * ((int32_t)P[P_ENVAMT] - 64)) >> 14) + ((int32_t)V->note - 60);
            if (dest == 0u) ci += (lfo * 96) >> 15;
            ci = ci < 0 ? 0 : ci > 255 ? 255 : ci;
            g = CUT_G[ci];
            k = 131072u - (uint32_t)P[P_RES] * 980u;
            gk = (uint32_t)(((uint64_t)g * (g + k)) >> 16);
            den = 65536u + gk;
            a1 = 0xFFFFFFFFu / den;
            if (a1 > 65535u) a1 = 65535u;
            a2 = (uint32_t)(((uint64_t)g * a1) >> 16);
            a3 = (uint32_t)(((uint64_t)g * a2) >> 16);
            dg = 256 + (int32_t)P[P_DRV] * 16;
            velg = 32767 - ((127 - (int32_t)V->vel) * (int32_t)P[P_VELO] * 2);

            pa = V->pa; pb = V->pb; ps = V->ps; ic1 = V->ic1; ic2 = V->ic2;
            for (i = 0; i < 16u; i++) {
                int32_t x, v3, v1, v2, y, sa, sb, ss, amp;
                pa += inc; pb += incb; ps += inc >> 1;
                sa = (int32_t)(pa >> 16) - 32768;
                sb = pb < wph ? 32767 : -32767;
                ss = (ps >> 31) ? 32767 : -32767;
                x = (((sa * mixa + sb * mixb) >> 7) * 22 >> 5) + ((ss * sublvl) >> 9);

                v3 = x - ic2;
                v1 = (int32_t)(((int64_t)a1 * ic1 + (int64_t)a2 * v3) >> 16);
                v2 = ic2 + (int32_t)(((int64_t)a2 * ic1 + (int64_t)a3 * v3) >> 16);
                ic1 = 2 * v1 - ic1;
                ic2 = 2 * v2 - ic2;
                ic1 = ic1 > (1 << 20) ? (1 << 20) : ic1 < -(1 << 20) ? -(1 << 20) : ic1;
                ic2 = ic2 > (1 << 20) ? (1 << 20) : ic2 < -(1 << 20) ? -(1 << 20) : ic2;

                y = sclip((v2 * dg) >> 8);
                ei += de;
                amp = ((ei >> 8) * velg) >> 15;
                mono[i] += (y * amp) >> 15;
            }
            V->pa = pa; V->pb = pb; V->ps = ps; V->ic1 = ic1; V->ic2 = ic2;
        }

        /* bit crusher, ping-pong delay, out */
        dtt = 1500 + (int32_t)P[P_DTIME] * 245;
        {
            int32_t d = dtt - dt_cur;
            d = d >> 6;
            if (d == 0 && dtt != dt_cur) d = dtt > dt_cur ? 1 : -1;
            dt_cur += d;
        }
        for (i = 0; i < 16u; i++) {
            int32_t dry, yL, yR, fL, fR, L, R, wd;
            uint32_t rd;
            m = mono[i] >> 1;
            if (P[P_CRUSH]) {
                uint32_t bits = 16u - ((uint32_t)P[P_CRUSH] * 12u >> 7), hold = 1u + ((uint32_t)P[P_CRUSH] * 10u >> 7);
                if (++crush_n >= hold) { crush_n = 0; held = m & ~((1 << (16u - bits)) - 1); }
                m = held;
            }
            dry = m;
            rd = (dw - (uint32_t)dt_cur) & (DLEN - 1u);
            yL = dlL[rd]; yR = dlR[rd];
            lpL += (yL - lpL) >> 2; lpR += (yR - lpR) >> 2;
            fL = (lpL * (int32_t)((P[P_DFB] * 113u) >> 7)) >> 7;
            fR = (lpR * (int32_t)((P[P_DFB] * 113u) >> 7)) >> 7;
            wd = dry + fR;                      /* ping-pong: input and the right repeat feed the left, left feeds right */
            dlL[dw] = (int16_t)(wd > 32767 ? 32767 : wd < -32767 ? -32767 : wd);
            dlR[dw] = (int16_t)(fL > 32767 ? 32767 : fL < -32767 ? -32767 : fL);
            dw = (dw + 1u) & (DLEN - 1u);
            L = sclip(dry + ((yL * (int32_t)P[P_DMIX]) >> 7));
            R = sclip(dry + ((yR * (int32_t)P[P_DMIX]) >> 7));
            out[2u * (b + i)] += L;
            out[2u * (b + i) + 1u] += R;
            scope[scope_w++ & (SCOPE_N - 1u)] = (int16_t)((L + R) >> 1);
        }
    }

    {   /* CPU: the worst block of the last 64, in percent of 256 frames (5805 us) */
        uint32_t dt = fm1_ticks() - t0;
        if (dt > cpu_max) cpu_max = dt;
        if (++cpu_blk >= 64u) { cpu_pct = cpu_max / (5805u * FM1_TICKS_PER_US / 100u); cpu_max = 0; cpu_blk = 0; }
    }
}

/* ---- notes (main loop) */
static const uint32_t INC_C9[12] = {            /* MIDI 120..131; lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t ages;
static uint8_t stk[8], nstk;

static uint32_t note_inc(uint32_t note)
{
    uint32_t inc = INC_C9[note % 12u] >> (10u - note / 12u);
    int32_t tune = (int32_t)P[P_TUNE] - 64;
    return inc + (uint32_t)(int32_t)(((int64_t)inc * (tune * 28)) >> 16);
}
static int is_mono(void) { return P[P_MODE] >= 64u; }

static void note_on(uint32_t note, uint32_t vel)
{
    uint32_t v, pick = 0, best = 0xFFFFFFFFu, i, j;
    if (note > 127u || !vel) return;
    if (is_mono()) {
        for (i = j = 0; i < nstk; i++) if (stk[i] != note) stk[j++] = stk[i];
        nstk = (uint8_t)j;
        if (nstk < 8u) stk[nstk++] = (uint8_t)note;
        voice[0].note = (uint8_t)note;
        voice[0].vel = (uint8_t)vel;
        voice[0].inc_t = note_inc(note);
        if (!voice[0].gate) voice[0].trig++;    /* legato: no retrigger while a key is held */
        voice[0].gate = 1;
        return;
    }
    for (v = 0; v < NV; v++) {                  /* same note, else a free voice, else a releasing one, else the oldest */
        uint32_t score = voice[v].note == note && voice[v].gate ? 0u
                       : voice[v].stage == ST_OFF ? 1u
                       : !voice[v].gate ? 2u + ((uint32_t)voice[v].env >> 12)
                       : 0x80000000u + voice[v].age;
        if (score < best) { best = score; pick = v; }
    }
    voice[pick].note = (uint8_t)note;
    voice[pick].vel = (uint8_t)vel;
    voice[pick].age = ++ages;
    voice[pick].inc_t = note_inc(note);
    voice[pick].gate = 1;
    voice[pick].trig++;
}
static void note_off(uint32_t note)
{
    uint32_t v, i, j;
    if (is_mono()) {
        for (i = j = 0; i < nstk; i++) if (stk[i] != note) stk[j++] = stk[i];
        nstk = (uint8_t)j;
        if (nstk) {                             /* back to the previous held key, legato */
            voice[0].note = stk[nstk - 1u];
            voice[0].inc_t = note_inc(stk[nstk - 1u]);
        } else
            voice[0].gate = 0;
        return;
    }
    for (v = 0; v < NV; v++)
        if (voice[v].note == note) voice[v].gate = 0;
}
static void panic(void)
{
    uint32_t v;
    nstk = 0;
    for (v = 0; v < NV; v++) voice[v].gate = 0;
}
static int quiet(void)
{
    uint32_t v;
    for (v = 0; v < NV; v++) if (voice[v].stage != ST_OFF) return 0;
    return 1;
}

/* ---- settings: the patch slots and the octave */
#define SET_MAGIC 0xC7u
static struct { uint8_t magic, cur; int8_t oct, pad; uint8_t slot[NPRESET][NP]; } set;

static uint32_t cur;                            /* current preset */
static uint32_t edited;                         /* edited since loaded */

static void load_preset(uint32_t n)
{
    uint32_t i;
    panic();
    cur = n;
    for (i = 0; i < NP; i++) P[i] = set.slot[n][i];
    edited = 0;
}
static void set_param(uint32_t idx, int32_t val)
{
    val = val < 0 ? 0 : val > 127 ? 127 : val;
    if (P[idx] != val) { P[idx] = (uint8_t)val; edited = 1; }
}

/* ---- the screen: a neon HUD, the live scope, four knobs per page */
#define BG RGB(6, 4, 14)
#define C_CY RGB(0, 235, 255)
#define C_MG RGB(255, 0, 160)
#define C_YE RGB(255, 225, 0)
#define C_PU RGB(60, 38, 100)
#define C_GR RGB(24, 16, 48)
#define C_TX RGB(200, 200, 230)
#define KEY0 53u

static void itoa3(int32_t v, char *s)
{
    uint32_t i = 0, u = (uint32_t)(v < 0 ? -v : v);
    char t[4];
    uint32_t n = 0;
    if (v < 0) s[i++] = '-';
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (n) s[i++] = t[--n];
    s[i] = 0;
}
static void fmt_val(uint32_t idx, char *s)
{
    static const char *const SHP[4] = {"TRI", "SAW", "SQR", "S&H"}, *const DST[3] = {"CUTOFF", "PITCH", "PWM"};
    const char *p = 0;
    uint32_t v = P[idx];
    if (idx == P_LSHAPE) p = SHP[v >> 5];
    else if (idx == P_LDEST) p = DST[v * 3u >> 7];
    else if (idx == P_MODE) p = v >= 64u ? "MONO" : "POLY";
    if (p) { do { *s++ = *p; } while (*p++); return; }
    itoa3(idx == P_ENVAMT || idx == P_TUNE ? (int32_t)v - 64 : (int32_t)v, s);
}

static void draw_title(void)
{
    char s[24];
    uint32_t n = 0;
    const char *p = PNAME[cur];
    cv_begin(240, 56, BG);
    cv_text(10, 0, &FONT_L, "CYBERDECK", C_MG);      /* chromatic aberration: magenta, cyan, then white on top */
    cv_text(6, 0, &FONT_L, "CYBERDECK", C_CY);
    cv_text(8, 0, &FONT_L, "CYBERDECK", C_WHITE);
    s[n++] = (char)('1' + cur); s[n++] = ' ';
    while (*p && n < 20u) s[n++] = *p++;
    if (edited) s[n++] = '*';
    s[n] = 0;
    cv_rect(0, 36, 240, 1, C_PU);
    cv_text(8, 38, &FONT_S, s, C_YE);
    cv_blit(0, 0);
}

static void draw_scope(void)
{
    uint32_t end = scope_w, a = end - 1500u, i, start = a;
    int32_t x, py = 32, y;
    cv_begin(240, 64, BG);
    for (x = 0; x < 240; x += 30) cv_rect(x, 0, 1, 64, C_GR);
    cv_rect(0, 32, 240, 1, C_GR);
    cv_rect(0, 0, 14, 1, C_MG); cv_rect(0, 0, 1, 8, C_MG); cv_rect(226, 0, 14, 1, C_MG); cv_rect(239, 0, 1, 8, C_MG);
    cv_rect(0, 63, 14, 1, C_MG); cv_rect(0, 56, 1, 8, C_MG); cv_rect(226, 63, 14, 1, C_MG); cv_rect(239, 56, 1, 8, C_MG);
    for (i = 0; i < 600u; i++)                  /* trigger on a rising zero crossing */
        if (scope[(a + i) & (SCOPE_N - 1u)] < 0 && scope[(a + i + 1u) & (SCOPE_N - 1u)] >= 0) { start = a + i; break; }
    for (x = 0; x < 240; x++) {
        y = 32 - ((int32_t)scope[(start + (uint32_t)x * 3u) & (SCOPE_N - 1u)] * 28 >> 15);
        if (x) { cv_line(x - 1, py + 1, x, y + 1, C_MG); cv_line(x - 1, py, x, y, C_CY); }
        py = y;
    }
    cv_blit(0, 56);
}

static void draw_row(uint32_t page, uint32_t r, uint32_t sel)
{
    char s[12];
    uint32_t idx = page * 4u + r, v = P[idx];
    cv_begin(240, 20, BG);
    cv_text(8, 2, &FONT_S, LABEL[idx], r == sel ? C_YE : C_CY);
    cv_rect(112, 6, 90, 9, C_PU);
    cv_rect(113, 7, 88, 7, BG);
    cv_rect(113, 7, (int32_t)(v * 88u / 127u), 7, r == sel ? C_YE : C_MG);
    fmt_val(idx, s);
    cv_text(236 - text_w(&FONT_S, s), 2, &FONT_S, s, C_WHITE);
    cv_blit(0, 122 + r * 20u);
}

static void draw_tabs(uint32_t page)
{
    uint32_t t;
    cv_begin(240, 16, BG);
    for (t = 0; t < 6u; t++) {
        if (t == page) cv_rect((int32_t)t * 40, 0, 38, 16, C_CY);
        cv_text((int32_t)t * 40 + 20 - text_w(&FONT_S, TAB[t]) / 2, 0, &FONT_S, TAB[t], t == page ? BG : C_PU);
    }
    cv_blit(0, 206);
}

static void draw_status(int32_t oct, const char *msg)
{
    char s[40], n[8];
    uint32_t i = 0, j;
    const char *p = "CPU ";
    while (*p) s[i++] = *p++;
    itoa3((int32_t)cpu_pct, n);
    for (j = 0; n[j]; j++) s[i++] = n[j];
    s[i++] = '%'; s[i++] = ' '; s[i++] = ' ';
    p = is_mono() ? "MONO" : "POLY";
    while (*p) s[i++] = *p++;
    s[i++] = ' '; s[i++] = ' '; s[i++] = 'O'; s[i++] = 'C'; s[i++] = 'T'; s[i++] = oct < 0 ? '-' : '+'; s[i++] = (char)('0' + (oct < 0 ? -oct : oct));
    if (msg) { s[i++] = ' '; s[i++] = ' '; while (*msg) s[i++] = *msg++; }
    s[i] = 0;
    draw_text_box(0, 224, 240, &FONT_S, s, C_PU, 1);
}

static const uint8_t CC_MAP[6][2] = {{74, P_CUT}, {71, P_RES}, {73, P_ATK}, {72, P_REL}, {1, P_LDEPTH}, {91, P_DMIX}};

static void app_main(void)
{
    static const uint8_t PAGE_BTN[6] = {BTN_EDIT, BTN_SEL, BTN_ENV, BTN_LFO, BTN_FX, BTN_GLO};
    uint32_t prev = 0, k, t_scope = 0, t_stat = 0, t_change = 0, redraw = 1, page = 0, sel = 0, sel_drawn = 0;
    uint32_t pending_save = 0, was_mono, lit_drawn = ~0u, edited_drawn = 0;
    uint8_t shown[NP];
    int32_t oct_drawn = 99;
    uint8_t held[27] = {0};
    const char *msg = 0;
    uint32_t t_msg = 0;

    settings_load(&set, sizeof set);
    if (set.magic != SET_MAGIC || set.cur >= NPRESET || set.oct < -2 || set.oct > 2) {
        uint32_t i, j;
        set.magic = SET_MAGIC; set.cur = 0; set.oct = 0; set.pad = 0;
        for (i = 0; i < NPRESET; i++) for (j = 0; j < NP; j++) set.slot[i][j] = FACTORY[i][j];
    }
    load_preset(set.cur);
    was_mono = (uint32_t)is_mono();
    for (k = 0; k < NP; k++) shown[k] = 255;

    lcd_fill(0, 0, 240, 240, BG);

    for (;;) {
        uint32_t keys, down, up, rel, b, lit = 0, base = (uint32_t)(KEY0 + 12 * set.oct), r;
        uint8_t st, d1, d2;
        int32_t d;
        platform_poll();

        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < 27u; k++) {
            if (down >> k & 1u) { held[k] = (uint8_t)(base + k); note_on(held[k], 100); midi_send(0x90, held[k], 100); }
            if (up >> k & 1u) { note_off(held[k]); midi_send(0x80, held[k], 0); }
        }

        b = fm1_input_edges(&rel);
        for (k = 0; k < 6u; k++)
            if (b & (1u << PAGE_BTN[k])) { page = k; redraw = 1; }
        if (b & (1u << BTN_HOME)) panic();
        if (b & (1u << BTN_OCT_DOWN) && set.oct > -2) { set.oct--; pending_save = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && set.oct < 2) { set.oct++; pending_save = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_SAVE)) {
            for (k = 0; k < NP; k++) set.slot[cur][k] = P[k];
            set.cur = (uint8_t)cur; edited = 0; pending_save = 1; t_change = fm1_ms; redraw = 1;
            msg = "SAVED"; t_msg = fm1_ms;
        }

        for (k = 0; k < 4u; k++) {              /* the four knobs of the page */
            d = fm1_enc_take(ENC_KNOB1 + k);
            if (d) { set_param(page * 4u + k, (int32_t)P[page * 4u + k] + d * (d > 2 || d < -2 ? 6 : 2)); sel = k; }
        }
        d = fm1_enc_take(ENC_ALGORITHM);
        if (d) set_param(P_CUT, (int32_t)P[P_CUT] + d * (d > 2 || d < -2 ? 6 : 2));
        d = fm1_enc_take(ENC_SELECT);
        if (d) set_param(P_RES, (int32_t)P[P_RES] + d * (d > 2 || d < -2 ? 6 : 2));
        d = fm1_enc_take(ENC_PRESETS);
        if (d) {
            int32_t n = (int32_t)cur + (d > 0 ? 1 : -1);
            load_preset((uint32_t)(n < 0 ? (int32_t)NPRESET - 1 : n >= (int32_t)NPRESET ? 0 : n));
            redraw = 1;
        }
        if ((uint32_t)is_mono() != was_mono) { was_mono = (uint32_t)is_mono(); panic(); }

        while (midi_get(&st, &d1, &d2)) {
            if ((st & 0xF0u) == 0x90u && d2) note_on(d1, d2);
            else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) note_off(d1);
            else if ((st & 0xF0u) == 0xB0u)
                for (k = 0; k < 6u; k++)
                    if (CC_MAP[k][0] == d1) set_param(CC_MAP[k][1], d2);
        }

        base = (uint32_t)(KEY0 + 12 * set.oct);
        for (k = 0; k < NV; k++)
            if (voice[k].gate && voice[k].note >= base && voice[k].note < base + 27u)
                lit |= 1u << (voice[k].note - base);
        if (lit != lit_drawn) {
            lit_drawn = lit;
            for (k = 0; k < 27u; k++) fm1_led_key(KEY_LED(k), lit >> k & 1u);
        }
        for (k = 0; k < 6u; k++) fm1_led_key(PAGE_BTN[k], k == page);
        fm1_led_key(BTN_OCT_DOWN, set.oct < 0);
        fm1_led_key(BTN_OCT_UP, set.oct > 0);

        /* the screen: only what changed */
        if (redraw) { draw_title(); draw_tabs(page); for (k = 0; k < NP; k++) shown[k] = 255; redraw = 0; oct_drawn = 99; }
        for (r = 0; r < 4u; r++)
            if (shown[page * 4u + r] != P[page * 4u + r]) {
                shown[page * 4u + r] = P[page * 4u + r];
                draw_row(page, r, sel);
            }
        if (sel != sel_drawn) {                 /* the highlight follows the last turned knob */
            sel_drawn = sel;
            for (r = 0; r < 4u; r++) shown[page * 4u + r] = 255;
        }
        if (edited != edited_drawn) { edited_drawn = edited; draw_title(); }   /* the * after the preset name */
        if (fm1_ms - t_scope >= 50u) { t_scope = fm1_ms; draw_scope(); }
        if (msg && fm1_ms - t_msg > 1500u) { msg = 0; oct_drawn = 99; }
        if (set.oct != oct_drawn || fm1_ms - t_stat >= 500u) {
            oct_drawn = set.oct; t_stat = fm1_ms;
            draw_status(set.oct, msg);
        }

        if (pending_save && quiet() && fm1_ms - t_change > 1000u) {   /* an erase stalls the sound: only when silent */
            settings_save(&set, sizeof set);
            pending_save = 0;
        }
    }
}
