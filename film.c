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
static void grad(int32_t y0, int32_t y1, uint16_t c0, uint16_t c1)   /* a sky: rows y0..y1-1, eased */
{
    int32_t y, n = y1 - y0;
    for (y = y0; y < y1; y++) {
        uint32_t t = (uint32_t)((y - y0) * (y - y0)) * 256u / (uint32_t)(n * n);
        cv_rect(0, y, 240, 1, mix565(c0, c1, t));
    }
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
{
    int32_t i;
    for (i = -4; i <= 4; i++) {
        float t = a + s * (float)i / 4.0f;
        int32_t ex = x + (int32_t)(cos_turns(t) * (float)len), ey = y + (int32_t)(sin_t(t) * (float)len);
        cv_line(x, y, ex, ey, mix565(c, bg, 120u + (uint32_t)(i < 0 ? -i : i) * 25u));
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

/* ===================================================================== STREET === */
static void st_trees(int32_t x, int32_t i, uint32_t h)
{
    (void)i;
    tri(x, GROUND - 30 - (int32_t)(h % 14u), 8 + (int32_t)((h >> 4) % 5u), 34 + (int32_t)(h % 14u), RGB(10, 10, 26));
}
static void st_house(int32_t x, int32_t i, uint32_t h)
{
    int32_t w = 46 + (int32_t)(h % 12u), hh = 24 + (int32_t)((h >> 4) % 10u), y = GROUND - hh, k;
    uint16_t body = RGB(30, 22, 46), roof = RGB(40, 24, 50);
    (void)i;
    tri(x + w / 2, y - 17, w / 2 + 5, 17, RGB(110, 50, 110));   /* the roof, its edge lit by the sky */
    tri(x + w / 2, y - 16, w / 2 + 4, 16, roof);
    cv_rect(x, y, w, hh, body);
    if ((h >> 9) & 1u)                   /* a chimney */
        cv_rect(x + w - 14, y - 18, 5, 10, roof);
    for (k = 0; k < 3; k++) {            /* three windows: lit by their keys */
        uint32_t q = hsh(h, (uint32_t)k), l = lit(q);
        int32_t wx = x + 6 + k * (w - 16) / 2, wy = y + 6;
        if (k == 1) {                    /* the door, and the porch light */
            cv_rect(wx, y + hh - 14, 7, 14, RGB(8, 6, 12));
            if (!(film.flicker && (h & 1u)))
                cv_rect(wx + 8, y + hh - 16, 2, 2, mix565(RGB(120, 90, 40), RGB(255, 220, 140), l));
            continue;
        }
        cv_rect(wx, wy, 8, 7, l ? mix565(RGB(60, 40, 20), RGB(255, 210, 120), l) : (q >> 5) % 3u == 0u ? RGB(150, 110, 60) : RGB(20, 16, 26));
        cv_rect(wx + 3, wy, 1, 7, body);
    }
}
static void st_lamp(int32_t x, int32_t i, uint32_t h)
{
    uint32_t l = lit(h);
    int off = film.flicker && ((h >> 3) & 1u) && (film.frame & 2u);
    (void)i;
    cv_rect(x, GROUND - 40, 2, 44, RGB(30, 30, 40));
    cv_rect(x, GROUND - 40, 8, 2, RGB(30, 30, 40));
    if (!off) {
        uint16_t lc = mix565(RGB(110, 90, 50), RGB(255, 230, 160), 80u + l * 2u / 3u);
        cv_rect(x + 6, GROUND - 38, 4, 2, lc);
        cone(x + 8, GROUND - 37, 0.25f, 0.06f, 44, mix565(lc, RGB(12, 10, 22), 150), RGB(12, 10, 22));
    }
}
static void scene_street(void)
{
    int32_t x;
    grad(0, GROUND, RGB(4, 6, 22), RGB(64, 22, 70));
    stars(50, 1);
    layer(0.2f, 9, 1, 12, st_trees);
    layer(0.6f, 66, 2, 64, st_house);
    cv_rect(0, GROUND, 240, 20, RGB(12, 10, 22));   /* the road, its middle line passing */
    for (x = -((int32_t)film.sx % 24); x < 240; x += 24)
        cv_rect(x, GROUND + 10, 10, 1, RGB(90, 80, 50));
    layer(1.0f, 120, 3, 12, st_lamp);
    if (film.car_on)
        bike((int32_t)film.car_x, GROUND + 14, film.car_v > 0.0f ? 1 : -1, 200u, RGB(4, 4, 8));
}

/* ====================================================================== WOODS === */
static void wd_pine(int32_t x, uint32_t h, int32_t base, int32_t size, uint16_t c)
{
    int32_t k;
    cv_rect(x - 1, base - 6, 3, 6, c);
    for (k = 0; k < 3; k++)
        tri(x, base - size + k * size / 4 - 6, size / 4 + k * 3, size / 3, c);
    (void)h;
}
static void wd_far(int32_t x, int32_t i, uint32_t h) { (void)i; wd_pine(x, h, GROUND - 8, 34 + (int32_t)(h % 12u), RGB(10, 26, 34)); }
static void wd_mid(int32_t x, int32_t i, uint32_t h) { (void)i; wd_pine(x, h, GROUND + 4, 54 + (int32_t)(h % 16u), RGB(6, 16, 22)); }
static void wd_near(int32_t x, int32_t i, uint32_t h)
{
    (void)i;
    cv_rect(x, 0, 9 + (int32_t)(h % 5u), 120, RGB(2, 4, 8));   /* a trunk, close */
}
static void scene_woods(void)
{
    uint32_t k;
    int32_t y;
    float sw = fm_sin_turns((float)(film.frame % 200u) / 200.0f);
    grad(0, GROUND, RGB(2, 6, 14), RGB(18, 46, 56));
    stars(36, 2);
    cv_rect(186, 14, 10, 10, RGB(200, 210, 200));          /* the moon, a cloud across it */
    cv_rect(184, 18, 16, 3, RGB(18, 30, 44));
    layer(0.25f, 13, 4, 20, wd_far);
    for (y = 60; y < 96; y += 3)                            /* fog, drifting */
        cv_rect((int32_t)((film.frame / 2u + (uint32_t)y * 7u) % 80u) - 40, y, 200, 1, RGB(30, 56, 64));
    layer(0.55f, 23, 5, 30, wd_mid);
    cv_rect(0, GROUND, 240, 20, RGB(4, 10, 12));
    for (k = 0; k < NKEYS; k++)                             /* fireflies: the keys */
        if (ui.win[k]) {
            uint32_t h = hsh(k, 77u);
            int32_t fx = (int32_t)(h % 230u) + 5, fy = 30 + (int32_t)((h >> 8) % 60u) + (int32_t)(fm_sin_turns((float)((film.frame + h) % 50u) / 50.0f) * 3.0f);
            uint16_t c = mix565(RGB(20, 40, 20), RGB(220, 255, 140), ui.win[k]);
            cv_rect(fx, fy, 2, 2, c);
            if (ui.win[k] > 160u)
                cv_rect(fx - 1, fy - 1, 4, 4, mix565(c, RGB(10, 30, 30), 160));
        }
    cone(30, 118, 0.80f + 0.05f * sw, 0.03f, 120, RGB(110, 110, 80), RGB(10, 20, 26));   /* two flashlights, searching */
    cone(210, 118, 0.70f - 0.06f * sw, 0.03f, 120, RGB(110, 110, 80), RGB(10, 20, 26));
    layer(1.3f, 150, 6, 14, wd_near);
}

/* ======================================================================== LAB === */
static void lab_fence(int32_t x, int32_t i, uint32_t h)
{
    int32_t k;
    (void)i;
    (void)h;
    cv_rect(x, GROUND - 2, 2, 22, RGB(40, 40, 50));
    for (k = 0; k < 4; k++)
        cv_line(x, GROUND + k * 5, x + 18, GROUND + 5 + k * 5, RGB(28, 28, 38));
    cv_line(x, GROUND - 2, x + 18, GROUND - 2, RGB(50, 50, 60));
}
static void lab_pole(int32_t x, int32_t i, uint32_t h)
{
    (void)i;
    (void)h;
    cv_rect(x, 30, 2, GROUND - 30, RGB(14, 14, 22));
    cv_rect(x - 6, 32, 14, 1, RGB(14, 14, 22));
    cv_line(x - 6, 33, x - 60, 38, RGB(20, 20, 30));
    cv_line(x + 8, 33, x + 60, 38, RGB(20, 20, 30));
}
static void scene_lab(void)
{
    int32_t y, bx = 60 - (int32_t)(film.sx * 0.15f) % 300;
    uint32_t k;
    float sw = fm_sin_turns((float)(film.frame % 300u) / 300.0f);
    if (bx < -140)
        bx += 300;
    grad(0, GROUND, RGB(4, 4, 16), RGB(30, 40, 64));
    stars(40, 3);
    for (y = 70; y < GROUND; y++) {                          /* the hill */
        int32_t w = (y - 70) * 6;
        cv_rect(120 - w, y, 2 * w, 1, RGB(10, 12, 20));
    }
    cone(bx + 90, 48, 0.62f + 0.12f * sw, 0.025f, 160, RGB(220, 230, 255), RGB(10, 12, 26));   /* the searchlight */
    cv_rect(bx, 50, 120, 24, RGB(40, 44, 56));              /* the lab: a long block, its windows */
    cv_rect(bx + 30, 42, 50, 8, RGB(40, 44, 56));
    windows(bx, 50, 120, 74, 7, 6, 4, 3, 99u, RGB(30, 40, 40), RGB(170, 255, 220));
    cv_rect(bx + 86, 34, 2, 8, RGB(40, 40, 50));            /* the dish */
    cv_rect(bx + 82, 32, 10, 2, RGB(60, 60, 70));
    for (k = 0; k < 3u; k++) {                              /* warning lights: blink with the notes */
        uint32_t l = lit(k * 9u + 2u);
        cv_rect(bx + 10 + (int32_t)k * 48, 47, 3, 3, l || (film.frame & 32u) ? mix565(RGB(90, 10, 10), RGB(255, 30, 30), l ? l : 120u) : RGB(40, 8, 8));
    }
    layer(0.7f, 110, 7, 60, lab_pole);
    cv_rect(0, GROUND, 240, 20, RGB(6, 8, 12));
    if (film.car_on)
        bike((int32_t)film.car_x, GROUND + 10, film.car_v > 0.0f ? 1 : -1, 180u, RGB(2, 2, 6));
    layer(1.0f, 20, 8, 20, lab_fence);
}

/* ======================================================================= ROAD === */
static void rd_pole(int32_t x, int32_t i, uint32_t h)
{
    (void)i;
    (void)h;
    cv_rect(x, 20, 3, GROUND - 18, RGB(16, 6, 12));
    cv_rect(x - 8, 24, 19, 2, RGB(16, 6, 12));
    cv_line(x - 8, 25, x - 92, 30, RGB(30, 10, 20));
    cv_line(x + 10, 25, x + 94, 30, RGB(30, 10, 20));
}
static void rd_field(int32_t x, int32_t i, uint32_t h)
{
    (void)i;
    tri(x, GROUND - 14 - (int32_t)(h % 10u), 18 + (int32_t)(h % 12u), 16 + (int32_t)(h % 10u), RGB(24, 8, 20));
}
static void scene_road(void)
{
    int32_t x, k;
    grad(0, GROUND, RGB(10, 4, 20), RGB(150, 36, 40));
    stars(30, 4);
    for (k = 0; k < 24; k++)                                 /* the glow at the horizon */
        cv_rect(40 + k * 2, GROUND - 24 + k, 160 - k * 4, 1, mix565(RGB(255, 110, 60), RGB(150, 36, 40), (uint32_t)(24 - k) * 10u));
    layer(0.2f, 40, 9, 40, rd_field);
    cv_rect(0, GROUND - 4, 240, 24, RGB(16, 6, 14));
    for (x = -((int32_t)(film.sx * 2.0f) % 30); x < 240; x += 30)   /* the road rushing by */
        cv_rect(x, GROUND + 10, 14, 2, RGB(120, 90, 60));
    for (k = 0; k < 3; k++) {            /* three riders: their headlights are the keys */
        uint32_t l = 0, j;
        for (j = (uint32_t)k; j < NKEYS; j += 3u)
            if (ui.win[j] > l)
                l = ui.win[j];
        bike(70 + k * 50 + (int32_t)(fm_sin_turns((float)((film.frame + (uint32_t)k * 13u) % 90u) / 90.0f) * 4.0f),
             GROUND - 5 + (k & 1) * 2, 1, 60u + l * 3u / 4u, RGB(2, 2, 6));
    }
    layer(1.6f, 130, 10, 100, rd_pole);
}

/* ======================================================================= DOOR === */
static void scene_door(void)
{
    float lv = lvl01();
    int32_t y, open = 10 + (int32_t)(lv * 16.0f), dx = 100, dy = 22, dw = 40, dh = 78;
    uint32_t k;
    cv_rect(0, 0, 240, 120, RGB(8, 6, 12));                 /* the room */
    for (y = 0; y < 120; y += 6)                            /* the wallpaper's stripes */
        cv_rect(0, y, 240, 1, RGB(12, 9, 16));
    cv_rect(0, GROUND, 240, 20, RGB(14, 10, 14));
    for (y = GROUND; y < 120; y += 3)                       /* the light on the floor, spilling out */
        cv_rect(dx + dw - open - (y - GROUND) * 2, y, open + (y - GROUND) * 4, 1,
                mix565(RGB(200, 40, 20), RGB(14, 10, 14), 120u + (uint32_t)(y - GROUND) * 6u));
    for (y = 0; y < 6; y++)                                  /* the red light round the frame */
        cv_rect(dx - 10 + y, dy - 10 + y, dw + 20 - 2 * y, dh + 10 - y, mix565(RGB(90, 14, 14), RGB(8, 6, 12), 200u - (uint32_t)y * 20u - (uint32_t)(lv * 60.0f)));
    cv_rect(dx - 4, dy - 4, dw + 8, dh + 4, RGB(30, 20, 22)); /* the frame */
    for (y = 0; y < dh; y++)                                 /* behind it: red, burning */
        cv_rect(dx + dw - open, dy + y, open, 1,
                mix565(RGB(255, 120, 40), RGB(140, 10, 10), (uint32_t)y * 256u / (uint32_t)dh));
    cv_rect(dx, dy, dw - open, dh, RGB(20, 12, 14));        /* the door, ajar */
    cv_rect(dx + dw - open - 6, dy + dh / 2, 3, 3, RGB(120, 100, 60));
    for (k = 0; k < NKEYS; k++)                             /* sparks drifting out: the keys */
        if (ui.win[k]) {
            uint32_t h = hsh(k, 31u), age = (255u - ui.win[k]);
            int32_t px = dx + dw - (int32_t)(age / 3u) - (int32_t)(h % 20u), py = dy + 10 + (int32_t)((h >> 8) % 60u) - (int32_t)(age / 8u);
            cv_rect(px, py, 2, 2, mix565(RGB(80, 10, 10), RGB(255, 160, 60), ui.win[k]));
        }
}

/* ====================================================================== STORM === */
static void sm_cloud(int32_t x, int32_t i, uint32_t h)
{
    int32_t y = 4 + (int32_t)(h % 40u), w = 40 + (int32_t)((h >> 6) % 40u);
    uint16_t c = mix565(RGB(60, 6, 10), RGB(170, 40, 30), (h >> 12) & 127u);
    (void)i;
    cv_rect(x + 6, y, w - 12, 4, c);
    cv_rect(x, y + 4, w, 8, c);
    cv_rect(x + 10, y + 12, w - 20, 4, mix565(c, C_BLACK, 80));
}
static void sm_pylon(int32_t x, int32_t i, uint32_t h)
{
    uint16_t c = RGB(8, 2, 4);
    (void)i;
    (void)h;
    cv_line(x, GROUND, x + 10, GROUND - 50, c);
    cv_line(x + 20, GROUND, x + 10, GROUND - 50, c);
    cv_line(x + 3, GROUND - 16, x + 17, GROUND - 16, c);
    cv_line(x + 6, GROUND - 32, x + 14, GROUND - 32, c);
    cv_rect(x - 4, GROUND - 42, 28, 2, c);
    cv_line(x - 4, GROUND - 41, x - 80, GROUND - 34, RGB(40, 6, 8));
}
static void scene_storm(void)
{
    grad(0, GROUND, RGB(14, 2, 6), RGB(170, 34, 24));
    layer(0.12f, 50, 11, 80, sm_cloud);
    layer(0.3f, 70, 12, 80, sm_cloud);
    cv_rect(0, GROUND, 240, 20, RGB(10, 2, 4));
    layer(0.8f, 140, 13, 30, sm_pylon);
}

/* ====================================================================== TOWER === */
static void scene_tower(void)
{
    int32_t tx = 120 - (int32_t)(film.sx * 0.2f) % 260 + 60, y, r;
    uint32_t nn = eng_notes(), k;
    if (tx < -40)
        tx += 260;
    grad(0, GROUND, RGB(2, 4, 18), RGB(20, 50, 80));
    stars(70, 5);
    for (k = 0; k < 4u; k++) {                              /* the rings: a new one at each note */
        r = (int32_t)((film.frame - film.cut_frame + k * 16u) % 64u) * 2;
        if (nn != film.notes_seen || r > 0) {
            uint16_t c = mix565(RGB(255, 60, 60), RGB(2, 10, 30), (uint32_t)r * 2u);
            int32_t s;
            for (s = 0; s < 24; s++) {
                float a = (float)s / 24.0f;
                cv_pset(tx + (int32_t)(cos_turns(a) * (float)r), 16 + (int32_t)(fm_sin_turns(a) * (float)r * 0.6f), c);
            }
        }
    }
    for (y = 18; y < GROUND; y += 8) {                      /* the tower's lattice */
        int32_t w = (y - 18) / 5 + 2;
        cv_line(tx - w, y, tx + w, y + 8, RGB(40, 44, 60));
        cv_line(tx + w, y, tx - w, y + 8, RGB(40, 44, 60));
    }
    cv_line(tx, 14, tx - 18, GROUND, RGB(60, 64, 80));
    cv_line(tx, 14, tx + 18, GROUND, RGB(60, 64, 80));
    cv_rect(tx - 1, 12, 3, 3, (film.frame & 16u) || lvl01() > 0.3f ? RGB(255, 40, 40) : RGB(60, 10, 10));
    cv_rect(0, GROUND, 240, 20, RGB(4, 8, 14));
    for (k = 0; k < 30u; k++)                               /* tall grass, swaying */
        cv_line((int32_t)(k * 8u), 119, (int32_t)(k * 8u) + (int32_t)(fm_sin_turns((float)((film.frame + k * 5u) % 80u) / 80.0f) * 2.0f), 108 - (int32_t)(hsh(k, 6u) % 6u), RGB(10, 20, 26));
}


/* ===================================================================== ARCADE === */
static void ar_cab(int32_t x, int32_t i, uint32_t h)
{
    static const uint16_t SCR[4] = {RGB(60, 200, 255), RGB(255, 60, 170), RGB(255, 200, 60), RGB(80, 255, 120)};
    uint16_t sc = SCR[h & 3u], body = RGB(20, 12, 30);
    uint32_t l = lit(h), k;
    int32_t y = GROUND - 62;
    (void)i;
    cv_rect(x, y, 30, 62, body);                            /* the cabinet */
    cv_rect(x + 2, y + 2, 26, 7, mix565(sc, C_BLACK, 90));  /* the marquee */
    cv_rect(x + 3, y + 12, 24, 20, RGB(4, 4, 10));          /* the screen: its game, lit by its key */
    for (k = 0; k < 6u; k++) {
        uint32_t q = hsh(h, k + film.frame / 6u);
        cv_rect(x + 4 + (int32_t)(q % 20u), y + 13 + (int32_t)((q >> 8) % 17u), 2, 2, mix565(RGB(20, 20, 30), sc, 90u + l * 2u / 3u));
    }
    cv_rect(x + 3, y + 36, 24, 4, RGB(40, 30, 50));         /* the panel: a stick, buttons */
    cv_rect(x + 8, y + 33, 2, 4, RGB(200, 40, 40));
    cv_rect(x + 16, y + 37, 2, 2, l ? C_WHITE : RGB(160, 160, 40));
    cv_rect(x + 20, y + 37, 2, 2, RGB(60, 160, 255));
    cv_rect(x - 2, y + 12, 2, 50, mix565(sc, C_BLACK, 200u - l / 2u));   /* the screen's light on the next one */
}
static void scene_arcade(void)
{
    int32_t y, x;
    cv_rect(0, 0, 240, GROUND, RGB(10, 4, 18));
    for (x = -((int32_t)(film.sx * 0.3f) % 40); x < 240; x += 40) {   /* neon on the ceiling */
        cv_rect(x, 6, 30, 2, (x / 40) & 1 ? RGB(255, 60, 170) : RGB(60, 200, 255));
        cv_rect(x, 8, 30, 1, mix565((x / 40) & 1 ? RGB(255, 60, 170) : RGB(60, 200, 255), RGB(10, 4, 18), 160));
    }
    for (y = GROUND; y < 120; y += 4)                       /* the checkered floor, running away */
        for (x = -((int32_t)film.sx % 16) - 16; x < 240; x += 16)
            cv_rect(x + (((y - GROUND) / 4) & 1) * 8, y, 8, 4, RGB(26, 20, 30));
    for (y = GROUND; y < 120; y += 4)
        cv_rect(0, y, 240, 1, mix565(RGB(255, 60, 170), RGB(10, 4, 18), 200u));
    layer(1.0f, 38, 14, 34, ar_cab);
}

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
