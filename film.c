/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (STRANGER SYNTH, based on OFFWORLD) */
/* STRANGER SYNTH's film: the 240 x 120 band under the title. A small town in the 80s, one night, seven places, all
 * drawn here (no images, no figures from any film or show): a street of houses, the woods, the lab on the hill, a
 * road with bikes, a basement door with red light behind it, a red storm, a radio tower, the arcade at closing time. Each scrolls sideways in
 * layers; every 18 to 35 s a cut (the tape tears) to another place, its name typed in the corner, now and then a
 * line after it. The lights follow the notes (windows, lamps, headlights, fireflies: each belongs to a key).
 * The MIRROR: while PERFORM's effects play, the picture turns over into a dark, cold copy of itself, red at the
 * edges, with ash floating up; the more effects, the deeper.
 * Included by app.c (after the drawing helpers): ui, set, KM, PR and the engine's calls are in scope. */

enum { SC_STREET, SC_WOODS, SC_LAB, SC_ROAD, SC_DOOR, SC_STORM, SC_TOWER, SC_ARCADE, NSCENE };
static const char *const SCENE_NAME[NSCENE] = {
    "MAPLE LANE // NIGHT",   "THE WOODS // EAST TRAIL", "THE LAB // RESTRICTED", "ROUTE 9 // BIKES",
    "THE BASEMENT // DOOR",  "THE SKY // RED STORM",    "RADIO TOWER // 88.5",    "THE ARCADE // LAST GAME",
};

/* the little stories: a line now and then, typed after the place (our own lines) */
static const char *const LOGS[] = {
    "THE LIGHTS FLICKERED AGAIN.",
    "BIKE TRACKS END AT THE TREES.",
    "STATIC ON EVERY CHANNEL SINCE 9.",
    "BE HOME BEFORE THE STREETLIGHTS.",
    "THE VANS CAME BACK TONIGHT.",
    "THE WALKIE CLICKED TWICE. NO VOICE.",
    "THE CLOCK STOPPED AT 11:47.",
    "SOMETHING HUMS UNDER THE FLOOR.",
    "THE FENCE WAS NOT CUT LAST WEEK.",
    "THE RADIO PLAYS A SONG NOBODY KNOWS.",
    "MOM SAYS IT'S JUST THE WIRING.",
    "THE DOG WON'T GO INTO THE WOODS.",
    "THE SKY WAS RED FOR A SECOND.",
    "OVER. OVER. ANYONE OUT THERE?",
    "THE TAPE REWOUND BY ITSELF.",
    "THE BASEMENT IS COLDER THAN JULY.",
    "THREE BIKES. FOUR SHADOWS.",
    "THE LAB'S WINDOWS NEVER GO DARK.",
    "NO STARS OVER THE QUARRY TONIGHT.",
    "THE PHONE RANG. NOBODY THERE.",
    "HIGH SCORE: 3 LETTERS NOBODY KNOWS.",
    "REWIND THE TAPE BEFORE YOU RETURN IT.",
    "SIDE B IS JUST STATIC.",
};
#define NLOGS (sizeof LOGS / sizeof LOGS[0])

#define GROUND 100
#define NASH 28
static struct {
    uint8_t scene, glitch, flicker, flash, car_on;
    int8_t log;                          /* the line typed under the place (-1: none) */
    uint32_t frame, cut_frame, cut_at, len;
    float sx;                            /* the camera's pan, in pixels */
    float car_x, car_v;                  /* a bike passing (STREET, LAB) */
    uint32_t notes_seen;
    int16_t bolt[8];
    float mirror;                        /* the MIRROR, 0..1 */
    float ash_x[NASH], ash_y[NASH], ash_v[NASH];
} film;

static uint32_t hsh(uint32_t a, uint32_t b)
{
    uint32_t x = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return x;
}

static float cos_turns(float t)
{
    t += 0.25f;
    t -= (float)(int)t;
    if (t < 0.0f)
        t += 1.0f;
    return fm_sin_turns(t);
}
static float sin_t(float t)
{
    t -= (float)(int)t;
    if (t < 0.0f)
        t += 1.0f;
    return fm_sin_turns(t);
}

static uint32_t lit(uint32_t h) { return ui.win[h % NKEYS]; }   /* a light's key: how lit it is, 0..255 */
static float lvl01(void) { return fm_clampf(eng_level() * 1.6f, 0.0f, 1.0f); }

