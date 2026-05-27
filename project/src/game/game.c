/* ═══════════════════════════════════════════════════════════════════════════
 *  game.c — Jetpack Joyride-style game for LCOM
 *  All game logic in one file: player, obstacles, drawing, collision.
 *  Uses ONLY vg_draw_rectangle — no XPM dependencies.
 * ═══════════════════════════════════════════════════════════════════════════ */
#include <lcom/lcf.h>
#include <stdlib.h>
#include "game/game.h"
#include "devices/video_gr.h"

/* ── Scancodes ────────────────────────────────────────────────────────────── */
#define W_MAKE    0x11
#define W_BREAK   0x91
#define SPACE_MAKE  0x39
#define SPACE_BREAK 0xB9
#define ESC_MAKE  0x01
#define ENTER_MAKE 0x1C

/* ── Physics constants ────────────────────────────────────────────────────── */
#define GRAVITY       1
#define THRUST       -3
#define MAX_VY        8
#define MIN_VY       -6

/* ── Layout constants ─────────────────────────────────────────────────────── */
#define PLAYER_W     16
#define PLAYER_H     24
#define PLAYER_X     80
#define FLOOR_H      40

/* ── Zapper types ─────────────────────────────────────────────────────────── */
#define ZT_HORIZ   0    /* ceiling + floor pair, horizontal gap            */
#define ZT_DIAG    1    /* slanted pair                                    */
#define ZT_ROTATE  2    /* rotating 360°                                   */
#define ZT_SINGLE  3    /* single block (from ceiling or floor)            */

#define MAX_ZAPPERS  6
#define ZAPPER_W     14
#define ZAP_CEIL_MIN_H  60
#define ZAP_CEIL_MAX_H  130
#define ZAP_FLOOR_MIN_H 60
#define ZAP_FLOOR_MAX_H 130
#define ZAP_MIN_GAP   80
#define ZAP_MAX_GAP  150
#define ZAPPER_GAP   300
#define ZAP_MOVE_AMP  20
#define ZAP_MOVE_SPEED 1
#define ZAP_ROTATE_R  30

/* ── Coins ───────────────────────────────────────────────────────────────── */
#define MAX_COINS   30
#define COIN_SIZE     8

/* ── Missiles ─────────────────────────────────────────────────────────────── */
#define MAX_MISSILES  3
#define MISSILE_H    16
#define MISSILE_W    40
#define MISSILE_WARN  60      /* frames of warning before missile spawns    */
#define MISSILE_SPEED 6

/* ── Colors (24-bit, mode 0x115 = 800x600x24) ────────────────────────────── */
#define COL_BG        0x1A1A2E
#define COL_FLOOR     0x16213E
#define COL_PLAYER    0xE94560
#define COL_ZAP_BODY  0xFFCC00  /* yellow zapper body                       */
#define COL_ZAP_TIP   0x888888  /* gray zapper tip                          */
#define COL_ZAP_GLOW  0xFFFF88  /* bright yellow glow                       */
#define COL_MENU_BG   0x0F0F23
#define COL_TITLE     0xE94560
#define COL_TEXT      0xEEEEEE
#define COL_GAMEOVER  0xFF2222
#define COL_SCORE     0xFFCC00
#define COL_STAR      0xFFFFAA
#define COL_COIN      0xFFD700
#define COL_COIN_SHINE 0xFFFF88
#define COL_WARN      0xFF0000  /* warning red for missiles                  */

/* ── Data structures ──────────────────────────────────────────────────────── */
typedef struct {
    int y, vy;
    bool thrusting;
} Player;

typedef struct {
    int x;
    int ceil_bot, ceil_h;
    int floor_top, floor_h;
    int type;                  /* ZT_HORIZ, ZT_DIAG, ZT_ROTATE, ZT_SINGLE   */
    bool active;
    int move_type;             /* 0=static, 1=vertical, 2=horizontal        */
    int move_off, move_dir;
    int rot_frame;
} Zapper;

typedef struct {
    int x, y;
    bool active;
} Coin;

typedef struct {
    int y;
    int warning;               /* frames remaining before missile spawns    */
    int active;
    int x;
    int speed;
} Missile;

/* ── Module state ─────────────────────────────────────────────────────────── */
static game_state_t state = STATE_MENU;
static Player player;
static Zapper zaps[MAX_ZAPPERS];
static Coin coins[MAX_COINS];
static Missile missiles[MAX_MISSILES];

static int  scroll_speed = 3;
static int  score        = 0;
static int  coins_collected = 0;
static int  frame_count  = 0;
static int  spawn_timer  = 0;
static int  missile_timer = 0;
static int  difficulty_timer = 0;

static int  scr_w, scr_h;

