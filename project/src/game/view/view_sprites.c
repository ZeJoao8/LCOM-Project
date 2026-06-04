/**
 * @file view_sprites.c
 * @brief XPM sprite data, loading (view_init) and blitting helpers (plain and
 *        tilted). Owns the sprite globals shared through view_internal.h.
 */
#include <lcom/lcf.h>
#include "game/view.h"                    /* view_init prototype */
#include "game/view_internal.h"
#include "devices/video_gr/video_gr.h"    /* vg_draw_pixmap */

/* 24-bit XPMs. '.' = #FF00FF is the transparency colour handled by vg_draw_pixmap.
 * Sprite pixel data lives in src/img / *.xpm (standard X PixMap files). */
#include "player.xpm"
#include "player_p2.xpm"
#include "coin0.xpm"
#include "coin1.xpm"
#include "coin2.xpm"
#include "coin3.xpm"
#include "missile.xpm"
#include "instructions.xpm"
#include "shield.xpm"
#include "magnet.xpm"

/* Sprites owned by the view (declared extern in view_internal.h). */
bool        sprites_ok = false;
uint8_t    *player_pix = NULL;
xpm_image_t player_img;
uint8_t    *player_p2_pix = NULL;
xpm_image_t player_p2_img;
uint8_t    *coin_pix[COIN_FRAMES] = { NULL };
xpm_image_t coin_img[COIN_FRAMES];
uint8_t    *missile_pix = NULL;
xpm_image_t missile_img;
uint8_t    *instructions_pix = NULL;
xpm_image_t instructions_img;
uint8_t    *shield_pix = NULL;
xpm_image_t shield_img;
uint8_t    *magnet_pix = NULL;
xpm_image_t magnet_img;

void view_init(void) {
    player_pix    = xpm_load((xpm_map_t) player_xpm,    XPM_8_8_8, &player_img);
    player_p2_pix = xpm_load((xpm_map_t) player_p2_xpm, XPM_8_8_8, &player_p2_img);
    coin_pix[0] = xpm_load((xpm_map_t) coin_xpm0,   XPM_8_8_8, &coin_img[0]);
    coin_pix[1] = xpm_load((xpm_map_t) coin_xpm1,   XPM_8_8_8, &coin_img[1]);
    coin_pix[2] = xpm_load((xpm_map_t) coin_xpm2,   XPM_8_8_8, &coin_img[2]);
    coin_pix[3] = xpm_load((xpm_map_t) coin_xpm3,   XPM_8_8_8, &coin_img[3]);
    missile_pix = xpm_load((xpm_map_t) missile_xpm, XPM_8_8_8, &missile_img);
    instructions_pix = xpm_load((xpm_map_t) instructions_xpm, XPM_8_8_8, &instructions_img);
    shield_pix  = xpm_load((xpm_map_t) shield_xpm,  XPM_8_8_8, &shield_img);
    magnet_pix  = xpm_load((xpm_map_t) magnet_xpm,  XPM_8_8_8, &magnet_img);

    sprites_ok = (player_pix && missile_pix &&
                  coin_pix[0] && coin_pix[1] && coin_pix[2] && coin_pix[3]);
}

static uint8_t xpm_bpp(xpm_image_t img) {
    switch (img.type) {
        case XPM_INDEXED:               return 1;
        case XPM_1_5_5_5:
        case XPM_5_6_5:                 return 2;
        case XPM_8_8_8:                 return 3;
        case XPM_8_8_8_8:               return 4;
        default:                        return 3;
    }
}

/* Draw a loaded XPM at integer scale s. s==1 uses vg_draw_pixmap. */
void draw_sprite(uint8_t *pix, xpm_image_t img, int x, int y, int s) {
    if (pix == NULL) return;
    if (s <= 1) {
        vg_draw_pixmap(pix, img, (uint16_t) x, (uint16_t) y);
        return;
    }
    uint8_t bpp = xpm_bpp(img);
    for (int row = 0; row < (int) img.height; row++) {
        for (int col = 0; col < (int) img.width; col++) {
            const uint8_t *p = pix + ((uint32_t) row * img.width + col) * bpp;
            uint32_t color = 0;
            for (uint8_t b = 0; b < bpp; b++)
                color |= (uint32_t) p[b] << (8 * b);
            if (color == 0xFF00FFu) continue;          /* transparent */
            draw_rect(x + col * s, y + row * s, s, s, color);
        }
    }
}

/* Draw a native sprite with per-row horizontal shear to fake a tilt. */
void draw_sprite_tilt(uint8_t *pix, xpm_image_t img, int x, int y, int slope) {
    if (pix == NULL) return;
    uint8_t bpp = xpm_bpp(img);
    int cy = img.height / 2;
    for (int row = 0; row < (int) img.height; row++) {
        int xo = (slope * (row - cy)) >> 8;
        for (int col = 0; col < (int) img.width; col++) {
            const uint8_t *p = pix + ((uint32_t) row * img.width + col) * bpp;
            uint32_t color = 0;
            for (uint8_t b = 0; b < bpp; b++)
                color |= (uint32_t) p[b] << (8 * b);
            if (color == 0xFF00FFu) continue;
            draw_rect(x + col + xo, y + row, 1, 1, color);
        }
    }
}