/* ------------------------------------------------------------------ helpers --- */
static void grad(int32_t y0, int32_t y1, uint16_t c0, uint16_t c1)   /* a sky: rows y0..y1-1, eased, dithered (gfx2.c) */
{
    uint8_t a[3], b[3];
    a[0] = (uint8_t)(((c0 >> 11) & 31u) * 255u / 31u);
    a[1] = (uint8_t)(((c0 >> 5) & 63u) * 255u / 63u);
    a[2] = (uint8_t)((c0 & 31u) * 255u / 31u);
    b[0] = (uint8_t)(((c1 >> 11) & 31u) * 255u / 31u);
    b[1] = (uint8_t)(((c1 >> 5) & 63u) * 255u / 63u);
    b[2] = (uint8_t)((c1 & 31u) * 255u / 31u);
    sgrad(y0, y1, a, b, 2.0f);
}

static void tri(int32_t cx, int32_t top, int32_t hw, int32_t h, uint16_t c)   /* a filled triangle, point up */
{
    int32_t y;
    for (y = 0; y < h; y++) {
        int32_t w = hw * (y + 1) / h;
        cv_rect(cx - w, top + y, 2 * w + 1, 1, c);
    }
}

static void stars(int32_t y1, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < 40u; i++) {
        uint32_t h = hsh(i, seed);
        int32_t x = (int32_t)((h % 240u + 240u - ((uint32_t)(film.sx * 0.05f) % 240u)) % 240u), y = (int32_t)((h >> 8) % (uint32_t)y1);
        uint32_t tw = ((film.frame + (h >> 16)) & 63u) < 4u;   /* a twinkle */
        cv_pset(x, y, tw ? C_WHITE : mix565(RGB(120, 120, 160), C_BLACK, (h >> 20) & 127u));
    }
}

/* a cone of light from (x, y), its axis at angle a (turns: 0 right, 0.25 down), half-width s, length len */
static void cone(int32_t x, int32_t y, float a, float s, int32_t len, uint16_t c, uint16_t bg)
{                                        /* (the facelift) a soft fan of light: added, fading along it and to its edges */
    float ux = cos_turns(a), uy = sin_t(a), vx = -uy, vy = ux, k = 0.55f;
    uint32_t r = ((c >> 11) & 31u) << 3, g = ((c >> 5) & 63u) << 2, b = (c & 31u) << 3;
    int32_t i, j;
    (void)bg;
    for (i = 2; i < len; i++) {
        float t = (float)i / (float)len, hw = 1.0f + (float)i * s * 6.2832f, fall = (1.0f - t) * k;
        int32_t h = (int32_t)hw + 1;
        for (j = -h; j <= h; j++) {
            float e = 1.0f - fm_fabsf((float)j) / hw, q;
            if (e <= 0.0f)
                continue;
            q = e * e * fall;
            px_add(x + (int32_t)(ux * (float)i + vx * (float)j), y + (int32_t)(uy * (float)i + vy * (float)j),
                   (uint32_t)((float)r * q), (uint32_t)((float)g * q), (uint32_t)((float)b * q));
        }
    }
}

/* a layer: things every sp pixels of the world, panned at speed; draw(x, index, hash) for each one in sight */
typedef void (*thing_fn)(int32_t x, int32_t i, uint32_t h);
static void layer(float speed, int32_t sp, uint32_t id, int32_t wmax, thing_fn draw)
{
    int32_t off = (int32_t)(film.sx * speed), i = (off - wmax) / sp - 1, x;
    for (;; i++) {
        x = i * sp - off;
        if (x > 240)
            break;
        if (x + wmax < 0)
            continue;
        draw(x, i, hsh((uint32_t)i, id));
    }
}

/* windows in a wall: a grid, some dim, those of the keys that play bright */
static void windows(int32_t x, int32_t y0, int32_t w, int32_t y1, int32_t px, int32_t py, int32_t ww, int32_t wh,
                    uint32_t h, uint16_t dim, uint16_t warm)
{
    int32_t xx, yy;
    for (yy = y0 + 4; yy + wh < y1; yy += py)
        for (xx = x + 3; xx + ww < x + w - 2; xx += px) {
            uint32_t q = hsh(h, (uint32_t)((yy << 8) + xx - x)), l = lit(q);
            if (l && (q >> 9) % 3u == 0u)
                cv_rect(xx, yy, ww, wh, mix565(dim, warm, 60u + l * 196u / 255u));
            else if ((q >> 12) % 6u == 0u)
                cv_rect(xx, yy, ww, wh, dim);
        }
}