/* ── Menu state ─────────────────────────────────────────────────────────── */
#define MENU_ITEMS 3
static int  menu_sel = 0;       /* 0=Play, 1=Instructions, 2=Quit */
static int  mouse_x  = 0;       /* cursor X position */
static int  mouse_y  = 0;       /* cursor Y position */
static bool lb_prev  = false;   /* previous frame left-button state (edge detect) */

/* ── Menu state ─────────────────────────────────────────────────────────── */
#define MENU_ITEMS 3
static int  menu_sel = 0;       /* 0=Play, 1=Instructions, 2=Quit */
static int  mouse_x  = 0;       /* cursor X position */
static int  mouse_y  = 0;       /* cursor Y position */
static bool lb_prev  = false;   /* previous frame left-button state (edge detect) */

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void reset_game(void);
static void spawn_zapper(void);
static void spawn_coins(int cx, int cy, int pattern);

/* ═══════════════════ PUBLIC API ═══════════════════════════════════════════ */

game_state_t game_get_state(void) { return state; }

void game_init(void) {
    scr_w  = h_res;
    scr_h  = v_res;
    state  = STATE_MENU;
    menu_sel = 0;
    mouse_x  = scr_w / 2;
    mouse_y  = scr_h / 2;
    lb_prev  = false;
    srand(42);
}

void game_cleanup(void) {
    /* nothing to free — all static */
}

/* ── Reset for a new round ────────────────────────────────────────────────── */
static void reset_game(void) {
    player.y  = scr_h / 2;
    player.vy = 0;
    player.thrusting = false;

    for (int i = 0; i < MAX_ZAPPERS; i++) zaps[i].active = false;
    for (int i = 0; i < MAX_COINS; i++) coins[i].active = false;
    for (int i = 0; i < MAX_MISSILES; i++) missiles[i].active = 0;

    scroll_speed = 3;
    score = 0;
    coins_collected = 0;
    frame_count = 0;
    spawn_timer = 0;
    missile_timer = 0;
    difficulty_timer = 0;
}

/* ── Spawn a zapper on the right edge ──────────────────────────────────── */
static void spawn_zapper(void) {
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!zaps[i].active) {
            zaps[i].active = true;
            zaps[i].x = scr_w;
            zaps[i].type = rand() % 4;
            zaps[i].move_type = rand() % 3;
            zaps[i].move_off = 0;
            zaps[i].move_dir = 1;
            zaps[i].rot_frame = 0;

            if (zaps[i].type == ZT_SINGLE) {
                int from_ceiling = rand() % 2;
                int h = 50 + (rand() % 50);
                if (from_ceiling) {
                    zaps[i].ceil_bot = 60 + (rand() % (scr_h - FLOOR_H - 120));
                    zaps[i].ceil_h = h;
                    zaps[i].floor_h = 0; zaps[i].floor_top = 0;
                } else {
                    zaps[i].floor_top = 60 + (rand() % (scr_h - FLOOR_H - 120));
                    zaps[i].floor_h = h;
                    zaps[i].ceil_h = 0; zaps[i].ceil_bot = 0;
                }
            } else {
                int ch = ZAP_CEIL_MIN_H + (rand() % (ZAP_CEIL_MAX_H - ZAP_CEIL_MIN_H + 1));
                int fh = ZAP_FLOOR_MIN_H + (rand() % (ZAP_FLOOR_MAX_H - ZAP_FLOOR_MIN_H + 1));
                zaps[i].ceil_h = ch;
                zaps[i].floor_h = fh;
                int gs = ZAP_MIN_GAP + (rand() % (ZAP_MAX_GAP - ZAP_MIN_GAP + 1));
                int max_ceil_bot = scr_h - FLOOR_H - fh - gs - 10;
                int min_ceil_bot = ch + 10;
                if (max_ceil_bot < min_ceil_bot) max_ceil_bot = min_ceil_bot + 10;
                zaps[i].ceil_bot = min_ceil_bot + (rand() % (max_ceil_bot - min_ceil_bot + 1));
                zaps[i].floor_top = zaps[i].ceil_bot + gs;
            }

            int gap_cy;
            if (zaps[i].type == ZT_SINGLE) {
                gap_cy = (zaps[i].ceil_h > 0) ? zaps[i].ceil_bot + 30 : zaps[i].floor_top - 30;
            } else {
                gap_cy = (zaps[i].ceil_bot + zaps[i].floor_top) / 2;
            }
            spawn_coins(zaps[i].x + 40, gap_cy, rand() % 4);
            return;
        }
    }
}

