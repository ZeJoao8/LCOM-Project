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
extern uint8_t    *player_p2_pix;     extern xpm_image_t player_p2_img;
extern uint8_t    *coin_pix[COIN_FRAMES]; extern xpm_image_t coin_img[COIN_FRAMES];
extern uint8_t    *missile_pix;       extern xpm_image_t missile_img;
extern uint8_t    *instructions_pix;  extern xpm_image_t instructions_img;
extern uint8_t    *shield_pix;        extern xpm_image_t shield_img;
extern uint8_t    *magnet_pix;        extern xpm_image_t magnet_img;

/* ── Primitives + text (view_draw.c) ──────────────────────────────────────── */
/** Fill a rectangle in the back buffer (clipped to the screen). */
void     draw_rect(int x, int y, int w, int h, uint32_t color);
/** Draw one glyph @p c of the 5x7 bitmap font at scale @p s. */
void     draw_char(char c, int x, int y, int s, uint32_t color);
/** Draw a left-aligned string at (x,y) with scale @p s. */
void     draw_text(const char *str, int x, int y, int s, uint32_t color);
/** Draw a string with a drop shadow for readability. */
void     draw_text_sh(const char *str, int x, int y, int s, uint32_t color);
/** Draw a string horizontally centred on @p cx. */
void     draw_text_c(const char *s, int cx, int y, int scale, uint32_t col);
/** Draw a horizontally-centred string with a drop shadow. */
void     draw_text_c_sh(const char *s, int cx, int y, int scale, uint32_t col);
/** Format integer @p val into @p buf (size @p bufsz). */
void     int_to_str(int val, char *buf, int bufsz);
/** Integer square root (floor) of @p v. */
int      isqrt_i(int v);
/** Linearly interpolate between colours @p a and @p b by @p t (0..255). */
uint32_t lerp_col(uint32_t a, uint32_t b, int t);
/** Darken the whole frame buffer: t=0 black .. 255 full brightness. */
void     fade_buffer(int t);

/* ── Sprite blitting (view_sprites.c) ─────────────────────────────────────── */
/** Blit a sprite at (x,y) scaled by integer factor @p s. */
void draw_sprite(uint8_t *pix, xpm_image_t img, int x, int y, int s);
/** Blit a sprite tilted by @p slope (vertical shear, used for the player). */
void draw_sprite_tilt(uint8_t *pix, xpm_image_t img, int x, int y, int slope);

/* ── World entities (view_world.c) ────────────────────────────────────────── */
/** Draw all active particles. */
void draw_particles(void);
/** Draw the scrolling floor with seams and hazard dashes. */
void draw_floor(void);
/** Current coin animation frame index (0..COIN_FRAMES-1). */
int  coin_frame(void);
/** Draw a zapper beam with its glowing end orbs. */
void draw_beam(const Zapper *z);
/** Draw the jetpack exhaust flame at (x,y). */
void draw_flame(int x, int y);
/** Draw a floating power-up pickup. */
void draw_powerup(const PowerUp *pu);
/** Draw the pulsing shield aura around the player at (px,py). */
void draw_shield_aura(int px, int py);
/** Draw the opponent "ghost" at vertical position @p y (2-player mode). */
void draw_ghost(int y);

/* ── Background (view_background.c) ────────────────────────────────────────── */
/** Draw the procedural parallax background (sky, moon, clouds, skylines). */
void draw_parallax(void);

/* ── UI widgets (view_ui.c) ───────────────────────────────────────────────── */
/** Draw the mouse cursor at the model's current pointer position. */
void draw_cursor(void);
/** Draw a labelled button, highlighted when selected/hovered. */
void draw_button(int x, int y, int w, int h, const char *label,
                 int tscale, bool highlight);

#endif /* _GAME_VIEW_INTERNAL_H_ */