/* a bike and its rider, side on, facing d (1 right, -1 left); the headlight's beam ahead */
static void bike(int32_t x, int32_t y, int d, uint32_t light, uint16_t c)
{
    uint32_t ph = film.frame / 3u;
    int32_t pd = (int32_t)(ph & 3u) - 1;
    cv_rect(x - 7, y - 3, 5, 5, c);      /* the wheels */
    cv_rect(x + 3, y - 3, 5, 5, c);
    cv_rect(x - 6, y - 2, 3, 3, C_BLACK);
    cv_rect(x + 4, y - 2, 3, 3, C_BLACK);
    cv_line(x - 4, y - 1, x + 5, y - 1, c);   /* the frame */
    cv_line(x, y - 1, x - d * 2, y - 6, c);
    cv_rect(x - d * 3 - 1, y - 14, 3, 8, c);   /* the rider: body, head, legs */
    cv_rect(x - d * 3 - 1, y - 18, 3, 3, c);
    cv_line(x - d * 2, y - 7, x + pd, y - 2, c);
    cv_line(x - d * 2, y - 13, x + d * 4, y - 8, c);   /* arms to the bars */
    if (light) {
        uint16_t lc = mix565(RGB(80, 70, 40), RGB(255, 245, 200), light);
        cone(x + d * 6, y - 6, d > 0 ? 0.01f : 0.49f, 0.035f, 30 + (int32_t)light / 10, lc, RGB(10, 8, 20));
        cv_rect(x + d * 6 - 1, y - 7, 2, 2, C_WHITE);
    }
}

#include "scenes.c"                      /* the eight places */

/* ====================================================================== FILM === */
static void px_row_shift(int32_t y, int32_t d)   /* a row of the canvas moved d pixels (the tape glitch) */
{
    uint16_t *row = cv_px + (uint32_t)y * 240u;
    int32_t x;
    if (d > 0)
        for (x = 239; x >= d; x--)
            row[x] = row[x - d];
    else
        for (x = 0; x < 240 + d; x++)
            row[x] = row[x - d];
}

static void px_dim_row(int32_t y)        /* a scanline: the row at three quarters */
{
    uint16_t *row = cv_px + (uint32_t)y * 240u;
    int32_t x;
    for (x = 0; x < 240; x++) {
        uint32_t c = swap16(row[x]);
        row[x] = swap16(((c >> 1) & 0x7BEFu) + ((c >> 2) & 0x39E7u));
    }
}

static void px_tint(int32_t y1, uint32_t t, uint16_t c)   /* the sky: lit by lightning, or by dusk and dawn */
{
    int32_t y, x;
    for (y = 0; y < y1; y++) {
        uint16_t *row = cv_px + (uint32_t)y * 240u;
        uint32_t ty = t * (uint32_t)(y1 - y / 2) / (uint32_t)y1;   /* (stronger at the top) */
        for (x = 0; x < 240; x++)
            row[x] = swap16(mix565(swap16(row[x]), c, ty));
    }
}

/* the MIRROR: the picture turned over (at full) and made cold, blue-grey, its brightest parts red */
static void px_mirror(float m)
{
    uint32_t t = (uint32_t)(m * 256.0f), x, y;
    if (m > 0.5f)                        /* turned over */
        for (y = 0; y < 60u; y++) {
            uint16_t *a = cv_px + y * 240u, *b = cv_px + (119u - y) * 240u;
            for (x = 0; x < 240u; x++) {
                uint16_t s = a[x];
                a[x] = b[x];
                b[x] = s;
            }
        }
    for (y = 0; y < 120u; y++) {
        uint16_t *row = cv_px + y * 240u;
        for (x = 0; x < 240u; x++) {
            uint32_t c = swap16(row[x]), r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
            uint32_t lum = (r * 2u + g + b * 2u) >> 2;           /* 0 .. ~31 */
            uint16_t cold = lum > 20u ? RGB(200, 30, 30) : RGB(lum * 2u, lum * 3u, lum * 5u + 20u);
            row[x] = swap16(mix565((uint16_t)c, cold, t));
        }
    }
}

static void film_cut(uint32_t now)       /* to another place, at random */
{
    uint32_t s = film.scene;
    if (film.frame)
        while (s == film.scene)
            s = urand() % NSCENE;
#ifdef PLAT_HOST
    if (getenv("STRANGER_SCENE"))
        s = film.scene;
#endif
    film.scene = (uint8_t)s;
    film.cut_at = now;
    film.cut_frame = film.frame;
    film.len = 18000u + urand() % 17000u;
    film.glitch = film.frame ? 9 : 0;
    film.car_on = 0;
    film.log = (int8_t)(set.logs && film.frame && urand() % 3u != 0u ? (int32_t)(urand() % NLOGS) : -1);
}