/* ── Spawn coins in patterns near a position ───────────────────────────── */
static void spawn_coins(int cx, int cy, int pattern) {
    int count = 4 + (rand() % 4);
    int placed = 0;
    for (int i = 0; i < MAX_COINS && placed < count; i++) {
        if (!coins[i].active) {
            coins[i].active = true;
            int off = placed * 12;
            switch (pattern) {
                case 0:
                    coins[i].x = cx + off;
                    coins[i].y = cy - 10 + abs(off - 24);
                    break;
                case 1:
                    coins[i].x = cx + off;
                    coins[i].y = cy + 10 - abs(off - 24);
                    break;
                case 2:
                    coins[i].x = cx + off;
                    coins[i].y = cy;
                    break;
                case 3:
                    coins[i].x = cx + (rand() % 30);
                    coins[i].y = cy - 10 + (rand() % 20);
                    break;
            }
            placed++;
        }
    }
}

/* ═══════════════════ UPDATE (called every timer tick) ═════════════════════ */

void game_update(void) {
    if (state != STATE_PLAYING) return;

    frame_count++;

    /* ── Player physics ──────────────────────────────────────────────────── */
    if (player.thrusting) player.vy += THRUST;
    else player.vy += GRAVITY;

    if (player.vy > MAX_VY)  player.vy = MAX_VY;
    if (player.vy < MIN_VY)  player.vy = MIN_VY;
    player.y += player.vy;

    if (player.y < 0) { player.y = 0; player.vy = 0; }
    int floor_top = scr_h - FLOOR_H - PLAYER_H;
    if (player.y > floor_top) { player.y = floor_top; player.vy = 0; }

    /* ── Move zappers ───────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!zaps[i].active) continue;
        zaps[i].x -= scroll_speed;

        if (zaps[i].move_type > 0) {
            zaps[i].move_off += zaps[i].move_dir * ZAP_MOVE_SPEED;
            if (zaps[i].move_off > ZAP_MOVE_AMP) zaps[i].move_dir = -1;
            if (zaps[i].move_off < -ZAP_MOVE_AMP) zaps[i].move_dir = 1;
        }

        if (zaps[i].type == ZT_ROTATE) zaps[i].rot_frame++;

        int extra = (zaps[i].move_type == 2) ? ZAP_MOVE_AMP : 0;
        if (zaps[i].x + ZAPPER_W + extra + 10 < 0) zaps[i].active = false;
    }

    /* ── Move coins ──────────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_COINS; i++) {
        if (coins[i].active) {
            coins[i].x -= scroll_speed;
            if (coins[i].x + COIN_SIZE < 0) coins[i].active = false;
        }
    }

    /* ── Missiles ────────────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_MISSILES; i++) {
        Missile *m = &missiles[i];
        if (m->warning > 0) {
            m->warning--;
            if (m->warning == 0) {
                m->active = 1;
                m->x = scr_w;
                m->y = 30 + (rand() % (scr_h - FLOOR_H - 60));
                m->speed = MISSILE_SPEED + (scroll_speed / 2);
            }
        }
        if (m->active) {
            m->x -= m->speed;
            if (m->x + MISSILE_W < 0) m->active = 0;
        }
    }

    missile_timer++;
    if (missile_timer >= MISSILE_WARN * 3) {
        missile_timer = 0;
        for (int i = 0; i < MAX_MISSILES; i++) {
            if (missiles[i].warning == 0 && !missiles[i].active) {
                missiles[i].warning = MISSILE_WARN;
                missiles[i].y = 30 + (rand() % (scr_h - FLOOR_H - 60));
                break;
            }
        }
    }

    /* ── Spawn logic ─────────────────────────────────────────────────────── */
    spawn_timer++;
    int spawn_interval = ZAPPER_GAP / scroll_speed;
    if (spawn_interval < 20) spawn_interval = 20;
    if (spawn_timer >= spawn_interval) {
        spawn_zapper();
        spawn_timer = 0;
    }

    /* ── Coin collection ─────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_COINS; i++) {
        if (!coins[i].active) continue;
        if (PLAYER_X + PLAYER_W > coins[i].x &&
            PLAYER_X < coins[i].x + COIN_SIZE &&
            player.y + PLAYER_H > coins[i].y &&
            player.y < coins[i].y + COIN_SIZE) {
            coins[i].active = false;
            coins_collected++;
            score += 10;
        }
    }

    /* ── Score ───────────────────────────────────────────────────────────── */
    score = (frame_count / 6) + coins_collected * 10;

    /* ── Difficulty ramp ─────────────────────────────────────────────────── */
    difficulty_timer++;
    if (difficulty_timer >= 300 && scroll_speed < 10) {
        scroll_speed++;
        difficulty_timer = 0;
    }

    /* ── Collision: zappers ──────────────────────────────────────────────── */
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!zaps[i].active) continue;
        int mo = (zaps[i].move_type == 1) ? zaps[i].move_off : 0;
        int hx = (zaps[i].move_type == 2) ? zaps[i].move_off : 0;
        int rf = zaps[i].rot_frame;
        int zw = ZAPPER_W;
        int cx = zaps[i].x + hx;

        if (zaps[i].type == ZT_ROTATE) {
            int shift = (rf % 60 < 30) ? (rf % 30) : (60 - (rf % 30));
            shift = (shift * 12) / 30;
            if (rf % 120 < 60) shift = -shift;
            int c_dx = shift;
            int f_dx = -shift;

            if (PLAYER_X + PLAYER_W > cx + c_dx &&
                PLAYER_X < cx + c_dx + zw) {
                int cb = zaps[i].ceil_bot + mo;
                int ct = cb - zaps[i].ceil_h;
                if (player.y + PLAYER_H > ct && player.y < cb)
                    { state = STATE_GAMEOVER; return; }
            }
            if (PLAYER_X + PLAYER_W > cx + f_dx &&
                PLAYER_X < cx + f_dx + zw) {
                int ft = zaps[i].floor_top + mo;
                int fb = ft + zaps[i].floor_h;
                if (player.y + PLAYER_H > ft && player.y < fb)
                    { state = STATE_GAMEOVER; return; }
            }
        } else if (zaps[i].type == ZT_DIAG) {
            int c_dx = -6, f_dx = 6;
            if (PLAYER_X + PLAYER_W > cx + c_dx && PLAYER_X < cx + c_dx + zw) {
                int cb = zaps[i].ceil_bot + mo;
                int ct = cb - zaps[i].ceil_h;
                if (player.y + PLAYER_H > ct && player.y < cb)
                    { state = STATE_GAMEOVER; return; }
            }
            if (PLAYER_X + PLAYER_W > cx + f_dx && PLAYER_X < cx + f_dx + zw) {
                int ft = zaps[i].floor_top + mo;
                int fb = ft + zaps[i].floor_h;
                if (player.y + PLAYER_H > ft && player.y < fb)
                    { state = STATE_GAMEOVER; return; }
            }
        } else if (zaps[i].type == ZT_SINGLE) {
            if (zaps[i].ceil_h > 0) {
                if (PLAYER_X + PLAYER_W > cx && PLAYER_X < cx + zw + 8) {
                    int cb = zaps[i].ceil_bot + mo;
                    int ct = cb - zaps[i].ceil_h;
                    if (player.y + PLAYER_H > ct && player.y < cb)
                        { state = STATE_GAMEOVER; return; }
                }
            } else {
                if (PLAYER_X + PLAYER_W > cx && PLAYER_X < cx + zw + 8) {
                    int ft = zaps[i].floor_top + mo;
                    int fb = ft + zaps[i].floor_h;
                    if (player.y + PLAYER_H > ft && player.y < fb)
                        { state = STATE_GAMEOVER; return; }
                }
            }
        } else {
            /* ZT_HORIZ */
            if (PLAYER_X + PLAYER_W > cx && PLAYER_X < cx + zw) {
                int cb = zaps[i].ceil_bot + mo;
                int ft = zaps[i].floor_top + mo;
                int ct = cb - zaps[i].ceil_h;
                int fb = ft + zaps[i].floor_h;
                if ((player.y + PLAYER_H > ct && player.y < cb) ||
                    (player.y + PLAYER_H > ft && player.y < fb))
                    { state = STATE_GAMEOVER; return; }
            }
        }
    }

    /* ── Collision: missiles ─────────────────────────────────────────────── */
    for (int i = 0; i < MAX_MISSILES; i++) {
        if (!missiles[i].active) continue;
        Missile *m = &missiles[i];
        if (PLAYER_X + PLAYER_W > m->x &&
            PLAYER_X < m->x + MISSILE_W &&
            player.y + PLAYER_H > m->y &&
            player.y < m->y + MISSILE_H)
            { state = STATE_GAMEOVER; return; }
    }
}

