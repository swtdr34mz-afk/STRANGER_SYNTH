/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (OFFWORLD) */
/* OFFWORLD's film: the 240 x 120 band under the title. Eight scenes of a city at night, all drawn here (no images),
 * each scrolling sideways in layers that move at their own speed, so it reads as a camera panning. Every 18 to 35 s
 * a cut (a tape glitch) to another scene, at random, with its place typed in the corner. In every scene the lights
 * follow the notes (windows, signs, screens: each one belongs to a key), the flames and the holograms follow the
 * loudness, RAIN brings the rain, and thunder lights the sky.
 * Included by app.c (after the drawing helpers): ui, set, KM, PR and the engine's calls are in scope. */

enum { SC_CITY, SC_STREET, SC_FLIGHT, SC_KOI, SC_REFINERY, SC_TVWALL, SC_ALLEY, SC_ROOFTOP, NSCENE };
static const char *const SCENE_NAME[NSCENE] = {
    "SECTOR 9 // SKYLINE",    "LEVEL 0 // NOODLE ROW", "AIRSPACE // NIGHT FLIGHT", "DISTRICT 4 // HOLO KOI",
    "OUTSKIRTS // REFINERY", "MARKET // TV WALL",     "BACKSTREET // NEON ALLEY", "ROOFTOP // SEARCHLIGHTS",
};

/* the little stories: a line now and then, typed after the place (our own; none from any film) */
static const char *const LOGS[] = {
    "RAIN FOR 41 DAYS NOW.",
    "SIGNAL LOST. TRYING AGAIN.",
    "THE NOODLE MAN KNOWS MY ORDER.",
    "NO STARS TONIGHT. NEVER STARS.",
    "SOMEONE LEFT A LIGHT ON FOR ME.",
    "THE KOI SWIMS. NOBODY LOOKS UP.",
    "MEMORY FILE 7 OF 12: DAMAGED.",
    "THE SHUTTLE IS LATE AGAIN.",
    "I DREAMED OF A REAL SUNRISE.",
    "THE CITY HUMS IN B FLAT.",
    "ALL THE CLOCKS SAY 3 A.M.",
    "A PHONE RINGS IN AN EMPTY ROOM.",
    "THE SNOW ON CHANNEL 9 IS WARM.",
    "WHO PAINTED THE SKY ORANGE?",
    "FOUND A PHOTO. NOBODY I KNOW.",
    "THE ELEVATOR ONLY GOES UP.",
    "THE NEON SPELLS A NAME I FORGOT.",
    "SIRENS. NOT FOR ME. NOT TONIGHT.",
    "THE RAIN TASTES OF IRON.",
    "LEVEL 0 NEVER SLEEPS.",
    "THE LAST TRAIN LEFT WITHOUT ME.",
    "MY HANDS REMEMBER THIS CHORD.",
    "THE WIND KNOWS EVERY ROOFTOP.",
    "STATIC ON EVERY CHANNEL. PEACEFUL.",
};
#define NLOGS (sizeof LOGS / sizeof LOGS[0])