static void film_init(void)
{
    uint32_t i;
    film.frame = 0;
    film.log = -1;
    for (i = 0; i < NASH; i++) {
        film.ash_x[i] = (float)(urand() % 240u);
        film.ash_y[i] = (float)(urand() % 120u);
        film.ash_v[i] = 0.2f + (float)(urand() % 10u) * 0.06f;
    }
    film.scene = (uint8_t)(urand() % NSCENE);
    film.sx = (float)(urand() % 4000u);
#ifdef PLAT_HOST
    if (getenv("STRANGER_SCENE"))        /* the host: one place, held (the screenshots) */
        film.scene = (uint8_t)(atoi(getenv("STRANGER_SCENE")) % NSCENE);
#endif
}

static void film_step(void)              /* the movement, a frame */
{
    uint32_t i, nn = eng_notes();
    float want = 0.0f;
    film.sx += film.scene == SC_ROAD ? 1.4f : 0.8f;
    for (i = 0; i < NFX; i++)            /* the MIRROR follows the effects */
        want += eng_fx_amt((int)i);
    want = fm_minf(1.0f, want * 0.8f);
    film.mirror += (want - film.mirror) * (want > film.mirror ? 0.12f : 0.05f);
    if (film.mirror > 0.05f)
        for (i = 0; i < NASH; i++) {     /* ash, floating up */
            film.ash_y[i] -= film.ash_v[i];
            film.ash_x[i] += 0.3f * fm_sin_turns((float)((film.frame + i * 11u) % 100u) / 100.0f);
            if (film.ash_y[i] < 0.0f) {
                film.ash_y[i] = 120.0f;
                film.ash_x[i] = (float)(urand() % 240u);
            }
        }
    if (!film.car_on && (film.scene == SC_STREET || film.scene == SC_LAB) && (nn != film.notes_seen || urand() % 300u == 0u)) {
        film.car_on = 1;                 /* a bike passing */
        film.car_v = (urand() & 1u) ? 2.2f : -2.2f;
        film.car_x = film.car_v > 0.0f ? -20.0f : 260.0f;
    }
    if (film.scene == SC_STORM && nn != film.notes_seen && urand() % 3u == 0u) {   /* the storm: lightning on notes */
        int32_t x = 20 + (int32_t)(urand() % 200u);
        film.flash = 6;
        for (i = 0; i < 8u; i++) {
            film.bolt[i] = (int16_t)x;
            x += (int32_t)(urand() % 17u) - 8;
        }
    } else if (film.flash)
        film.flash--;
    film.notes_seen = nn;
    if (film.car_on) {
        film.car_x += film.car_v;
        if (film.car_x < -60.0f || film.car_x > 280.0f)
            film.car_on = 0;
    }
    if (film.flicker)
        film.flicker--;
    else if (urand() % 200u == 0u)
        film.flicker = (uint8_t)(6u + urand() % 12u);   /* (seen, not heard: no sounds from the film here) */
    film.frame++;
}