/* ═══════════════════ DRAW (called every timer tick) ══════════════════════ */

/* Helper: draw a filled rectangle clamped to screen */
static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0)      { w += x; x = 0; }
    if (y < 0)      { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    vg_draw_rectangle((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, color);
}

/* Draw simple block letters — each character is 5x7 pixels, scaled by s */
static void draw_char(char c, int x, int y, int s, uint32_t color);
static void draw_text(const char *str, int x, int y, int s, uint32_t color) {
    while (*str) {
        draw_char(*str, x, y, s, color);
        x += 6 * s; /* 5 wide + 1 gap */
        str++;
    }
}

/* Tiny 5x7 font — each row is a 5-bit pattern */
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
};

static void draw_char(char c, int x, int y, int s, uint32_t color) {
    if (c < 0 || (unsigned char)c >= sizeof(font_5x7)/sizeof(font_5x7[0]))
        return;
    const uint8_t *glyph = font_5x7[(unsigned char)c];
    if (glyph[0] == 0 && glyph[1] == 0 && glyph[6] == 0 && c != ' ')
        return; /* no glyph for this char */
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (glyph[row] & (0x10 >> col)) {
                draw_rect(x + col*s, y + row*s, s, s, color);
            }
        }
    }
}

/* Format an integer into a string buffer */
static void int_to_str(int val, char *buf, int bufsz) {
    if (bufsz < 2) return;
    if (val < 0) { buf[0] = '-'; buf++; bufsz--; val = -val; }
    int len = 0;
    char tmp[16];
    do { tmp[len++] = '0' + (val % 10); val /= 10; } while (val > 0 && len < 15);
    for (int i = len - 1; i >= 0 && bufsz > 1; i--) { *buf++ = tmp[i]; bufsz--; }
    *buf = '\0';
}

