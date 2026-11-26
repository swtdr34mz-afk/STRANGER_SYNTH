/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (STRANGER SYNTH) */
/* STRANGER SYNTH's smooth drawing (the facelift): on the canvas (cv_px, cv_w wide, RGB565 stored byte-swapped) the
 * pixels blended rather than set: a dithered gradient (no bands), soft additive glows, soft shapes, anti-aliased lines
 * and arcs, light beams. Included by film.c (gfx.c and mix565 in scope); app.c's panel uses them too. */

static float at2(float y, float x)        /* atan2, a few thousandths of a radian off: enough for a ring */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a, s, r;
    if (ax < 1e-6f && ay < 1e-6f)
        return 0.0f;
    a = (ax < ay ? ax / ay : ay / ax);
    s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = 3.14159274f - r;
    return y < 0.0f ? -r : r;
}

static const uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

static inline void px_mix(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* blend c over a pixel, a 0..256 */
{
    uint16_t *p;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h || !a)
        return;
    p = cv_px + (uint32_t)y * cv_w + (uint32_t)x;
    *p = swap16(mix565(swap16(*p), c, a > 256u ? 256u : a));
}

static inline void px_add(int32_t x, int32_t y, uint32_t r, uint32_t g, uint32_t b)   /* light added: 0..255 each */
{
    uint16_t *p;
    uint32_t c, cr, cg, cb;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    p = cv_px + (uint32_t)y * cv_w + (uint32_t)x;
    c = swap16(*p);
    cr = ((c >> 11) & 31u) + (r >> 3);
    cg = ((c >> 5) & 63u) + (g >> 2);
    cb = (c & 31u) + (b >> 3);
    *p = swap16(((cr > 31u ? 31u : cr) << 11) | ((cg > 63u ? 63u : cg) << 5) | (cb > 31u ? 31u : cb));
}

