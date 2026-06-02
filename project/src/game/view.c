/**
 * @file view.c
 * @brief All rendering: sprites, font, HUD and per-state screens. Reads a
 *        const GameModel and presents the frame; never mutates game state.
 */
#include <lcom/lcf.h>
#include "game/view.h"
#include "game/model.h"           /* isin / icos / zap_endpoints */
#include "devices/video_gr/video_gr.h"

/* ── Sprites ──────────────────────────────────────────────────────────────── */
/* 24-bit XPMs. '.' = #FF00FF is the transparency colour handled by vg_draw_pixmap. */
/* Sprite pixel data lives in src/img / *.xpm (standard X PixMap files).
 * Missile is a red rocket pointing left, exhaust trailing right (20x8 native). */
#include "player.xpm"
#include "coin0.xpm"
#include "coin1.xpm"
#include "coin2.xpm"
#include "coin3.xpm"
#include "missile.xpm"

/* Current frame context (set at the top of view_render). */
static const GameModel *M = NULL;
static int scr_w = 0, scr_h = 0;

/* Sprites owned by the view. */
static bool        sprites_ok = false;
static uint8_t    *player_pix = NULL;
static xpm_image_t player_img;
static uint8_t    *coin_pix[COIN_FRAMES] = { NULL };
static xpm_image_t coin_img[COIN_FRAMES];
static uint8_t    *missile_pix = NULL;
static xpm_image_t missile_img;

/* ── Low-level primitives ─────────────────────────────────────────────────── */