void game_draw(void) {
    if (state == STATE_MENU) {
        /* ── MENU SCREEN ──────────────────────────────────────────────── */
        clear_buffer(COL_MENU_BG);

        /* Decorative stars */
        for (int i = 0; i < 50; i++) {
            int sx = (i * 97 + frame_count * (i % 3 + 1)) % scr_w;
            int sy = (i * 73) % (scr_h - 100);
            draw_rect(sx, sy, 2, 2, COL_STAR);
        }

        /* Animated player preview */
        int preview_y = scr_h/4 - 30 + ((frame_count / 8) % 2 == 0 ? -5 : 5);
        draw_rect(scr_w/2 - PLAYER_W/2, preview_y, PLAYER_W, PLAYER_H, COL_PLAYER);

        /* Title */
        draw_text("JETPACK JOYRIDE", scr_w/2 - 15*6*3/2, scr_h/4 + 10, 3, COL_TITLE);
        /* Subtitle */
        draw_text("LCOM EDITION", scr_w/2 - 12*6*2/2, scr_h/4 + 55, 2, COL_TEXT);

        /* ── Menu buttons ──────────────────────────────────────────────── */
        const char *labels[MENU_ITEMS] = {"PLAY", "INSTRUCTIONS", "QUIT"};
        int btn_w = 240;
        int btn_h = 30;
        int btn_x = scr_w/2 - btn_w/2;
        int btn_start_y = scr_h/2 + 10;
        int btn_gap = 50;

        for (int i = 0; i < MENU_ITEMS; i++) {
            int by = btn_start_y + i * btn_gap;

            /* Hover detection: check if mouse cursor is inside this button */
            bool hovered = (mouse_x >= btn_x && mouse_x <= btn_x + btn_w &&
                            mouse_y >= by    && mouse_y <= by + btn_h);

            /* Keyboard selection overrides hover highlight */
            bool selected = (menu_sel == i) || hovered;
            if (hovered) menu_sel = i; /* sync keyboard selection with mouse */

            /* Button background */
            uint32_t bg_col  = selected ? COL_TITLE   : 0x1A1A3A;
            uint32_t txt_col = selected ? 0xFFFFFF    : COL_TEXT;
            draw_rect(btn_x, by, btn_w, btn_h, bg_col);
            /* Button border */
            draw_rect(btn_x,          by,          btn_w, 2, COL_OBS_TOP);
            draw_rect(btn_x,          by+btn_h-2,  btn_w, 2, COL_OBS_TOP);
            draw_rect(btn_x,          by,          2, btn_h, COL_OBS_TOP);
            draw_rect(btn_x+btn_w-2,  by,          2, btn_h, COL_OBS_TOP);

            /* Centered label */
            int llen = 0;
            for (const char *p = labels[i]; *p; p++) llen++;
            int lx = btn_x + (btn_w - llen * 6 * 2) / 2;
            int ly = by + (btn_h - 7*2) / 2;
            draw_text(labels[i], lx, ly, 2, txt_col);
        }

        /* Mouse cursor — small crosshair */
        draw_rect(mouse_x - 5, mouse_y,     11, 2, COL_SCORE);
        draw_rect(mouse_x,     mouse_y - 5, 2, 11, COL_SCORE);

        frame_count++;
    }
    else if (state == STATE_INSTRUCTIONS) {
        /* ── INSTRUCTIONS SCREEN ──────────────────────────────────────── */
        clear_buffer(COL_MENU_BG);

        draw_text("INSTRUCTIONS", scr_w/2 - 12*6*3/2, 40, 3, COL_TITLE);

        /* Instructions text */
        const char *lines[] = {
            "GOAL: FLY AS FAR AS POSSIBLE",
            "AVOID ALL OBSTACLES",
            "W OR SPACE - FLY UP",
            "RELEASE  - FALL DOWN",
            "ESC - QUIT THE GAME",
            "SPEED INCREASES OVER TIME",
        };
        int nlines = 6;
        for (int i = 0; i < nlines; i++) {
            draw_text(lines[i], scr_w/2 - 30*6/2, 140 + i * 40, 1, COL_TEXT);
        }

        /* BACK button */
        int bbw = 140, bbh = 28;
        int bbx = scr_w/2 - bbw/2;
        int bby = scr_h - 80;
        bool bb_hov = (mouse_x >= bbx && mouse_x <= bbx + bbw &&
                       mouse_y >= bby && mouse_y <= bby + bbh);
        uint32_t bb_bg  = bb_hov ? COL_TITLE : 0x1A1A3A;
        uint32_t bb_txt = bb_hov ? 0xFFFFFF  : COL_TEXT;
        draw_rect(bbx, bby, bbw, bbh, bb_bg);
        draw_rect(bbx,       bby,         bbw, 2, COL_OBS_TOP);
        draw_rect(bbx,       bby+bbh-2,   bbw, 2, COL_OBS_TOP);
        draw_rect(bbx,       bby,         2, bbh, COL_OBS_TOP);
        draw_rect(bbx+bbw-2, bby,         2, bbh, COL_OBS_TOP);
        draw_text("BACK", bbx + (bbw - 4*6*2)/2, bby + (bbh - 7*2)/2, 2, bb_txt);

        /* Mouse cursor */
        draw_rect(mouse_x - 5, mouse_y,     11, 2, COL_SCORE);
        draw_rect(mouse_x,     mouse_y - 5, 2, 11, COL_SCORE);
    }
    else if (state == STATE_PLAYING) {
        /* ── GAME SCREEN ──────────────────────────────────────────────── */
        clear_buffer(COL_BG);

        /* Floor */
        draw_rect(0, scr_h - FLOOR_H, scr_w, FLOOR_H, COL_FLOOR);

        /* Floor line */
        draw_rect(0, scr_h - FLOOR_H, scr_w, 2, COL_ZAP_TIP);

        /* Background particles (parallax-like scrolling dots) */
        for (int i = 0; i < 30; i++) {
            int px = (i * 137 - frame_count * 1) % scr_w;
            if (px < 0) px += scr_w;
            int py = (i * 53) % (scr_h - FLOOR_H);
            draw_rect(px, py, 1, 1, 0x333355);
        }

        /* Zappers */
        for (int i = 0; i < MAX_ZAPPERS; i++) {
            if (!zaps[i].active) continue;

            int mo = (zaps[i].move_type == 1) ? zaps[i].move_off : 0;
            int hx = (zaps[i].move_type == 2) ? zaps[i].move_off : 0;
            int rf = zaps[i].rot_frame;
            int zw = ZAPPER_W;
            int cx = zaps[i].x + hx;
            int c_dx = 0, f_dx = 0;

            if (zaps[i].type == ZT_ROTATE) {
                int shift = (rf % 60 < 30) ? (rf % 30) : (60 - (rf % 30));
                shift = (shift * 12) / 30;
                if (rf % 120 < 60) shift = -shift;
                c_dx = shift; f_dx = -shift;
            } else if (zaps[i].type == ZT_DIAG) {
                c_dx = -6; f_dx = 6;
            }

            if (zaps[i].type != ZT_SINGLE || zaps[i].ceil_h > 0) {
                int cb = zaps[i].ceil_bot + mo;
                int ct = cb - zaps[i].ceil_h;
                draw_rect(cx + c_dx, ct, zw, zaps[i].ceil_h, COL_ZAP_BODY);
                draw_rect(cx + c_dx - 1, cb - 6, zw + 2, 6, COL_ZAP_TIP);
                draw_rect(cx + c_dx + 2, cb - 14, 2, 6, COL_ZAP_GLOW);
                draw_rect(cx + c_dx + zw - 4, cb - 14, 2, 6, COL_ZAP_GLOW);
            }

            if (zaps[i].type != ZT_SINGLE || zaps[i].floor_h > 0) {
                int ft = zaps[i].floor_top + mo;
                draw_rect(cx + f_dx, ft, zw, zaps[i].floor_h, COL_ZAP_BODY);
                draw_rect(cx + f_dx - 1, ft, zw + 2, 6, COL_ZAP_TIP);
                draw_rect(cx + f_dx + 2, ft + 8, 2, 6, COL_ZAP_GLOW);
                draw_rect(cx + f_dx + zw - 4, ft + 8, 2, 6, COL_ZAP_GLOW);
            }
        }

        /* Coins (diamond shape = circle approximation) */
        for (int i = 0; i < MAX_COINS; i++) {
            if (!coins[i].active) continue;
            int cx = coins[i].x + COIN_SIZE/2;
            int cy = coins[i].y + COIN_SIZE/2;
            int r = COIN_SIZE/2;
            for (int dy = -r; dy <= r; dy++) {
                int half_w = r - abs(dy);
                draw_rect(cx - half_w, cy + dy, half_w * 2, 1, COL_COIN);
                if (dy > -r && dy < r)
                    draw_rect(cx - half_w + 1, cy + dy, half_w * 2 - 2, 1, COL_COIN_SHINE);
            }
        }

        /* Missiles + warnings */
        for (int i = 0; i < MAX_MISSILES; i++) {
            Missile *m = &missiles[i];
            if (m->warning > 0) {
                int blink = (m->warning / 5) % 2;
                if (blink) {
                    draw_rect(scr_w - 6, m->y - 2, 6, MISSILE_H + 4, COL_WARN);
                    draw_rect(scr_w - 12, m->y + 2, 6, MISSILE_H - 4, COL_WARN);
                }
            }
            if (m->active) {
                draw_rect(m->x, m->y, MISSILE_W, MISSILE_H, COL_GAMEOVER);
                draw_rect(m->x + 4, m->y + 2, MISSILE_W - 8, MISSILE_H - 4, 0xCC4444);
                draw_rect(m->x + MISSILE_W - 6, m->y - 2, 6, MISSILE_H + 4, COL_SCORE);
            }
        }

        /* Player */
        draw_rect(PLAYER_X, player.y, PLAYER_W, PLAYER_H, COL_PLAYER);
        /* Jetpack flame when thrusting */
        if (player.thrusting) {
            int flame_h = 4 + (frame_count % 4);
            draw_rect(PLAYER_X + 4, player.y + PLAYER_H, PLAYER_W - 8, flame_h, COL_SCORE);
            draw_rect(PLAYER_X + 6, player.y + PLAYER_H + flame_h, PLAYER_W - 12, 3, COL_GAMEOVER);
        }

        /* Score display */
        char sbuf[16];
        int_to_str(score, sbuf, sizeof(sbuf));
        draw_text("SCORE:", 10, 10, 2, COL_TEXT);
        draw_text(sbuf, 10 + 6*6*2, 10, 2, COL_SCORE);

        /* Coins display */
        char cbuf[16];
        int_to_str(coins_collected, cbuf, sizeof(cbuf));
        draw_text("x", 12, 34, 1, COL_COIN);
        draw_text(cbuf, 22, 34, 1, COL_COIN);
        int ci = COIN_SIZE;
        for (int dy = -ci/2; dy <= ci/2; dy++) {
            int hw = ci/2 - abs(dy);
            draw_rect(10 + ci/2 - hw, 34 + ci/2 + dy, hw*2, 1, COL_COIN);
        }

        /* Speed indicator */
        char spdbuf[16];
        int_to_str(scroll_speed, spdbuf, sizeof(spdbuf));
        draw_text("SPD:", scr_w - 120, 10, 1, COL_TEXT);
        draw_text(spdbuf, scr_w - 80, 10, 1, COL_SCORE);
    }
    else if (state == STATE_GAMEOVER) {
        /* ── GAME OVER SCREEN ─────────────────────────────────────────── */
        clear_buffer(COL_MENU_BG);

        draw_text("GAME OVER", scr_w/2 - 9*6*4/2, scr_h/4, 4, COL_GAMEOVER);

        char sbuf[16];
        int_to_str(score, sbuf, sizeof(sbuf));
        draw_text("SCORE:", scr_w/2 - 10*6*2/2, scr_h/2 - 30, 2, COL_TEXT);
        draw_text(sbuf, scr_w/2 + 2*6*2, scr_h/2 - 30, 2, COL_SCORE);

        char cbuf[16];
        int_to_str(coins_collected, cbuf, sizeof(cbuf));
        draw_text("COINS:", scr_w/2 - 10*6*1/2, scr_h/2 - 2, 1, COL_TEXT);
        draw_text(cbuf, scr_w/2 + 2*6*1, scr_h/2 - 2, 1, COL_COIN);

        draw_text("PRESS W OR CLICK TO RESTART", scr_w/2 - 27*6*1/2, scr_h/2 + 30, 1, COL_SCORE);

        /* BACK button */
        int bbw = 140, bbh = 28;
        int bbx = scr_w/2 - bbw/2;
        int bby = scr_h - 80;
        bool bb_hov = (mouse_x >= bbx && mouse_x <= bbx + bbw &&
                       mouse_y >= bby && mouse_y <= bby + bbh);
        uint32_t bb_bg  = bb_hov ? COL_TITLE : 0x1A1A3A;
        uint32_t bb_txt = bb_hov ? 0xFFFFFF  : COL_TEXT;
        draw_rect(bbx, bby, bbw, bbh, bb_bg);
        draw_rect(bbx,       bby,         bbw, 2, COL_OBS_TOP);
        draw_rect(bbx,       bby+bbh-2,   bbw, 2, COL_OBS_TOP);
        draw_rect(bbx,       bby,         2, bbh, COL_OBS_TOP);
        draw_rect(bbx+bbw-2, bby,         2, bbh, COL_OBS_TOP);
        draw_text("BACK", bbx + (bbw - 4*6*2)/2, bby + (bbh - 7*2)/2, 2, bb_txt);

        /* Mouse cursor */
        draw_rect(mouse_x - 5, mouse_y,     11, 2, COL_SCORE);
        draw_rect(mouse_x,     mouse_y - 5, 2, 11, COL_SCORE);
    }

    /* Always swap to VRAM */
    swap_buffers();
}