/* a sky, rows y0..y1-1, c0 at the top to c1 (eased), dithered so it has no bands */
static void sgrad(int32_t y0, int32_t y1, const uint8_t c0[3], const uint8_t c1[3], float ease)
{
    int32_t y, x, n = y1 - y0;
    for (y = y0; y < y1; y++) {
        float t = (float)(y - y0) / (float)(n > 1 ? n - 1 : 1), r, g, b;
        uint16_t *row = cv_px + (uint32_t)y * cv_w;
        if (ease != 1.0f)
            t = fm_powf(t, ease);
        r = (float)c0[0] + ((float)c1[0] - (float)c0[0]) * t;   /* in 5/6-bit steps, plus a 4x4 threshold */
        g = (float)c0[1] + ((float)c1[1] - (float)c0[1]) * t;
        b = (float)c0[2] + ((float)c1[2] - (float)c0[2]) * t;
        r *= 31.0f / 255.0f;
        g *= 63.0f / 255.0f;
        b *= 31.0f / 255.0f;
        for (x = 0; x < (int32_t)cv_w; x++) {
            float th = ((float)BAYER4[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
            uint32_t R = (uint32_t)(r + th), G = (uint32_t)(g + th), B = (uint32_t)(b + th);
            row[x] = swap16(((R > 31u ? 31u : R) << 11) | ((G > 63u ? 63u : G) << 5) | (B > 31u ? 31u : B));
        }
    }
}

/* a glow: light added in an ellipse, strongest in the middle, fading softly to nothing at its edge */
static void glow(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t r, uint32_t g, uint32_t b, float k)
{
    int32_t x, y;
    float irx = 1.0f / (float)rx, iry = 1.0f / (float)ry;
    for (y = cy - ry; y <= cy + ry; y++) {
        float dy = (float)(y - cy) * iry, dy2 = dy * dy;
        if ((uint32_t)y >= cv_h)
            continue;
        for (x = cx - rx; x <= cx + rx; x++) {
            float dx = (float)(x - cx) * irx, q = 1.0f - (dx * dx + dy2);
            if (q <= 0.0f)
                continue;
            q *= q * k;
            px_add(x, y, (uint32_t)((float)r * q), (uint32_t)((float)g * q), (uint32_t)((float)b * q));
        }
    }
}

/* a soft shape: c over an ellipse, a = its opacity in the middle (0..256), the edge feathered */
static void soft_ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c, uint32_t a)
{
    int32_t x, y;
    float irx = 1.0f / (float)rx, iry = 1.0f / (float)ry;
    for (y = cy - ry; y <= cy + ry; y++) {
        float dy = (float)(y - cy) * iry, dy2 = dy * dy;
        for (x = cx - rx; x <= cx + rx; x++) {
            float dx = (float)(x - cx) * irx, q = 1.0f - (dx * dx + dy2);
            if (q <= 0.0f)
                continue;
            q = q > 0.35f ? 1.0f : q / 0.35f;
            px_mix(x, y, c, (uint32_t)((float)a * q));
        }
    }
}

/* an anti-aliased line, one pixel wide (Wu's): each step lights the two pixels it falls between, by how near */
static void aa_line(float x0, float y0, float x1, float y1, uint16_t c, uint32_t a)
{
    float dx = x1 - x0, dy = y1 - y0, adx = dx < 0.0f ? -dx : dx, ady = dy < 0.0f ? -dy : dy;
    int32_t i, n;
    if (adx >= ady) {
        float g;
        if (x1 < x0) {
            float t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
        }
        g = adx > 0.0f ? (y1 - y0) / (x1 - x0) : 0.0f;
        n = (int32_t)(x1 - x0 + 0.5f);
        for (i = 0; i <= n; i++) {
            float y = y0 + g * (float)i, f = y - fm_floorf(y);
            int32_t xi = (int32_t)(x0 + 0.5f) + i, yi = (int32_t)fm_floorf(y);
            px_mix(xi, yi, c, (uint32_t)((1.0f - f) * (float)a));
            px_mix(xi, yi + 1, c, (uint32_t)(f * (float)a));
        }
    } else {
        float g;
        if (y1 < y0) {
            float t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
        }
        g = (x1 - x0) / (y1 - y0);
        n = (int32_t)(y1 - y0 + 0.5f);
        for (i = 0; i <= n; i++) {
            float x = x0 + g * (float)i, f = x - fm_floorf(x);
            int32_t yi = (int32_t)(y0 + 0.5f) + i, xi = (int32_t)fm_floorf(x);
            px_mix(xi, yi, c, (uint32_t)((1.0f - f) * (float)a));
            px_mix(xi + 1, yi, c, (uint32_t)(f * (float)a));
        }
    }
}

/* a beam of light from (x, y), dir 1 right or -1 left, len long, opening to half-width w at its end: light added,
 * brightest at the lamp and along its middle */
static void beam(int32_t x, int32_t y, int d, int32_t len, int32_t w, uint32_t r, uint32_t g, uint32_t b, float k)
{
    int32_t i, j;
    for (i = 1; i < len; i++) {
        float t = (float)i / (float)len, hw = 1.0f + (float)w * t, fall = (1.0f - t) * (1.0f - t) * k;
        int32_t h = (int32_t)hw + 1;
        for (j = -h; j <= h; j++) {
            float e = 1.0f - fm_fabsf((float)j) / hw, q;
            if (e <= 0.0f)
                continue;
            q = e * fall;
            px_add(x + d * i, y + j, (uint32_t)((float)r * q), (uint32_t)((float)g * q), (uint32_t)((float)b * q));
        }
    }
}

/* a ring's arc, anti-aliased: from angle a0 to a1 (turns, 0 = right, clockwise on the screen), radius rad, thick */
static void aa_arc(float cx, float cy, float rad, float thick, float a0, float a1, uint16_t c, uint32_t alpha)
{
    int32_t x, y, r = (int32_t)(rad + thick) + 2;
    float half = thick * 0.5f;
    for (y = (int32_t)cy - r; y <= (int32_t)cy + r; y++)
        for (x = (int32_t)cx - r; x <= (int32_t)cx + r; x++) {
            float dx = (float)x - cx, dy = (float)y - cy, dist = fm_sqrtf(dx * dx + dy * dy), e = half + 0.5f - fm_fabsf(dist - rad), ang;
            if (e <= 0.0f)
                continue;
            ang = at2(dy, dx) * (1.0f / FM_TWO_PI);
            if (ang < 0.0f)
                ang += 1.0f;
            if (ang < a0)
                ang += 1.0f;
            if (ang > a1)
                continue;
            px_mix(x, y, c, (uint32_t)((e > 1.0f ? 1.0f : e) * (float)alpha));
        }
}
