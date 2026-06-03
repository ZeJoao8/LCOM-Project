#ifndef _GAME_VIEW_INTERNAL_H_
#define _GAME_VIEW_INTERNAL_H_

/**
 * @file view_internal.h
 * @brief Private state and helpers shared between the view_*.c translation units.
 *        NOT part of the public view API (game/view.h). Holds the current frame
 *        context, the loaded sprites, and the drawing primitives every view
 *        module builds on.
 */

#include <lcom/lcf.h>        /* xpm_image_t */
#include "game/entities.h"   /* GameModel, Zapper, PowerUp, config constants */

/* ── Current frame context (defined in view.c, set by view_render) ────────── */
extern const GameModel *M;
extern int scr_w, scr_h;
extern int g_scroll_x;   /* world scroll accumulator (advances only while PLAYING) */

/* ── Sprites (defined in view_sprites.c, loaded by view_init) ─────────────── */
extern bool        sprites_ok;
extern uint8_t    *player_pix;        extern xpm_image_t player_img;
extern uint8_t    *coin_pix[COIN_FRAMES]; extern xpm_image_t coin_img[COIN_FRAMES];
extern uint8_t    *missile_pix;       extern xpm_image_t missile_img;
extern uint8_t    *instructions_pix;  extern xpm_image_t instructions_img;
extern uint8_t    *shield_pix;        extern xpm_image_t shield_img;
extern uint8_t    *magnet_pix;        extern xpm_image_t magnet_img;

/* ── Primitives + text (view_draw.c) ──────────────────────────────────────── */
void     draw_rect(int x, int y, int w, int h, uint32_t color);
void     draw_char(char c, int x, int y, int s, uint32_t color);
void     draw_text(const char *str, int x, int y, int s, uint32_t color);
void     draw_text_sh(const char *str, int x, int y, int s, uint32_t color);
void     draw_text_c(const char *s, int cx, int y, int scale, uint32_t col);
void     draw_text_c_sh(const char *s, int cx, int y, int scale, uint32_t col);
void     int_to_str(int val, char *buf, int bufsz);
int      isqrt_i(int v);
uint32_t lerp_col(uint32_t a, uint32_t b, int t);
void     fade_buffer(int t);   /* darken the whole frame: t=0 black .. 255 full */

/* ── Sprite blitting (view_sprites.c) ─────────────────────────────────────── */
void draw_sprite(uint8_t *pix, xpm_image_t img, int x, int y, int s);
void draw_sprite_tilt(uint8_t *pix, xpm_image_t img, int x, int y, int slope);

/* ── World entities (view_world.c) ────────────────────────────────────────── */
void draw_particles(void);
void draw_floor(void);
int  coin_frame(void);
void draw_beam(const Zapper *z);
void draw_flame(int x, int y);
void draw_powerup(const PowerUp *pu);
void draw_shield_aura(int px, int py);
void draw_ghost(int y);

/* ── Background (view_background.c) ────────────────────────────────────────── */
void draw_parallax(void);

/* ── UI widgets (view_ui.c) ───────────────────────────────────────────────── */
void draw_cursor(void);
void draw_button(int x, int y, int w, int h, const char *label,
                 int tscale, bool highlight);

#endif /* _GAME_VIEW_INTERNAL_H_ */