/* ═══════════════════ KEYBOARD INPUT ══════════════════════════════════════ */

void game_handle_kbd(uint8_t scancode) {
    /* Arrow key scancodes */
    #define UP_MAKE   0x48
    #define DOWN_MAKE 0x50

    switch (state) {
        case STATE_MENU:
            if (scancode == ESC_MAKE) {
                state = STATE_EXIT;
            } else if (scancode == UP_MAKE) {
                menu_sel = (menu_sel - 1 + MENU_ITEMS) % MENU_ITEMS;
            } else if (scancode == DOWN_MAKE) {
                menu_sel = (menu_sel + 1) % MENU_ITEMS;
            } else if (scancode == ENTER_MAKE || scancode == SPACE_MAKE) {
                if (menu_sel == 0) { reset_game(); state = STATE_PLAYING; }
                else if (menu_sel == 1) { state = STATE_INSTRUCTIONS; }
                else if (menu_sel == 2) { state = STATE_EXIT; }
            }
            break;

        case STATE_INSTRUCTIONS:
            /* Only ESC goes back to menu; other keys ignored */
            if (scancode == ESC_MAKE)
                state = STATE_MENU;
            break;

        case STATE_PLAYING:
            if (scancode == ESC_MAKE) {
                state = STATE_MENU; /* pause/back to menu */
            } else if (scancode == W_MAKE || scancode == SPACE_MAKE) {
                player.thrusting = true;
            } else if (scancode == W_BREAK || scancode == SPACE_BREAK) {
                player.thrusting = false;
            }
            break;

        case STATE_GAMEOVER:
            if (scancode == ESC_MAKE) {
                state = STATE_MENU;
            } else if (scancode == W_MAKE || scancode == SPACE_MAKE || scancode == ENTER_MAKE) {
                reset_game();
                state = STATE_PLAYING;
            }
            break;

        default:
            break;
    }
}

