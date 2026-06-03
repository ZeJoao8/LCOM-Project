/**
 * @file view_background.c
 * @brief Procedural parallax backdrop: a sky gradient, drifting stars and two
 *        scrolling city skylines. Drawn entirely in code — no image assets.
 */
#include "game/view_internal.h"

/* One wrapping row of building silhouettes, scrolled by @p scroll px. */
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

/* Procedural parallax backdrop: sky gradient, stars, two scrolling skylines. */
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
    for (int i = 0; i < 40; i++) {                  /* stars, gentle drift */
        int sx = (i * 97 + M->anim / 3) % scr_w;
        int sy = (i * 61) % (sky_h / 2);
        draw_rect(sx, sy, 1, 1, COL_STAR);
    }
    draw_skyline(M->anim / 3, 140, 60,  COL_CITY_FAR);   /* far: short, slow */
    draw_skyline(M->anim,      60, 130, COL_CITY_NEAR);  /* near: tall, fast */
}