#define NDROPS 90
#define GROUND 108
static struct {
    uint8_t scene, glitch, flicker, flash, car_on;
    uint8_t burst;                       /* REFINERY: a flame shooting up (frames) */
    int8_t log;                          /* the line typed under the place (-1: none) */
    uint32_t frame, cut_frame, cut_at, len;
    float sx;                            /* the camera's pan, in pixels */
    float blimp_x, car_x, car_y, car_v;
    uint32_t notes_seen, thunder_seen;
    int16_t bolt[8];
    float drop_x[NDROPS], drop_y[NDROPS], drop_v[NDROPS];
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

/* a sign's letter: 5 x 5 strokes from a seed (no real script: the look of one) */
static void sign_glyph(int32_t x, int32_t y, uint32_t h, uint16_t c)
{
    uint32_t r, i, bits = h & 0x1FFFFFFu;
    bits |= ((h >> 25) & 1u) ? 0x1Fu : 0x0084210u;   /* a top bar, or a stem */
    for (r = 0; r < 5u; r++)
        for (i = 0; i < 5u; i++)
            if (bits >> (r * 5u + i) & 1u)
                cv_rect(x + (int32_t)i, y + (int32_t)r, 1, 1, c);
}

/* a vertical neon sign: n letters in a dark box; lit by its key (bright, a glow at the edges), else dim */
static void sign_col(int32_t x, int32_t y, uint32_t n, uint32_t h, uint16_t c)
{
    uint32_t i, l = lit(h);
    uint16_t col = mix565(mix565(c, C_BLACK, 110), C_WHITE, l / 3u);
    if (!(l || ((h >> 7) % 5u)))
        col = mix565(c, C_BLACK, 190);   /* one in five dark (a tube gone) unless its key plays */
    cv_rect(x - 1, y - 2, 9, (int32_t)n * 7 + 3, RGB(18, 8, 20));
    if (l > 80u) {
        cv_rect(x - 2, y - 2, 1, (int32_t)n * 7 + 3, mix565(c, C_BLACK, 120));
        cv_rect(x + 8, y - 2, 1, (int32_t)n * 7 + 3, mix565(c, C_BLACK, 120));
    }
    for (i = 0; i < n; i++)
        sign_glyph(x + 1, y + (int32_t)i * 7, hsh(h, i), col);
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

static void street(int32_t y0, uint16_t c) { cv_rect(0, y0, 240, 120 - y0, c); }

/* a reflection in the wet street: a light's colour, rippling, fading downwards */
static void reflect(int32_t x, int32_t w, int32_t y0, uint16_t c, uint16_t ground)
{
    int32_t y;
    for (y = y0 + 1; y < 120; y += 2)
        cv_rect(x + (int32_t)((film.frame / 3u + (uint32_t)y) % 3u) - 1, y, w, 1,
                mix565(c, ground, (uint32_t)(y - y0) * 256u / (uint32_t)(120 - y0)));
}

static void flying_car(void)
{
    int32_t cx = (int32_t)film.car_x, cy = (int32_t)film.car_y, d = film.car_v > 0.0f ? 1 : -1;
    uint32_t i;
    if (!film.car_on)
        return;
    for (i = 1; i < 6u; i++)
        cv_rect(cx - d * (int32_t)i * 4, cy + 1, 3, 1, mix565(RGB(10, 10, 24), C_CYAN, 200u - i * 35u));
    cv_rect(cx - 3, cy, 7, 3, RGB(60, 60, 80));
    cv_rect(d > 0 ? cx + 4 : cx - 5, cy + 1, 2, 1, RGB(255, 255, 230));
    cv_rect(d > 0 ? cx - 4 : cx + 4, cy + 1, 1, 1, RGB(255, 40, 40));
}

static void blimp(int32_t by)
{
    const char *nm = PR.name;
    int32_t w = text_w(&FONT_XS, nm) + 16, bx = (int32_t)film.blimp_x;
    cv_line(bx + w / 2, by + 16, bx + w / 2 - 30, 88, RGB(70, 52, 60));
    cv_line(bx + w / 2 + 6, by + 16, bx + w / 2 + 40, 88, RGB(70, 52, 60));
    cv_rect(bx + 4, by, w - 8, 18, RGB(28, 26, 40));
    cv_rect(bx, by + 3, w, 12, RGB(28, 26, 40));
    cv_rect(bx + 6, by + 3, w - 12, 12, RGB(40, 8, 40));
    cv_text(bx + 8, by + 3, &FONT_XS, nm, (film.frame & 31u) < 29u ? C_MAG : RGB(140, 40, 110));
    cv_rect(bx + w / 2 - 4, by + 18, 8, 3, RGB(28, 26, 40));
    if ((film.frame & 15u) < 8u)
        cv_rect(bx + w - 3, by + 7, 2, 2, RGB(255, 40, 40));
}

/* a tower's lit windows: a grid, some dim, those of the keys that play bright */
static void windows(int32_t x, int32_t y0, int32_t w, int32_t y1, int32_t px, int32_t py, int32_t ww, int32_t wh,
                    uint32_t h, uint16_t dim, uint16_t warm)
{
    int32_t xx, yy;
    for (yy = y0 + 4; yy + wh < y1; yy += py)
        for (xx = x + 3; xx + ww < x + w - 2; xx += px) {
            uint32_t q = hsh(h, (uint32_t)((yy << 8) + xx - x)), l = lit(q);
            if (l && (q >> 9) % 3u == 0u)
                cv_rect(xx, yy, ww, wh, mix565(dim, warm, 60u + l * 196u / 255u));
            else if ((q >> 12) % 7u == 0u)
                cv_rect(xx, yy, ww, wh, dim);
        }
}

/* ===================================================================== CITY === */
static void city_far(int32_t x, int32_t i, uint32_t h)
{
    int32_t hh = 18 + (int32_t)(h % 40u);
    (void)i;
    cv_rect(x, GROUND - hh, 6, hh, RGB(24, 18, 44));
    if (h & 1u)
        cv_rect(x + 2, GROUND - hh + 4 + (int32_t)((h >> 4) & 15u), 1, 1, RGB(120, 80, 50));
}
static void city_mid(int32_t x, int32_t i, uint32_t h)
{
    int32_t w = 18 + (int32_t)(h % 10u), hh = 40 + (int32_t)((h >> 4) % 45u), y = GROUND - hh;
    (void)i;
    cv_rect(x, y, w, hh, RGB(14, 12, 30));
    windows(x, y, w, GROUND, 4, 5, 2, 2, h, RGB(50, 44, 40), RGB(255, 200, 100));
    if ((h >> 9) & 1u)
        cv_rect((h >> 10) & 1u ? x : x + w - 1, y, 1, hh, (h >> 11) & 1u ? C_CYAN : C_MAG);
    if (((h >> 12) & 3u) == 0u) {
        cv_rect(x + w / 2, y - 10, 1, 10, RGB(40, 40, 60));
        if (((film.frame + h) & 31u) < 10u)
            cv_rect(x + w / 2 - 1, y - 12, 3, 2, RGB(255, 40, 40));
    }
}
static void city_near(int32_t x, int32_t i, uint32_t h)
{
    int32_t w = 30 + (int32_t)(h % 14u), hh = 82 + (int32_t)((h >> 5) % 24u), y = GROUND - hh;
    uint16_t c = (h >> 8) & 1u ? C_MAG : C_CYAN;
    (void)i;
    cv_rect(x, y, w, hh, RGB(8, 8, 18));
    if ((h >> 10) & 1u) {                /* a stepped top */
        cv_rect(x + 6, y - 6, w - 12, 6, RGB(8, 8, 18));
        cv_rect(x + w / 2 - 1, y - 18, 2, 12, RGB(40, 40, 60));
    }
    windows(x, y, w, GROUND, 6, 8, 4, 3, h, RGB(36, 32, 36), RGB(255, 190, 90));
    sign_col(x - 10, y + 10, 4, h, c);
    reflect(x - 9, 7, GROUND, lit(h) ? mix565(c, C_WHITE, 80) : mix565(c, C_BLACK, 140), RGB(8, 6, 18));
}
static void scene_city(void)
{
    grad(0, 90, RGB(6, 4, 22), RGB(150, 64, 36));
    street(GROUND, RGB(8, 6, 18));
    layer(0.15f, 7, 1, 8, city_far);
    blimp(10);
    layer(0.45f, 34, 2, 30, city_mid);
    flying_car();
    layer(1.0f, 95, 3, 60, city_near);
}

/* ==================================================================== STREET === */
static void street_back(int32_t x, int32_t i, uint32_t h)
{
    (void)i;
    cv_rect(x, 0, 38, 58, (h & 1u) ? RGB(16, 18, 28) : RGB(20, 20, 32));
    windows(x, 0, 38, 50, 6, 9, 3, 4, h, RGB(40, 50, 56), RGB(120, 230, 255));
}
static void street_shop(int32_t x, int32_t i, uint32_t h)
{
    static const uint16_t AWN[4] = {RGB(170, 30, 40), RGB(20, 120, 130), RGB(190, 120, 30), RGB(110, 30, 120)};
    static const uint16_t NEON[4] = {C_MAG, C_CYAN, C_AMBER, RGB(120, 255, 140)};
    uint16_t aw = AWN[h & 3u], ne = NEON[(h >> 2) & 3u];
    uint32_t l = lit(h), s;
    int32_t y;
    (void)i;
    for (y = 62; y < 100; y += 2)        /* the room inside, warm */
        cv_rect(x + 2, y, 54, 2, mix565(RGB(80, 44, 18), RGB(170, 100, 40), (uint32_t)(y - 62) * 7u));
    cv_rect(x + 2, 92, 54, 8, RGB(34, 20, 18));   /* the counter */
    cv_rect(x + 8, 70, 40, 1, RGB(110, 70, 30));
    cv_rect(x, 38, 58, 17, RGB(14, 8, 16));       /* the sign */
    for (s = 0; s < 5u; s++)
        sign_glyph(x + 5 + (int32_t)s * 10, 44, hsh(h, s), l ? mix565(ne, C_WHITE, l / 3u) : mix565(ne, C_BLACK, 90));
    for (s = 0; s < 58u; s += 8u) {      /* the awning, striped */
        cv_rect(x + (int32_t)s, 55, 4, 7, aw);
        cv_rect(x + (int32_t)s + 4, 55, 4, 7, mix565(aw, C_WHITE, 90));
    }
    for (s = 0; s < 3u; s++) {           /* lanterns, and steam off the pots */
        int32_t lx = x + 8 + (int32_t)s * 18, rise = (int32_t)((film.frame + s * 11u + (h & 31u)) % 28u);
        cv_rect(lx, 63, 5, 6, RGB(230, 70, 40));
        cv_rect(lx + 1, 69, 3, 1, RGB(120, 30, 20));
        cv_rect(lx + 2 + (rise & 4 ? 1 : 0), 90 - rise, 3, 2, mix565(RGB(150, 150, 160), RGB(110, 70, 40), (uint32_t)rise * 9u));
    }
    reflect(x + 6, 46, 100, RGB(150, 90, 40), RGB(10, 10, 16));
    if (l)
        reflect(x + 12, 30, 100, mix565(ne, C_BLACK, 60), RGB(10, 10, 16));
}
static void street_people(int32_t x, int32_t i, uint32_t h)
{
    int32_t step = (int32_t)((film.frame / 4u + h) & 1u);
    uint16_t rim = (h >> 5) & 1u ? C_CYAN : C_MAG;
    (void)i;
    if (h % 4u == 0u)
        return;
    cv_rect(x + 2 + step, 96, 2, 6, RGB(8, 8, 12));   /* legs */
    cv_rect(x + 7 - step, 96, 2, 6, RGB(8, 8, 12));
    cv_rect(x, 82, 11, 15, RGB(12, 12, 18));          /* a coat */
    cv_rect(x + 3, 76, 5, 6, RGB(12, 12, 18));        /* a head */
    if ((h >> 3) & 1u) {                 /* an umbrella with a glowing rim */
        cv_rect(x - 4, 70, 19, 3, RGB(14, 14, 22));
        cv_rect(x - 2, 68, 15, 2, RGB(14, 14, 22));
        cv_rect(x - 4, 73, 19, 1, mix565(rim, C_BLACK, 120u - (uint32_t)(lvl01() * 110.0f)));
        cv_rect(x + 5, 73, 1, 9, RGB(40, 40, 50));
    }
}
static void scene_street(void)
{
    int32_t k;
    grad(0, 40, RGB(6, 12, 20), RGB(20, 34, 46));
    street(100, RGB(10, 10, 16));
    layer(0.3f, 40, 4, 40, street_back);
    for (k = 0; k < 3; k++) {            /* cables, sagging across */
        int32_t y = 8 + k * 9, x;
        for (x = 0; x < 240; x += 20)
            cv_line(x, y + (int32_t)((x % 80 - 40) * (x % 80 - 40)) / 300, x + 20,
                    y + (int32_t)(((x + 20) % 80 - 40) * ((x + 20) % 80 - 40)) / 300, RGB(30, 30, 40));
    }
    layer(0.7f, 62, 5, 60, street_shop);
    layer(1.4f, 47, 6, 20, street_people);
}

/* ==================================================================== FLIGHT === */
static void flight_tower(int32_t x, int32_t i, uint32_t h)
{
    int32_t w = 16 + (int32_t)(h % 16u), y;
    uint16_t edge = (h >> 6) & 1u ? C_CYAN : C_MAG;
    (void)i;
    cv_rect(x, 0, w, 120, RGB(10, 10, 22));
    for (y = 4 + (int32_t)(h % 5u); y < 116; y += 7) {
        uint32_t q = hsh(h, (uint32_t)y), l = lit(q);
        if ((q >> 3) % 3u == 0u)
            cv_rect(x + 3, y, w - 6, 1, l ? mix565(RGB(80, 60, 40), RGB(255, 220, 140), l) : RGB(60, 48, 40));
    }
    cv_rect(x, 0, 1, 120, mix565(edge, C_BLACK, 100));
}
static void scene_flight(void)
{
    uint32_t j;
    int32_t c;
    float t = film.sx * 0.006f;
    grad(0, 50, RGB(4, 6, 16), RGB(46, 34, 66));
    cv_rect(0, 50, 240, 70, RGB(6, 6, 14));
    for (j = 0; j < 16u; j++) {          /* the city below, rushing towards us */
        float u = (float)j / 16.0f + t, d = u - fm_floorf(u);
        int32_t y = 50 + (int32_t)(69.0f * d * d), sp = 3 + (int32_t)(30.0f * d * d), sz = d > 0.6f ? 2 : 1;
        uint32_t row = (uint32_t)(int32_t)fm_floorf(u * 16.0f);
        for (c = -12; c <= 12; c++) {
            uint32_t q = hsh(row * 31u + j, (uint32_t)(c + 40));
            uint16_t col;
            if ((q & 3u) == 0u)
                continue;
            col = (q >> 4) % 5u == 0u ? C_CYAN : (q >> 4) % 7u == 0u ? C_MAG : RGB(200, 140, 70);
            if (lit(q) > 60u)
                col = C_WHITE;
            cv_rect(120 + c * sp, y, sz, sz, mix565(RGB(6, 6, 14), col, 60u + (uint32_t)(d * 196.0f)));
        }
    }
    cv_rect(0, 48, 240, 3, RGB(54, 40, 76));
    for (c = 0; c < 3; c++) {            /* traffic far ahead */
        int32_t x = (int32_t)((film.frame * (uint32_t)(2 + c) + (uint32_t)c * 97u) % 300u) - 30, y = 20 + c * 9;
        cv_rect(x, y, 2, 1, RGB(255, 250, 220));
        cv_rect(x - 6, y, 5, 1, mix565(RGB(4, 6, 16), C_MAG, 120));
    }
    layer(3.2f, 85, 7, 34, flight_tower);
}

/* ======================================================================= KOI === */
static void koi_far(int32_t x, int32_t i, uint32_t h)
{
    int32_t hh = 20 + (int32_t)(h % 40u);
    (void)i;
    cv_rect(x, GROUND - hh, 8, hh, RGB(12, 26, 40));
    if (h & 3u)
        cv_rect(x + 3, GROUND - hh + 5 + (int32_t)((h >> 4) & 15u), 1, 1, RGB(60, 140, 160));
}
static void koi_mid(int32_t x, int32_t i, uint32_t h)
{
    int32_t w = 22 + (int32_t)(h % 12u), hh = 50 + (int32_t)((h >> 4) % 40u), y = GROUND - hh;
    (void)i;
    cv_rect(x, y, w, hh, RGB(8, 16, 28));
    windows(x, y, w, GROUND, 5, 6, 3, 2, h, RGB(30, 60, 70), RGB(150, 255, 255));
}
/* the hologram: a koi as tall as a tower, swimming through the street, its light in scanlines */
static void scene_koi(void)
{
    static const uint8_t R[16] = {4, 6, 8, 9, 9, 9, 8, 8, 7, 6, 5, 4, 3, 2, 3, 6};
    float bright = 0.45f + 0.55f * lvl01(), ph = (float)film.frame * 0.02f;
    int32_t kx = 260 - (int32_t)((film.frame * 3u / 4u) % 400u), cy = 52 + (int32_t)(8.0f * fm_sin_turns(ph * 0.15f));
    int32_t s, y, jit = (film.frame % 97u) < 3u ? 3 : 0;
    grad(0, 90, RGB(4, 10, 24), RGB(26, 46, 72));
    street(GROUND, RGB(6, 10, 18));
    layer(0.2f, 9, 8, 10, koi_far);
    layer(0.5f, 40, 9, 36, koi_mid);
    cv_line(kx + 40, GROUND, kx + 20, cy + 10, mix565(RGB(6, 10, 18), C_CYAN, 80));   /* the projector's light */
    cv_line(kx + 40, GROUND, kx + 60, cy + 8, mix565(RGB(6, 10, 18), C_CYAN, 80));
    cv_rect(kx + 36, GROUND - 3, 9, 3, RGB(40, 60, 70));
    for (s = 0; s < 16; s++) {
        int32_t px = kx + s * 5, r = R[s], py = cy + (int32_t)(fm_sin_turns(ph - (float)s * 0.07f) * (2.0f + (float)s * 0.25f));
        uint16_t c = mix565(mix565(C_CYAN, C_MAG, (uint32_t)s * 16u), C_BLACK, (uint32_t)((1.0f - bright) * 200.0f));
        for (y = py - r; y <= py + r; y += 2)   /* scanlines */
            cv_rect(px + ((y & 4) ? jit : 0), y, 5, 1, c);
        if (s == 3) {                    /* the fins */
            cv_rect(px, py - r - 4, 4, 1, c);
            cv_rect(px + 1, py + r + 3, 5, 1, c);
        }
        if (s == 15) {                   /* the tail fanned */
            cv_rect(px + 5, py - 9, 2, 1, c);
            cv_rect(px + 5, py + 9, 2, 1, c);
        }
    }
    cv_rect(kx + 3, cy - 1, 1, 1, C_WHITE);     /* the eye */
    reflect(kx + 10, 50, GROUND, mix565(C_CYAN, C_BLACK, 140), RGB(6, 10, 18));
    sign_col(206, 30, 6, 0xC0FFEEu, C_CYAN);
    flying_car();
}

/* ================================================================== REFINERY === */
static void ref_ridge(int32_t x, int32_t i, uint32_t h)
{
    int32_t hh = 6 + (int32_t)(h % 12u);
    (void)i;
    cv_rect(x, 92 - hh, 22, hh, RGB(70, 30, 16));
}
static void ref_plant(int32_t x, int32_t i, uint32_t h)
{
    uint16_t dk = RGB(22, 10, 8);
    uint32_t kind = h % 3u, l = lit(h);
    (void)i;
    if (kind == 0u) {                    /* a tank */
        cv_rect(x, 84, 30, 20, dk);
        cv_rect(x + 3, 81, 24, 3, dk);
        cv_rect(x + 2, 90, 26, 1, RGB(50, 22, 14));
        if (l)
            cv_rect(x + 13, 86, 3, 2, mix565(RGB(90, 30, 10), RGB(255, 200, 80), l));
    } else if (kind == 1u) {             /* a stack and its flame: higher when it's loud */
        int32_t hh = 50 + (int32_t)((h >> 4) % 30u), top = 104 - hh;
        int32_t fh = 4 + (int32_t)(urand() % 5u) + (int32_t)(lvl01() * 26.0f) + (int32_t)(l / 20u) +
                     (film.burst && (h & 1u) ? (int32_t)film.burst : 0);
        cv_rect(x + 10, top, 6, hh, dk);
        cv_rect(x + 11, top - fh, 4, fh, C_FIRE);
        cv_rect(x + 12, top - fh * 2 / 3, 2, fh * 2 / 3, RGB(255, 220, 90));
        cv_rect(x + 9 + (int32_t)(urand() % 7u), top - fh - 3, 1, 2, RGB(255, 80, 20));
        reflect(x + 10, 6, 104, RGB(200, 90, 30), RGB(14, 6, 6));
    } else {                             /* a lattice tower */
        int32_t hh = 56, top = 104 - hh, y;
        cv_rect(x + 4, top, 2, hh, dk);
        cv_rect(x + 20, top, 2, hh, dk);
        for (y = top; y < 100; y += 10) {
            cv_line(x + 5, y, x + 20, y + 10, dk);
            cv_line(x + 20, y, x + 5, y + 10, dk);
        }
        if (((film.frame + h) & 31u) < 12u)
            cv_rect(x + 12, top - 2, 2, 2, RGB(255, 40, 40));
    }
}
static void scene_refinery(void)
{
    int32_t dy, r = 18;
    grad(0, 92, RGB(40, 12, 10), RGB(235, 125, 45));
    for (dy = -r; dy <= r; dy++) {       /* a pale sun in the haze */
        int32_t w = (int32_t)fm_sqrtf((float)(r * r - dy * dy));
        cv_rect(176 - w, 34 + dy, 2 * w, 1, mix565(RGB(250, 170, 90), RGB(235, 125, 45), (uint32_t)((dy + r) * 3)));
    }
    cv_rect(0, 92, 240, 12, RGB(40, 16, 10));
    street(104, RGB(14, 6, 6));
    layer(0.1f, 20, 10, 22, ref_ridge);
    layer(0.5f, 64, 11, 32, ref_plant);
    cv_rect(0, 97, 240, 1, RGB(30, 14, 10));     /* pipes */
    cv_rect(0, 100, 240, 2, RGB(26, 12, 8));
}

/* ==================================================================== TV WALL === */
static void tv(int32_t x, int32_t y, uint32_t h)
{
    uint32_t l = lit(h), mode = (h + film.frame / 75u) % 4u, i, j;
    cv_rect(x, y, 32, 30, RGB(36, 34, 40));
    cv_rect(x + 27, y + 6, 2, 2, RGB(70, 66, 70));
    cv_rect(x + 27, y + 12, 2, 2, RGB(70, 66, 70));
    if (l) {                             /* its key plays: a wave on blue */
        cv_rect(x + 3, y + 3, 22, 24, mix565(RGB(4, 10, 30), RGB(20, 60, 120), l / 2u));
        for (i = 0; i < 22u; i++) {
            int32_t yy = y + 15 + (int32_t)(8.0f * fm_sin_turns((float)(i + film.frame) * 0.07f) * (float)l / 255.0f);
            cv_rect(x + 3 + (int32_t)i, yy, 1, 1, C_CYAN);
        }
        return;
    }
    if (mode == 0u) {                    /* snow */
        for (j = 0; j < 12u; j++)
            for (i = 0; i < 11u; i++) {
                uint32_t g = urand() & 127u;
                cv_rect(x + 3 + (int32_t)i * 2, y + 3 + (int32_t)j * 2, 2, 2, RGB(g, g, g + 20u));
            }
    } else if (mode == 1u) {             /* colour bars */
        static const uint16_t BAR[7] = {RGB(190, 190, 190), RGB(190, 190, 0), RGB(0, 190, 190), RGB(0, 190, 0),
                                        RGB(190, 0, 190), RGB(190, 0, 0), RGB(0, 0, 190)};
        for (i = 0; i < 7u; i++)
            cv_rect(x + 3 + (int32_t)i * 3, y + 3, 3, 24, mix565(BAR[i], C_BLACK, 90));
        cv_rect(x + 3, y + 3 + (int32_t)((film.frame + h) % 24u), 22, 1, RGB(220, 220, 220));
    } else if (mode == 2u) {             /* a face of light: three bars, rolling */
        cv_rect(x + 3, y + 3, 22, 24, RGB(10, 20, 30));
        for (i = 0; i < 3u; i++)
            cv_rect(x + 5, y + 6 + (int32_t)(((film.frame / 2u) + i * 8u + h) % 20u), 18, 2, RGB(60, 140, 170));
    } else
        cv_rect(x + 3, y + 3, 22, 24, RGB(8, 8, 12));
}
static void tv_stack(int32_t x, int32_t i, uint32_t h)
{
    uint32_t r;
    (void)i;
    for (r = 0; r < 3u; r++)
        tv(x, 6 + (int32_t)r * 33, hsh(h, r));
}
static void scene_tvwall(void)
{
    cv_rect(0, 0, 240, 120, RGB(10, 8, 14));
    layer(0.4f, 36, 12, 34, tv_stack);
    cv_rect(0, 104, 240, 16, RGB(24, 20, 24));   /* the shop's sill */
    if (film.car_on) {                   /* someone passes the window */
        int32_t x = (int32_t)film.car_x;
        cv_rect(x, 40, 34, 80, RGB(4, 4, 8));
        cv_rect(x + 10, 26, 14, 14, RGB(4, 4, 8));
        cv_rect(x - 12, 18, 58, 4, RGB(6, 6, 10));
        cv_rect(x - 12, 22, 58, 1, mix565(C_MAG, C_BLACK, 60));
    }
    cv_rect(0, 0, 3, 120, RGB(4, 4, 6));         /* the window's frame */
    cv_rect(118, 0, 3, 120, RGB(4, 4, 6));
    cv_rect(237, 0, 3, 120, RGB(4, 4, 6));
}

/* ===================================================================== ALLEY === */
static void alley_wall(int32_t x, int32_t i, uint32_t h)
{
    int32_t y;
    uint16_t c = (h & 1u) ? RGB(24, 12, 34) : RGB(30, 14, 42);
    uint16_t ne = (h >> 3) & 1u ? RGB(255, 40, 90) : C_MAG;
    (void)i;
    cv_rect(x, 8, 56, 96, c);
    windows(x, 10, 40, 96, 9, 12, 5, 6, h, RGB(70, 30, 90), RGB(255, 150, 230));
    for (y = 38; y < 96; y += 30)        /* balconies */
        cv_rect(x + 2, y, 36, 1, RGB(64, 40, 74));
    cv_rect(x + 30, 60 + (int32_t)(h % 20u), 8, 6, RGB(70, 60, 80));   /* an air conditioner */
    cv_rect(x + 31, 62 + (int32_t)(h % 20u), 6, 1, RGB(40, 34, 50));
    cv_rect(x + 41, 8, 1, 96, RGB(50, 34, 60));  /* a pipe */
    sign_col(x + 45, 20 + (int32_t)(h % 16u), 6, h, ne);
    reflect(x + 46, 7, 104, lit(h) ? mix565(ne, C_WHITE, 60) : mix565(ne, C_BLACK, 130), RGB(16, 8, 24));
}
static void alley_car(int32_t x, int32_t i, uint32_t h)
{
    int32_t k;
    (void)i;
    if (!(h & 1u))
        return;
    cv_rect(x, 97, 48, 6, RGB(18, 14, 26));      /* a low wedge coupe */
    for (k = 0; k < 6; k++)
        cv_rect(x + 12 + k * 2, 96 - k, 22 - k * 3, 1, RGB(18, 14, 26));
    cv_rect(x + 18, 92, 10, 3, RGB(40, 30, 60));
    cv_rect(x + 44, 98, 4, 2, RGB(255, 30, 40));   /* the tail light */
    cv_rect(x + 46, 97, 3, 1, RGB(255, 120, 120));
    reflect(x + 43, 5, 104, RGB(200, 20, 30), RGB(16, 8, 24));
    cv_rect(x + 5, 102, 9, 3, RGB(8, 8, 10));
    cv_rect(x + 34, 102, 9, 3, RGB(8, 8, 10));
}
static void scene_alley(void)
{
    grad(0, 24, RGB(30, 10, 50), RGB(90, 24, 100));
    street(104, RGB(16, 8, 24));
    layer(0.6f, 58, 13, 56, alley_wall);
    layer(1.0f, 170, 14, 50, alley_car);
}

/* =================================================================== ROOFTOP === */
static void roof_far(int32_t x, int32_t i, uint32_t h)
{
    int32_t hh = 10 + (int32_t)(h % 26u);
    (void)i;
    cv_rect(x, 84 - hh, 6, hh, RGB(14, 12, 26));
    if (h & 1u)
        cv_rect(x + 2, 84 - hh + 3, 1, 1, RGB(140, 100, 60));
}
static void roof_mid(int32_t x, int32_t i, uint32_t h)
{
    int32_t w = 16 + (int32_t)(h % 10u), hh = 30 + (int32_t)((h >> 4) % 30u);
    (void)i;
    cv_rect(x, 96 - hh, w, hh, RGB(10, 10, 20));
    windows(x, 96 - hh, w, 96, 4, 5, 2, 2, h, RGB(46, 40, 40), RGB(255, 200, 110));
}
static void scene_rooftop(void)
{
    int32_t i, k, sway = (int32_t)(1.5f * fm_sin_turns((float)film.frame * 0.013f));
    grad(0, 84, RGB(6, 6, 18), RGB(70, 44, 80));
    for (i = 0; i < 3; i++) {            /* searchlights, sweeping */
        int32_t bx = 40 + i * 80, dx = (int32_t)(80.0f * fm_sin_turns((float)film.frame * 0.004f + (float)i * 0.31f));
        for (k = -1; k <= 1; k++)
            cv_line(bx + k, 84, bx + dx + k * 4, 0, RGB(44, 44, 70));
    }
    layer(0.15f, 6, 15, 6, roof_far);
    layer(0.35f, 26, 16, 26, roof_mid);
    flying_car();
    cv_rect(0, 96, 240, 24, RGB(22, 22, 34));    /* the roof, wet */
    cv_rect(0, 96, 240, 1, RGB(60, 60, 84));
    cv_rect(0, 90, 240, 1, RGB(70, 70, 92));     /* its railing */
    for (i = 4; i < 240; i += 14)
        cv_rect(i, 90, 1, 6, RGB(70, 70, 92));
    cv_rect(18, 86, 16, 10, RGB(20, 20, 30));    /* vents, a tank, a mast */
    cv_rect(196, 84, 20, 12, RGB(20, 20, 30));
    cv_rect(88, 70, 20, 16, RGB(18, 18, 28));
    cv_rect(90, 86, 2, 10, RGB(18, 18, 28));
    cv_rect(104, 86, 2, 10, RGB(18, 18, 28));
    cv_rect(60, 46, 1, 50, RGB(40, 40, 54));
    if ((film.frame & 31u) < 10u)
        cv_rect(59, 44, 3, 2, RGB(255, 40, 40));
    /* someone stands at the edge in the rain, the city's light on one side */
    cv_rect(158, 84, 3, 12, RGB(6, 6, 10));
    cv_rect(163, 84, 3, 12, RGB(6, 6, 10));
    for (k = 0; k < 24; k++) {           /* a long coat, its hem moving */
        int32_t w = 8 + k / 5, off = k > 16 ? sway : 0;
        cv_rect(162 - w / 2 + off, 61 + k, w, 1, RGB(6, 6, 10));
        cv_rect(162 + w / 2 + off, 61 + k, 1, 1, mix565(C_CYAN, C_BLACK, 60));
    }
    cv_rect(156, 59, 13, 3, RGB(6, 6, 10));      /* shoulders, a head */
    cv_rect(159, 51, 6, 8, RGB(6, 6, 10));
    cv_rect(165, 52, 1, 6, mix565(C_CYAN, C_BLACK, 60));
    cv_rect(168, 59, 1, 2, mix565(C_CYAN, C_BLACK, 60));
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

static uint16_t px_tc = RGB(200, 190, 240);
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
static void px_flash(int32_t y1, uint32_t t) { px_tint(y1, t, px_tc); }

static void film_cut(uint32_t now)       /* to another scene, at random */
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
    eng_world(film.scene, (float)set.city / 100.0f);
}

static void film_init(void)
{
    uint32_t i;
    film.frame = 0;
    film.log = -1;
    for (i = 0; i < NDROPS; i++) {
        film.drop_x[i] = (float)(urand() % 250u);
        film.drop_y[i] = (float)(urand() % 120u);
        film.drop_v[i] = 4.0f + (float)(urand() % 40u) * 0.1f;
    }
    film.blimp_x = 150.0f;
    film.scene = (uint8_t)(urand() % NSCENE);
    film.sx = (float)(urand() % 4000u);
#ifdef PLAT_HOST
    if (getenv("STRANGER_SCENE"))        /* the host: one scene, held (the screenshots) */
        film.scene = (uint8_t)(atoi(getenv("STRANGER_SCENE")) % NSCENE);
#endif
}

static void film_step(void)              /* the movement, a frame */
{
    uint32_t i, rain_n = (uint32_t)ui.rain_now * NDROPS / 127u, th = eng_thunder(), nn = eng_notes();
    film.sx += film.scene == SC_FLIGHT ? 1.6f : 1.0f;
    for (i = 0; i < rain_n; i++) {
        film.drop_y[i] += film.drop_v[i];
        film.drop_x[i] -= film.drop_v[i] * 0.15f;
        if (film.drop_y[i] >= 120.0f) {
            film.drop_y[i] = -(float)(urand() % 20u);
            film.drop_x[i] = (float)(urand() % 250u);
        }
    }
    film.blimp_x -= 0.35f;
    if (film.blimp_x < -110.0f)
        film.blimp_x = 245.0f;
    if (!film.car_on && (nn != film.notes_seen || urand() % 300u == 0u)) {   /* a car (or someone, at the TVs) */
        film.car_on = 1;
        film.car_v = film.scene == SC_TVWALL ? -1.8f : (urand() & 1u) ? 2.6f : -2.6f;
        film.car_x = film.car_v > 0.0f ? -10.0f : 260.0f;
        film.car_y = (float)(24u + urand() % 34u);
        if (film.scene == SC_CITY || film.scene == SC_KOI || film.scene == SC_ROOFTOP)   /* heard flying past */
            eng_sfx(SFX_FLYBY, film.car_v > 0.0f ? 1.0f : -1.0f, 270.0f / (2.6f * 25.0f));
    }
    if (film.scene == SC_REFINERY) {     /* a flame shoots up, roaring */
        if (film.burst)
            film.burst--;
        else if (urand() % 150u == 0u) {
            film.burst = 30;
            eng_sfx(SFX_FLAME, 0.0f, 0.0f);
        }
    }
    if ((film.scene == SC_ROOFTOP || film.scene == SC_CITY) && urand() % 600u == 0u)   /* a siren, far off */
        eng_sfx(SFX_SIREN, (float)(urand() % 100u) / 100.0f, 0.0f);
    if (film.scene == SC_STREET && urand() % 70u == 0u)   /* a bowl set down at the counter */
        eng_sfx(SFX_CLINK, 0.0f, 0.0f);
    film.notes_seen = nn;
    if (film.car_on) {
        film.car_x += film.car_v;
        film.car_y += 0.08f;
        if (film.car_x < -60.0f || film.car_x > 270.0f)
            film.car_on = 0;
    }
    if (th != film.thunder_seen) {
        int32_t x = 30 + (int32_t)(urand() % 180u);
        film.thunder_seen = th;
        film.flash = 7;
        for (i = 0; i < 8u; i++) {
            film.bolt[i] = (int16_t)x;
            x += (int32_t)(urand() % 15u) - 7;
        }
    } else if (film.flash)
        film.flash--;
    if (film.flicker)
        film.flicker--;
    else if (urand() % 240u == 0u) {
        film.flicker = (uint8_t)(6u + urand() % 10u);
        if (film.scene == SC_ALLEY || film.scene == SC_STREET || film.scene == SC_CITY)   /* the tube crackles */
            eng_sfx(SFX_ZAP, 0.0f, 0.0f);
    }
    film.frame++;
}

static void film_frame(uint32_t now)
{
    uint32_t i, rain_n = (uint32_t)ui.rain_now * NDROPS / 127u, since;
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
    case SC_CITY: scene_city(); break;
    case SC_STREET: scene_street(); break;
    case SC_FLIGHT: scene_flight(); break;
    case SC_KOI: scene_koi(); break;
    case SC_REFINERY: scene_refinery(); break;
    case SC_TVWALL: scene_tvwall(); break;
    case SC_ALLEY: scene_alley(); break;
    default: scene_rooftop(); break;
    }
    if (set.story) {                     /* the night's light: dusk purple, dawn warm, the hours between as they are */
        int32_t mn = night_min();
        uint32_t tint = 0;
        uint16_t tc = 0;
        if (mn < 120) {
            tint = (uint32_t)(120 - mn) * 90u / 120u;
            tc = RGB(170, 70, 120);
        } else if (mn > 600) {
            tint = (uint32_t)(mn - 600) * 120u / 120u;
            tc = RGB(255, 160, 90);
        }
        if (tint && film.scene != SC_TVWALL)
            px_tint(film.scene == SC_STREET || film.scene == SC_ALLEY ? 30 : 80, tint, tc);
    }
    for (i = 0; i < rain_n; i++) {       /* the rain */
        int32_t dx = (int32_t)film.drop_x[i], dy = (int32_t)film.drop_y[i], len = 4 + (int32_t)(film.drop_v[i] * 0.6f);
        if (dy + len >= 119) {
            cv_rect(dx - 1, 118, 3, 1, RGB(90, 110, 140));
            continue;
        }
        cv_line(dx, dy, dx - 1, dy + len, C_RAIN);
    }
    if (film.flash >= 5u || film.flash == 3u)   /* the lightning */
        px_flash(film.scene == SC_TVWALL ? 120 : 90, film.flash >= 5u ? 150u : 70u);
    if (film.flash >= 4u && film.scene != SC_TVWALL)
        for (i = 0; i + 1u < 8u; i++)
            cv_line(film.bolt[i], (int32_t)i * 12, film.bolt[i + 1], (int32_t)(i + 1u) * 12, RGB(240, 235, 255));
    since = film.frame - film.cut_frame;
    if (since < 75u) {                   /* the place, typed in */
        char t[32];
        uint32_t n = since / 2u, k;
        for (k = 0; k < 31u && SCENE_NAME[film.scene][k] && k < n; k++)
            t[k] = SCENE_NAME[film.scene][k];
        t[k] = 0;
        if (k) {
            cv_rect(2, 2, text_w(&FONT_XS, t) + 8, 14, RGB(4, 4, 10));
            cv_text(5, 3, &FONT_XS, t, C_CYAN);
            if ((film.frame & 4u) || since > 70u)
                cv_rect(5 + text_w(&FONT_XS, t), 4, 5, 10, C_CYAN);
        }
    }
    if (film.log >= 0 && since >= 40u && since < 190u) {   /* the log line, typed after the place */
        char t[40];
        uint32_t n = (since - 40u) / 2u, k;
        for (k = 0; k < 39u && LOGS[film.log][k] && k < n; k++)
            t[k] = LOGS[film.log][k];
        t[k] = 0;
        if (k) {
            cv_rect(2, 17, text_w(&FONT_XS, t) + 8, 14, RGB(4, 4, 10));
            cv_text(5, 18, &FONT_XS, t, C_AMBER);
        }
    }
    if (set.story) {                     /* the night's clock */
        int32_t mn = night_min(), h = (19 + mn / 60) % 24, mi = mn % 60;
        char t[6];
        t[0] = (char)('0' + h / 10);
        t[1] = (char)('0' + h % 10);
        t[2] = (film.frame & 16u) ? ':' : ' ';
        t[3] = (char)('0' + mi / 10);
        t[4] = (char)('0' + mi % 10);
        t[5] = 0;
        cv_rect(203, 2, 35, 14, RGB(4, 4, 10));
        cv_text(206, 3, &FONT_XS, t, mn > 600 ? C_AMBER : C_MAG);
    }
    for (y = (int32_t)(film.frame & 3u); y < 120; y += 4)   /* scanlines, rolling slowly */
        px_dim_row(y);
    for (i = 0; i < 40u; i++)            /* grain */
        cv_rect((int32_t)(urand() % 240u), (int32_t)(urand() % 120u), 1, 1,
                (urand() & 1u) ? RGB(60, 60, 70) : C_BLACK);
    if (film.glitch) {                   /* the cut: the tape tears */
        uint32_t b;
        for (b = 0; b < 5u; b++) {
            int32_t y0 = (int32_t)(urand() % 110u), h = 2 + (int32_t)(urand() % 10u), d = (int32_t)(urand() % 40u) - 20;
            for (y = y0; y < y0 + h && y < 120; y++)
                px_row_shift(y, d);
        }
        cv_rect(0, (int32_t)(urand() % 120u), 240, 1, C_MAG);
        cv_rect(0, (int32_t)(urand() % 120u), 240, 1, C_CYAN);
        film.glitch--;
    }
    cv_blit(0, 24);
}
