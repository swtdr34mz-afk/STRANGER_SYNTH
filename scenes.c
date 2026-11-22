/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (STRANGER SYNTH) */
/* STRANGER SYNTH's places: the eight scenes of the film, drawn here. Included by film.c (its helpers in scope). */
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

