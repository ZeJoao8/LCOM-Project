/**
 * @file view_background.c
 * @brief Procedural parallax backdrop: a sky gradient, a moon, drifting clouds
 *        and stars, and three scrolling city skylines (far / near / foreground).
 *        Drawn entirely in code — no image assets.
 */
#include "game/view_internal.h"

/** @brief Draw a filled disc of radius @p r centred on (cx,cy). */
static void fill_disc(int cx, int cy, int r, uint32_t col) {
    for (int dy = -r; dy <= r; dy++) {
        int hw = isqrt_i(r * r - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, col);
    }
}

/** @brief Draw the moon: a soft halo, the disc, and a few dim craters. */
static void draw_moon(void) {
    int cx = scr_w - 130, cy = 86, r = 30;
    fill_disc(cx, cy, r + 5, COL_MOON_GLOW);     /* halo  */
    fill_disc(cx, cy, r,     COL_MOON);          /* disc  */
    fill_disc(cx - 9, cy - 6, 5, COL_MOON_GLOW); /* craters */
    fill_disc(cx + 8, cy + 7, 4, COL_MOON_GLOW);
    fill_disc(cx + 3, cy - 9, 3, COL_MOON_GLOW);
}

/** @brief Draw a fluffy cloud blob at (x,y) sized by @p s from overlapping rects. */
static void draw_cloud(int x, int y, int s) {
    draw_rect(x,         y + s,     6 * s, 2 * s, COL_CLOUD);
    draw_rect(x + s,     y,         4 * s, 2 * s, COL_CLOUD);
    draw_rect(x + 3 * s, y - s + 2, 2 * s, 2 * s, COL_CLOUD);
}

/** @brief Draw three clouds drifting slowly leftward, wrapping around the screen. */
static void draw_clouds(void) {
    int drift = M->anim / 6;
    int span  = scr_w + 120;
    static const int cy[3] = { 70, 140, 110 };
    static const int cs[3] = { 6, 4, 5 };
    for (int i = 0; i < 3; i++) {
        int base = i * (span / 3) + i * 37;
        int x = scr_w - (((base + drift) % span));
        draw_cloud(x, cy[i], cs[i]);
    }
}

/**
 * @brief Draw one wrapping row of building silhouettes with lit windows.
 * @param scroll            Horizontal scroll offset in pixels.
 * @param height_from_floor Pixels the skyline base sits above the floor.
 * @param max_h             Maximum extra building height (deterministic per slot).
 * @param col               Building colour.
 */
static void draw_skyline(int scroll, int height_from_floor, int max_h, uint32_t col) {
    int bw = 46;                                    /* width slot per building   */
    int base_y = scr_h - FLOOR_H - height_from_floor;
    int n = scr_w / bw + 2;
    int sh = ((scroll % bw) + bw) % bw;
    for (int i = 0; i < n; i++) {
        int idx = i + scroll / bw;
        unsigned hsh = (unsigned) idx * 2654435761u; /* cheap deterministic hash */
        int bh = 24 + (int)(hsh % (unsigned) max_h);
        int x = i * bw - sh;
        draw_rect(x, base_y - bh, bw - 6, bh + height_from_floor, col);
        unsigned w = hsh;                            /* lit windows */
        for (int wy = base_y - bh + 6; wy < base_y - 4; wy += 13) {
            for (int wx = x + 5; wx < x + bw - 11; wx += 11) {
                w = w * 1103515245u + 12345u;
                if (w & 0x40000u) draw_rect(wx, wy, 3, 4, COL_STAR);
            }
        }
    }
}

/**
 * @brief Draw the full procedural parallax backdrop: sky gradient, moon, stars,
 *        drifting clouds and three scrolling skylines (far/near/foreground) at
 *        increasing speeds for depth. Falls back to a flat fill when visual
 *        effects are disabled.
 */
void draw_parallax(void) {
    if (!M->visual_effects) {
        draw_rect(0, 0, scr_w, scr_h - FLOOR_H, COL_BG);
        return;
    }
    int sky_h = scr_h - FLOOR_H;
    int bands = 8;
    for (int b = 0; b < bands; b++) {
        uint32_t col = lerp_col(COL_SKY_TOP, COL_SKY_BOT, b * 255 / (bands - 1));
        draw_rect(0, b * sky_h / bands, scr_w, sky_h / bands + 1, col);
    }
    draw_moon();
    for (int i = 0; i < 40; i++) {                  /* stars, gentle drift */
        int sx = (i * 97 + M->anim / 3) % scr_w;
        int sy = (i * 61) % (sky_h / 2);
        draw_rect(sx, sy, 1, 1, COL_STAR);
    }
    draw_clouds();
    draw_skyline(M->anim / 3, 140, 60,  COL_CITY_FAR);   /* far:  short, slow    */
    draw_skyline(M->anim,      60, 130, COL_CITY_NEAR);  /* near: tall,  fast    */
    draw_skyline(M->anim * 2,   0, 150, COL_CITY_FORE);  /* fore: tallest, fastest */
}