static void film_frame(uint32_t now)
{
    uint32_t i, since;
    int32_t y;
    if (now - film.cut_at >= film.len) {
#ifdef PLAT_HOST
        if (getenv("STRANGER_SCENE"))
            film.cut_at = now;
        else
#endif
            film_cut(now);
    }
    film_step();
    cv_begin(240, 120, C_BLACK);
    switch (film.scene) {
    case SC_STREET: scene_street(); break;
    case SC_WOODS: scene_woods(); break;
    case SC_LAB: scene_lab(); break;
    case SC_ROAD: scene_road(); break;
    case SC_DOOR: scene_door(); break;
    case SC_STORM: scene_storm(); break;
    case SC_TOWER: scene_tower(); break;
    default: scene_arcade(); break;
    }
    if (film.flicker && film.scene != SC_STORM && (film.frame & 2u))   /* the power dips */
        px_tint(120, 90, C_BLACK);
    if (set.heart && eng_heart_env() > 0.2f)                            /* the heart: the picture pulses red */
        px_tint(120, (uint32_t)(eng_heart_env() * 70.0f), RGB(120, 0, 0));
    if (film.flash >= 4u || film.flash == 2u)                           /* red lightning */
        px_tint(GROUND, film.flash >= 4u ? 140u : 60u, RGB(255, 170, 150));
    if (film.flash >= 3u)
        for (i = 0; i + 1u < 8u; i++)
            cv_line(film.bolt[i], (int32_t)i * 12, film.bolt[i + 1], (int32_t)(i + 1u) * 12, RGB(255, 230, 220));
    if (film.mirror > 0.03f) {           /* the MIRROR, and its ash */
        px_mirror(film.mirror);
        for (i = 0; i < NASH; i++)
            cv_rect((int32_t)film.ash_x[i], (int32_t)film.ash_y[i], 1 + (int32_t)(i & 1u), 1 + (int32_t)(i & 1u),
                    mix565(RGB(40, 40, 50), RGB(200, 200, 210), (uint32_t)(film.mirror * 200.0f)));
    }
    since = film.frame - film.cut_frame;
    if (since < 75u) {                   /* the place, typed in */
        char t[32];
        uint32_t n = since / 2u, k;
        for (k = 0; k < 31u && SCENE_NAME[film.scene][k] && k < n; k++)
            t[k] = SCENE_NAME[film.scene][k];
        t[k] = 0;
        if (k) {
            cv_rect(2, 2, text_w(&FONT_XS, t) + 8, 14, RGB(4, 2, 4));
            cv_text(5, 3, &FONT_XS, t, C_RED);
            if ((film.frame & 4u) || since > 70u)
                cv_rect(5 + text_w(&FONT_XS, t), 4, 5, 10, C_RED);
        }
    }
    if (film.log >= 0 && since >= 40u && since < 190u) {   /* the log line, typed after the place */
        char t[40];
        uint32_t n = (since - 40u) / 2u, k;
        for (k = 0; k < 39u && LOGS[film.log][k] && k < n; k++)
            t[k] = LOGS[film.log][k];
        t[k] = 0;
        if (k) {
            cv_rect(2, 17, text_w(&FONT_XS, t) + 8, 14, RGB(4, 2, 4));
            cv_text(5, 18, &FONT_XS, t, RGB(200, 200, 210));
        }
    }
    {                                    /* the camcorder's viewfinder: the tape counter */
        uint32_t s = plat_ms() / 1000u;
        char t[16];
        t[0] = 'S';
        t[1] = 'P';
        t[2] = ' ';
        t[3] = ' ';
        t[4] = (char)('0' + s / 3600u % 10u);
        t[5] = ':';
        t[6] = (char)('0' + s / 600u % 6u);
        t[7] = (char)('0' + s / 60u % 10u);
        t[8] = ':';
        t[9] = (char)('0' + s / 10u % 6u);
        t[10] = (char)('0' + s % 10u);
        t[11] = 0;
        cv_text(6, 106, &FONT_XS, t, C_BLACK);
        cv_text(5, 105, &FONT_XS, t, RGB(235, 235, 225));
    }
    for (y = (int32_t)(film.frame & 3u); y < 120; y += 4)   /* scanlines, rolling slowly */
        px_dim_row(y);
    for (i = 0; i < 50u; i++)            /* grain */
        cv_rect((int32_t)(urand() % 240u), (int32_t)(urand() % 120u), 1, 1,
                (urand() & 1u) ? RGB(70, 60, 70) : C_BLACK);
    if ((film.frame % 97u) < 3u || film.mirror > 0.4f) {   /* the tracking line, now and then (always in the MIRROR) */
        int32_t ty = (int32_t)((film.frame * 3u) % 120u);
        for (y = ty; y < ty + 3 && y < 120; y++)
            px_row_shift(y, (int32_t)(urand() % 6u) - 3);
    }
    if (film.glitch >= 7u) {             /* the cut: a moment of colour bars */
        static const uint16_t BARS[7] = {RGB(190, 190, 190), RGB(190, 190, 0), RGB(0, 190, 190), RGB(0, 190, 0),
                                         RGB(190, 0, 190), RGB(190, 0, 0), RGB(0, 0, 190)};
        for (i = 0; i < 7u; i++)
            cv_rect((int32_t)i * 35, 0, 35, 90, BARS[i]);
        cv_rect(0, 90, 240, 30, RGB(16, 16, 20));
        cv_text(170, 100, &FONT_XS, "PLAY >", C_WHITE);
    }
    if (film.glitch) {                   /* the cut: the tape tears */
        uint32_t b;
        for (b = 0; b < 5u; b++) {
            int32_t y0 = (int32_t)(urand() % 110u), h = 2 + (int32_t)(urand() % 10u), d = (int32_t)(urand() % 40u) - 20;
            for (y = y0; y < y0 + h && y < 120; y++)
                px_row_shift(y, d);
        }
        cv_rect(0, (int32_t)(urand() % 120u), 240, 1, C_RED);
        cv_rect(0, (int32_t)(urand() % 120u), 240, 1, RGB(60, 200, 220));
        film.glitch--;
    }
    cv_blit(0, 24);
}