/* Filled rectangle, clipped to the screen. */
static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0)      { w += x; x = 0; }
    if (y < 0)      { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    vg_draw_rectangle((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, color);
}

/* 5x7 bitmap font — each row is a 5-bit pattern. */
static const uint8_t font_5x7[][7] = {
    ['0'] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    ['1'] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    ['2'] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    ['3'] = {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
    ['4'] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    ['5'] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    ['6'] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    ['7'] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    ['8'] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    ['9'] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    ['A'] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['B'] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    ['C'] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    ['D'] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    ['E'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    ['F'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    ['G'] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    ['H'] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['I'] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    ['J'] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    ['K'] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    ['L'] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    ['M'] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    ['N'] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    ['O'] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    ['P'] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    ['Q'] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    ['R'] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    ['S'] = {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
    ['T'] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    ['U'] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    ['V'] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    ['W'] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    ['X'] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    ['Y'] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    ['Z'] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    [':'] = {0x00,0x04,0x04,0x00,0x04,0x04,0x00},
    ['!'] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    ['/'] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    ['-'] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
};

static void draw_char(char c, int x, int y, int s, uint32_t color) {
    if (c < 0 || (unsigned char)c >= sizeof(font_5x7)/sizeof(font_5x7[0]))
        return;
    const uint8_t *glyph = font_5x7[(unsigned char)c];
    bool empty = true;
    for (int r = 0; r < 7; r++) if (glyph[r]) { empty = false; break; }
    if (empty && c != ' ')
        return; /* no glyph for this character */
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (glyph[row] & (0x10 >> col)) {
                draw_rect(x + col*s, y + row*s, s, s, color);
            }
        }
    }
}

static void draw_text(const char *str, int x, int y, int s, uint32_t color) {
    while (*str) {
        draw_char(*str, x, y, s, color);
        x += 6 * s; /* 5 wide + 1 space */
        str++;
    }
}

/* Text with a 1-unit drop shadow, so it stays legible over the busy backdrop. */
static void draw_text_sh(const char *str, int x, int y, int s, uint32_t color) {
    draw_text(str, x + s, y + s, s, COL_SHADOW);
    draw_text(str, x,     y,     s, color);
}

/* Convert an integer to a string. */
static void int_to_str(int val, char *buf, int bufsz) {
    if (bufsz < 2) return;
    if (val < 0) { buf[0] = '-'; buf++; bufsz--; val = -val; }
    int len = 0;
    char tmp[16];
    do { tmp[len++] = '0' + (val % 10); val /= 10; } while (val > 0 && len < 15);
    for (int i = len - 1; i >= 0 && bufsz > 1; i--) { *buf++ = tmp[i]; bufsz--; }
    *buf = '\0';
}

static int isqrt_i(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}


void view_init(void) {
    player_pix  = xpm_load((xpm_map_t) player_xpm,  XPM_8_8_8, &player_img);
    coin_pix[0] = xpm_load((xpm_map_t) coin_xpm0,   XPM_8_8_8, &coin_img[0]);
    coin_pix[1] = xpm_load((xpm_map_t) coin_xpm1,   XPM_8_8_8, &coin_img[1]);
    coin_pix[2] = xpm_load((xpm_map_t) coin_xpm2,   XPM_8_8_8, &coin_img[2]);
    coin_pix[3] = xpm_load((xpm_map_t) coin_xpm3,   XPM_8_8_8, &coin_img[3]);
    missile_pix = xpm_load((xpm_map_t) missile_xpm, XPM_8_8_8, &missile_img);

    sprites_ok = (player_pix && missile_pix &&
                  coin_pix[0] && coin_pix[1] && coin_pix[2] && coin_pix[3]);
}

/* ── Sprite blitting helpers ──────────────────────────────────────────────── */

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
static void draw_sprite(uint8_t *pix, xpm_image_t img, int x, int y, int s) {
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
static void draw_sprite_tilt(uint8_t *pix, xpm_image_t img, int x, int y, int slope) {
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

/* Draw all live particles and score popups. */
static void draw_particles(void) {
    for (int i = 0; i < MAX_PARTS; i++)
        if (M->parts[i].active)
            draw_rect(M->parts[i].x, M->parts[i].y, 3, 3, M->parts[i].col);
    for (int i = 0; i < MAX_POPUPS; i++)
        if (M->popups[i].active)
            draw_text("+10", M->popups[i].x, M->popups[i].y, 1, COL_SCORE);
}

/* Pick the current coin animation frame from the free-running clock. */
static int coin_frame(void) {
    static const int seq[6] = { 0, 1, 2, 3, 2, 1 };
    return seq[(M->anim / 4) % 6];
}

/* Glowing orb at a zapper tip. */
static void draw_orb(int cx, int cy) {
    for (int dy = -ORB_R; dy <= ORB_R; dy++) {
        int hw = isqrt_i(ORB_R * ORB_R - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, COL_ZAP_TIP);     /* shell */
    }
    int ir = ORB_R - 4;
    for (int dy = -ir; dy <= ir; dy++) {
        int hw = isqrt_i(ir * ir - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, COL_ZAP_GLOW);    /* glow */
    }
    draw_rect(cx - 2, cy - 2, 4, 4, COL_TEXT);                   /* centre dot */
}

/* Draw a zapper as an animated electric beam between two glowing orbs. */
static void draw_beam(const Zapper *z) {
    int ax, ay, bx, by;
    zap_endpoints(z, &ax, &ay, &bx, &by);

    int steps = z->half / 3;
    if (steps < 2) steps = 2;

    int perpx = icos(z->ang + ANG_VERT);
    int perpy = isin(z->ang + ANG_VERT);

    for (int s = 0; s <= steps; s++) {
        int bxp = ax + (bx - ax) * s / steps;
        int byp = ay + (by - ay) * s / steps;
        int w   = (isin(z->ang + M->anim * 3 + s * 6) * 5) / 1024;  /* ±5px wobble */
        int wx  = bxp + (perpx * w) / 1024;
        int wy  = byp + (perpy * w) / 1024;
        draw_rect(wx - 3, wy - 3, 6, 6, COL_ZAP_BODY);   /* amber glow */
        draw_rect(wx - 2, wy - 2, 4, 4, COL_ZAP_GLOW);   /* bright glow */
        draw_rect(wx - 1, wy - 1, 2, 2, COL_TEXT);       /* white core */
    }

    draw_orb(ax, ay);
    draw_orb(bx, by);
}

/* Layered jetpack flame, anchored at the player's back. */
static void draw_flame(int x, int y) {
    int f = 8 + (M->anim % 8);
    draw_rect(x,      y,           10, f,     COL_SCORE);     /* yellow core */
    draw_rect(x + 2,  y + f,        6, f / 2, 0xFF8800);      /* orange */
    draw_rect(x + 3,  y + f + f/2,  4, 3,     COL_GAMEOVER);  /* red tip */
}

/* Small mouse crosshair. */
static void draw_cursor(void) {
    draw_rect(M->mouse_x - 5, M->mouse_y,     11, 2, COL_SCORE);
    draw_rect(M->mouse_x,     M->mouse_y - 5, 2, 11, COL_SCORE);
}

/* Outlined button with centred label. */
static void draw_button(int x, int y, int w, int h, const char *label,
                        int tscale, bool highlight) {
    uint32_t bg  = highlight ? COL_TITLE  : COL_BTN_BG;
    uint32_t txt = highlight ? COL_BTN_HI : COL_TEXT;
    draw_rect(x, y, w, h, bg);
    draw_rect(x,         y,         w, 2, COL_OBS_TOP);
    draw_rect(x,         y + h - 2, w, 2, COL_OBS_TOP);
    draw_rect(x,         y,         2, h, COL_OBS_TOP);
    draw_rect(x + w - 2, y,         2, h, COL_OBS_TOP);
    int llen = 0;
    for (const char *p = label; *p; p++) llen++;
    int lx = x + (w - llen * 6 * tscale) / 2;
    int ly = y + (h - 7 * tscale) / 2;
    draw_text(label, lx, ly, tscale, txt);
}

/* Draw text horizontally centred on cx. */
static void draw_text_c(const char *s, int cx, int y, int scale, uint32_t col) {
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    draw_text(s, cx - n * 6 * scale / 2, y, scale, col);
}

/* Centred text with a drop shadow. */
static void draw_text_c_sh(const char *s, int cx, int y, int scale, uint32_t col) {
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    draw_text_sh(s, cx - n * 6 * scale / 2, y, scale, col);
}

/* Translucent-looking cyan silhouette of the 2-player opponent. */
static void draw_ghost(int y) {
    for (int row = 0; row < PLAYER_H; row += 2)        /* scanline fill = "ghosted" */
        draw_rect(PLAYER_X, y + row, PLAYER_W, 1, COL_GHOST);
    draw_rect(PLAYER_X, y, PLAYER_W, 1, COL_TEXT);
    draw_rect(PLAYER_X, y + PLAYER_H - 1, PLAYER_W, 1, COL_TEXT);
    draw_rect(PLAYER_X, y, 1, PLAYER_H, COL_TEXT);
    draw_rect(PLAYER_X + PLAYER_W - 1, y, 1, PLAYER_H, COL_TEXT);
    draw_text("P2", PLAYER_X + 2, y - 12, 1, COL_GHOST);
}

/* Linear interpolate two 24-bit colours, t in [0,255]. */
static uint32_t lerp_col(uint32_t a, uint32_t b, int t) {
    int ra=(a>>16)&0xFF, ga=(a>>8)&0xFF, ba=a&0xFF;
    int rb=(b>>16)&0xFF, gb=(b>>8)&0xFF, bb=b&0xFF;
    int r = ra + (rb - ra) * t / 255;
    int g = ga + (gb - ga) * t / 255;
    int bl= ba + (bb - ba) * t / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

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
static void draw_parallax(void) {
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

/* A floating power-up: glowing diamond tagged S (shield) or M (magnet). */
static void draw_powerup(const PowerUp *pu) {
    uint32_t col = (pu->type == PU_SHIELD) ? COL_SHIELD : COL_MAGNET;
    int cx = pu->x + PU_SIZE / 2;
    int cy = pu->y + PU_SIZE / 2 + (isin(M->anim * 4) * 3) / 1024;  /* bob */
    int r  = PU_SIZE / 2;
    for (int dy = -r; dy <= r; dy++) {              /* diamond */
        int hw = r - abs(dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, col);
    }
    draw_rect(cx - 2, cy - 2, 4, 4, COL_TEXT);      /* bright centre */
    draw_char((pu->type == PU_SHIELD) ? 'S' : 'M', cx - 2, cy - 3, 1, COL_SHADOW);
}

/* Pulsing ring drawn around the player while the shield is active. */
static void draw_shield_aura(int px, int py) {
    int cx = px + PLAYER_W / 2, cy = py + PLAYER_H / 2;
    int r  = PLAYER_H / 2 + 6 + (isin(M->anim * 6) * 3) / 1024;
    for (int dy = -r; dy <= r; dy++) {
        int hw = isqrt_i(r * r - dy * dy);
        draw_rect(cx - hw, cy + dy, 1, 1, COL_SHIELD);      /* left arc  */
        draw_rect(cx + hw, cy + dy, 1, 1, COL_SHIELD);      /* right arc */
    }
}

/* ── Per-state screens ────────────────────────────────────────────────────── */

static void render_menu(void) {
    clear_buffer(COL_MENU_BG);

    /* Decorative stars */
    for (int i = 0; i < 50; i++) {
        int sx = (i * 97 + M->anim * (i % 3 + 1)) % scr_w;
        int sy = (i * 73) % (scr_h - 100);
        draw_rect(sx, sy, 2, 2, COL_STAR);
    }

    /* Animated player preview */
    int preview_y = scr_h/4 - 30 + ((M->anim / 8) % 2 == 0 ? -8 : 8);
    draw_flame(scr_w/2 - PLAYER_W/2 + 1, preview_y + 33);
    if (sprites_ok)
        draw_sprite(player_pix, player_img,
                    scr_w/2 - PLAYER_W/2, preview_y, PLAYER_SCALE);
    else
        draw_rect(scr_w/2 - PLAYER_W/2, preview_y, PLAYER_W, PLAYER_H, COL_PLAYER);

    draw_text_sh("JETPACK JOYRIDE", scr_w/2 - 15*6*3/2, scr_h/4 + 10, 3, COL_TITLE);
    draw_text("LCOM EDITION", scr_w/2 - 12*6*2/2, scr_h/4 + 55, 2, COL_TEXT);

    /* Session best score */
    if (M->high_score > 0) {
        char hb[16];
        int_to_str(M->high_score, hb, sizeof(hb));
        int bx = scr_w/2 - 54;
        draw_text("BEST", bx, scr_h/4 + 92, 2, COL_SCORE);
        draw_text(hb, bx + 5*6*2, scr_h/4 + 92, 2, COL_SCORE);
    }

    const char *labels[MENU_ITEMS] = {"PLAY", "2-PLAYER", "INSTRUCTIONS", "QUIT"};
    for (int i = 0; i < MENU_ITEMS; i++) {
        int x, y, w, h;
        menu_btn_rect(scr_w, scr_h, i, &x, &y, &w, &h);
        draw_button(x, y, w, h, labels[i], 2, M->menu_sel == i);
    }

    /* Live RTC clock + date (top-right corner) */
    if (M->clock_str[0]) {
        draw_text(M->clock_str, scr_w - 8*6 - 12, 10, 1, COL_TEXT);
        draw_text(M->date_str,  scr_w - 8*6 - 12, 24, 1, COL_STAR);
    }

    /* Persistent leaderboard (left column, loaded from disk) */
    if (M->high_count > 0) {
        int lx = 24, ly = scr_h/2 - 6;
        draw_text("BEST RUNS", lx, ly, 1, COL_TITLE);
        for (int i = 0; i < M->high_count; i++) {
            uint32_t rc = (i == M->last_rank) ? COL_SCORE : COL_TEXT;
            char num[8], sc[12];
            int_to_str(i + 1, num, sizeof(num));
            int_to_str(M->high_scores[i].score, sc, sizeof(sc));
            int ry = ly + 16 + i * 12;
            draw_text(num, lx, ry, 1, rc);
            draw_text(sc,  lx + 18, ry, 1, rc);
            draw_text(M->high_scores[i].when, lx + 18 + 7*6, ry, 1, COL_STAR);
        }
    }

    draw_cursor();
}

static void render_instructions(void) {
    clear_buffer(COL_MENU_BG);

    draw_text_sh("INSTRUCTIONS", scr_w/2 - 12*6*3/2, 40, 3, COL_TITLE);

    const char *lines[] = {
        "GOAL: FLY AS FAR AS POSSIBLE",
        "AVOID ALL OBSTACLES AND COLLECT COINS",
        "W OR SPACE OR LEFT CLICK - FLY UP",
        "RELEASE - FALL DOWN",
        "P - PAUSE      ESC - QUIT",
        "SPEED INCREASES OVER TIME",
        "GRAB S-SHIELD AND M-MAGNET POWER-UPS",
        "2-PLAYER RACES OVER THE SERIAL PORT",
    };
    int nlines = 8;
    for (int i = 0; i < nlines; i++)
        draw_text(lines[i], scr_w/2 - 30*6/2, 140 + i * 40, 1, COL_TEXT);

    int x, y, w, h;
    back_btn_rect(scr_w, scr_h, &x, &y, &w, &h);
    draw_button(x, y, w, h, "BACK", 2, pt_in_rect(M->mouse_x, M->mouse_y, x, y, w, h));

    draw_cursor();
}

static void render_playing(void) {
    draw_parallax();                 /* sky gradient + scrolling skylines */

    /* Floor + floor line */
    draw_rect(0, scr_h - FLOOR_H, scr_w, FLOOR_H, COL_FLOOR);
    draw_rect(0, scr_h - FLOOR_H, scr_w, 2, COL_ZAP_TIP);

    /* Zappers */
    for (int i = 0; i < MAX_ZAPPERS; i++)
        if (M->zaps[i].active) draw_beam(&M->zaps[i]);

    /* Coins (spin animation) */
    int cf = coin_frame();
    for (int i = 0; i < MAX_COINS; i++) {
        if (!M->coins[i].active) continue;
        if (sprites_ok) {
            draw_sprite(coin_pix[cf], coin_img[cf],
                        M->coins[i].x, M->coins[i].y, COIN_SCALE);
        } else {
            int cx = M->coins[i].x + COIN_SIZE/2;
            int cy = M->coins[i].y + COIN_SIZE/2;
            int r = COIN_SIZE/2;
            for (int dy = -r; dy <= r; dy++) {
                int half_w = r - abs(dy);
                draw_rect(cx - half_w, cy + dy, half_w * 2, 1, COL_COIN);
                if (dy > -r && dy < r)
                    draw_rect(cx - half_w + 1, cy + dy, half_w * 2 - 2, 1, COL_COIN_SHINE);
            }
        }
    }

    /* Power-ups */
    for (int i = 0; i < MAX_POWERUPS; i++)
        if (M->powerups[i].active) draw_powerup(&M->powerups[i]);

    /* Missiles + warnings */
    for (int i = 0; i < MAX_MISSILES; i++) {
        const Missile *mi = &M->missiles[i];
        if (mi->warning > 0) {
            int blink = (mi->warning / 5) % 2;
            if (blink) {
                int ay = mi->y + MISSILE_H / 2;
                for (int t = 0; t < 10; t++)               /* ◄ arrow */
                    draw_rect(scr_w - 16 + t, ay - t, 2, 2 * t + 1, COL_WARN);
                draw_text("!", scr_w - 34, mi->y, 2, COL_WARN);
            }
        }
        if (mi->active) {
            if (sprites_ok)
                draw_sprite(missile_pix, missile_img, mi->x, mi->y, MISSILE_SCALE);
            else {
                draw_rect(mi->x, mi->y, MISSILE_W, MISSILE_H, COL_GAMEOVER);
                draw_rect(mi->x + MISSILE_W - 6, mi->y - 2, 6, MISSILE_H + 4, COL_SCORE);
            }
        }
    }

    /* Opponent ghost (2-player) — drawn behind the local player */
    if (M->versus && M->opp_alive)
        draw_ghost(M->opp_y);

    /* Shield aura behind the player while invulnerable */
    if (M->shield_timer > 0)
        draw_shield_aura(PLAYER_X, M->player.y);

    /* Jetpack flame behind the player */
    if (M->player.thrusting)
        draw_flame(PLAYER_X + 1, M->player.y + 33);

    /* Player — tilts with vertical velocity */
    if (sprites_ok)
        draw_sprite_tilt(player_pix, player_img, PLAYER_X, M->player.y,
                         M->player.vy * 6);
    else
        draw_rect(PLAYER_X, M->player.y, PLAYER_W, PLAYER_H, COL_PLAYER);

    draw_particles();

    /* HUD: score */
    char sbuf[16];
    int_to_str(M->score, sbuf, sizeof(sbuf));
    draw_text("SCORE:", 10, 10, 2, COL_TEXT);
    draw_text(sbuf, 10 + 6*6*2, 10, 2, COL_SCORE);

    /* HUD: coins */
    char cbuf[16];
    int_to_str(M->coins_collected, cbuf, sizeof(cbuf));
    draw_text("x", 34, 40, 2, COL_COIN);
    draw_text(cbuf, 50, 40, 2, COL_COIN);
    if (sprites_ok) {
        int hf = coin_frame();
        draw_sprite(coin_pix[hf], coin_img[hf], 10, 36, COIN_SCALE);
    } else {
        int ci = COIN_SIZE;
        for (int dy = -ci/2; dy <= ci/2; dy++) {
            int hw = ci/2 - abs(dy);
            draw_rect(10 + ci/2 - hw, 36 + ci/2 + dy, hw*2, 1, COL_COIN);
        }
    }

    /* HUD: speed */
    char spdbuf[16];
    int_to_str(M->scroll_speed, spdbuf, sizeof(spdbuf));
    draw_text("SPD:", scr_w - 120, 10, 1, COL_TEXT);
    draw_text(spdbuf, scr_w - 80, 10, 1, COL_SCORE);

    /* HUD: active power-ups, with whole seconds remaining */
    int hud_y = 26;
    if (M->shield_timer > 0) {
        char t[8]; int_to_str((M->shield_timer + 59) / 60, t, sizeof(t));
        draw_text("SHIELD", scr_w - 120, hud_y, 1, COL_SHIELD);
        draw_text(t, scr_w - 120 + 7 * 6, hud_y, 1, COL_SHIELD);
        hud_y += 14;
    }
    if (M->magnet_timer > 0) {
        char t[8]; int_to_str((M->magnet_timer + 59) / 60, t, sizeof(t));
        draw_text("MAGNET", scr_w - 120, hud_y, 1, COL_MAGNET);
        draw_text(t, scr_w - 120 + 7 * 6, hud_y, 1, COL_MAGNET);
    }

    /* HUD: opponent score (2-player) */
    if (M->versus) {
        char p2[16];
        int_to_str(M->opp_score, p2, sizeof(p2));
        draw_text_c("P2", scr_w/2 - 30, 10, 1, COL_GHOST);
        draw_text(p2, scr_w/2 - 10, 10, 1, COL_GHOST);
        if (!M->opp_alive)
            draw_text_c("P2 OUT", scr_w/2, 26, 1, COL_WARN);
    }
}

static void render_gameover(void) {
    clear_buffer(M->hit_flash > 0 ? 0x661111 : COL_MENU_BG);   /* red flash */

    draw_particles();

    draw_text_sh("GAME OVER", scr_w/2 - 9*6*4/2, scr_h/4, 4, COL_GAMEOVER);

    /* Made the persistent leaderboard this run? */
    if (!M->versus && M->last_rank >= 0) {
        draw_text_c_sh("NEW HIGH SCORE!", scr_w/2, scr_h/4 + 44, 2, COL_SCORE);
        char rk[8];
        int_to_str(M->last_rank + 1, rk, sizeof(rk));
        draw_text("RANK", scr_w/2 - 18, scr_h/4 + 68, 1, COL_SCORE);
        draw_text(rk, scr_w/2 + 14, scr_h/4 + 68, 1, COL_SCORE);
    }

    char sbuf[16];
    int_to_str(M->score, sbuf, sizeof(sbuf));
    draw_text("SCORE:", scr_w/2 - 10*6*2/2, scr_h/2 - 30, 2, COL_TEXT);
    draw_text(sbuf, scr_w/2 + 2*6*2, scr_h/2 - 30, 2, COL_SCORE);

    char cbuf[16];
    int_to_str(M->coins_collected, cbuf, sizeof(cbuf));
    draw_text("COINS:", scr_w/2 - 10*6*1/2, scr_h/2 - 2, 1, COL_TEXT);
    draw_text(cbuf, scr_w/2 + 2*6*1, scr_h/2 - 2, 1, COL_COIN);

    /* Session best score (solo) */
    if (!M->versus && M->high_score > 0) {
        char hb[16];
        int_to_str(M->high_score, hb, sizeof(hb));
        draw_text("BEST:", scr_w/2 - 10*6*1/2, scr_h/2 + 12, 1, COL_SCORE);
        draw_text(hb, scr_w/2 + 2*6*1, scr_h/2 + 12, 1, COL_SCORE);
    }

    /* 2-player result (updates live as the opponent's DEAD packet arrives) */
    if (M->versus) {
        char p2[16];
        int_to_str(M->opp_score, p2, sizeof(p2));
        draw_text("P2 SCORE:", scr_w/2 - 11*6/2, scr_h/2 + 14, 1, COL_GHOST);
        draw_text(p2, scr_w/2 + 7*6 - 11*6/2, scr_h/2 + 14, 1, COL_GHOST);

        const char *res; uint32_t rc;
        if (M->opp_alive)                  { res = "P2 STILL FLYING"; rc = COL_TEXT; }
        else if (M->score > M->opp_score)  { res = "YOU WIN";  rc = COL_SCORE; }
        else if (M->score < M->opp_score)  { res = "YOU LOSE"; rc = COL_GAMEOVER; }
        else                               { res = "DRAW";     rc = COL_TEXT; }
        draw_text_c(res, scr_w/2, scr_h/2 + 30, 2, rc);
    } else {
        draw_text("PRESS W OR CLICK TO RESTART", scr_w/2 - 27*6*1/2, scr_h/2 + 30, 1, COL_SCORE);
    }

    /* Timestamp of this run from the RTC */
    if (M->clock_str[0]) {
        draw_text(M->date_str,  scr_w/2 - 8*6/2, scr_h/2 + 52, 1, COL_TEXT);
        draw_text(M->clock_str, scr_w/2 - 8*6/2, scr_h/2 + 66, 1, COL_STAR);
    }

    int x, y, w, h;
    back_btn_rect(scr_w, scr_h, &x, &y, &w, &h);
    draw_button(x, y, w, h, "BACK", 2, pt_in_rect(M->mouse_x, M->mouse_y, x, y, w, h));

    draw_cursor();
}

/* Frozen play-field with a dimmed "PAUSED" overlay. */
static void render_pause(void) {
    render_playing();
    for (int y = 0; y < scr_h; y += 2)          /* venetian dim over the scene */
        draw_rect(0, y, scr_w, 1, COL_MENU_BG);
    draw_text_c_sh("PAUSED", scr_w/2, scr_h/2 - 24, 4, COL_TITLE);
    draw_text_c("PRESS P TO RESUME", scr_w/2, scr_h/2 + 24, 1, COL_TEXT);
}

void view_render(const GameModel *m) {
    M = m;
    scr_w = m->scr_w;
    scr_h = m->scr_h;

    switch (m->state) {
        case STATE_MENU:         render_menu();         break;
        case STATE_INSTRUCTIONS: render_instructions(); break;
        case STATE_PLAYING:      render_playing();      break;
        case STATE_PAUSE:        render_pause();        break;
        case STATE_GAMEOVER:     render_gameover();     break;
        default:                                        break;
    }

    swap_buffers();   /* present to VRAM */
}