/* ═══════════════════ MOUSE INPUT ════════════════════════════════════════ */

void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed) {
    /* Update cursor position, clamped to screen */
    mouse_x += dx;
    mouse_y -= dy; /* Y is inverted: mouse up = negative dy = up on screen */
    if (mouse_x < 0)       mouse_x = 0;
    if (mouse_x >= scr_w)  mouse_x = scr_w - 1;
    if (mouse_y < 0)       mouse_y = 0;
    if (mouse_y >= scr_h)  mouse_y = scr_h - 1;

    /* During gameplay, left button controls thrust (hold = fly, release = fall) */
    if (state == STATE_PLAYING) {
        player.thrusting = lb_pressed;
    }

    /* Edge detection: only act on the press (not the hold) for menu clicks */
    bool clicked = lb_pressed && !lb_prev;
    lb_prev = lb_pressed;

    if (!clicked) return;

    if (state == STATE_MENU) {
        int btn_w = 240, btn_h = 30;
        int btn_x = scr_w/2 - btn_w/2;
        int btn_start_y = scr_h/2 + 10;
        int btn_gap = 50;

        for (int i = 0; i < MENU_ITEMS; i++) {
            int by = btn_start_y + i * btn_gap;
            if (mouse_x >= btn_x && mouse_x <= btn_x + btn_w &&
                mouse_y >= by    && mouse_y <= by + btn_h) {
                if (i == 0) { reset_game(); state = STATE_PLAYING; }
                else if (i == 1) { state = STATE_INSTRUCTIONS; }
                else if (i == 2) { state = STATE_EXIT; }
                return;
            }
        }
    }
    else if (state == STATE_INSTRUCTIONS) {
        /* BACK button */
        int bbw = 140, bbh = 28;
        int bbx = scr_w/2 - bbw/2;
        int bby = scr_h - 80;
        if (mouse_x >= bbx && mouse_x <= bbx + bbw &&
            mouse_y >= bby && mouse_y <= bby + bbh) {
            state = STATE_MENU;
        }
    }
    else if (state == STATE_GAMEOVER) {
        /* BACK button goes to menu */
        int bbw = 140, bbh = 28;
        int bbx = scr_w/2 - bbw/2;
        int bby = scr_h - 80;
        if (mouse_x >= bbx && mouse_x <= bbx + bbw &&
            mouse_y >= bby && mouse_y <= bby + bbh) {
            state = STATE_MENU;
        } else {
            /* Click anywhere else restarts */
            reset_game();
            state = STATE_PLAYING;
        }
    }
}
