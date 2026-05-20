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
#define S_MAKE    0x1F
#define S_BREAK   0x9F
#define SPACE_MAKE  0x39
#define SPACE_BREAK 0xB9
#define ESC_MAKE  0x01
#define ENTER_MAKE 0x1C

/* ── Physics constants ────────────────────────────────────────────────────── */
#define GRAVITY       1        /* pixels/frame downward                      */
#define THRUST       -3        /* pixels/frame upward when W held            */
#define MAX_VY        8        /* max falling speed                          */
#define MIN_VY       -6        /* max upward speed                           */

/* ── Layout constants ─────────────────────────────────────────────────────── */
#define PLAYER_W     20
#define PLAYER_H     20
#define PLAYER_X     80        /* fixed horizontal position                  */
#define FLOOR_H      40        /* ground strip height                        */

#define MAX_OBSTACLES 6
#define OBS_W         20
#define OBS_MIN_H     60
#define OBS_MAX_H    200
#define OBS_GAP      250       /* minimum horizontal gap between obstacles   */

/* ── Colors (24-bit, mode 0x115 = 800x600x24) ────────────────────────────── */
#define COL_BG        0x1A1A2E  /* dark navy background                     */
#define COL_FLOOR     0x16213E  /* darker floor                             */
#define COL_PLAYER    0xE94560  /* bright red-pink player                   */
#define COL_OBSTACLE  0x0F3460  /* dark blue obstacles                      */
#define COL_OBS_TOP   0x533483  /* purple top                               */
#define COL_MENU_BG   0x0F0F23  /* very dark menu background                */
#define COL_TITLE     0xE94560  /* title color                              */
#define COL_TEXT      0xEEEEEE  /* light text                               */
#define COL_GAMEOVER  0xFF2222  /* game over red                            */
#define COL_SCORE     0xFFCC00  /* yellow score                             */
#define COL_STAR      0xFFFFAA  /* star/particle color                      */

/* ── Data structures ──────────────────────────────────────────────────────── */
typedef struct {
    int y;                     /* vertical position (x is fixed)             */
    int vy;                    /* vertical velocity                          */
    bool thrusting;            /* W key held?                                */
} Player;

typedef struct {
    int x, y;
    int w, h;
    bool active;
} Obstacle;

/* ── Module state ─────────────────────────────────────────────────────────── */
static game_state_t state = STATE_MENU;
static Player player;
static Obstacle obs[MAX_OBSTACLES];

static int  scroll_speed = 3;
static int  score        = 0;
static int  frame_count  = 0;
static int  spawn_timer  = 0;
static int  difficulty_timer = 0;

/* screen dimensions (from video_gr) */
static int  scr_w, scr_h;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void reset_game(void);
static void spawn_obstacle(void);

/* ═══════════════════ PUBLIC API ═══════════════════════════════════════════ */

game_state_t game_get_state(void) { return state; }

void game_init(void) {
    scr_w = h_res;
    scr_h = v_res;
    state  = STATE_MENU;
    srand(42); /* deterministic seed, fine for a game */
}

void game_cleanup(void) {
    /* nothing to free — all static */
}

/* ── Reset for a new round ────────────────────────────────────────────────── */
static void reset_game(void) {
    player.y  = scr_h / 2;
    player.vy = 0;
    player.thrusting = false;

    for (int i = 0; i < MAX_OBSTACLES; i++)
        obs[i].active = false;

    scroll_speed = 3;
    score = 0;
    frame_count = 0;
    spawn_timer = 0;
    difficulty_timer = 0;
}

/* ── Spawn a new obstacle on the right edge ───────────────────────────────── */
static void spawn_obstacle(void) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obs[i].active) {
            obs[i].active = true;
            obs[i].x = scr_w;
            obs[i].w = OBS_W;

            /* random height between OBS_MIN_H and OBS_MAX_H */
            obs[i].h = OBS_MIN_H + (rand() % (OBS_MAX_H - OBS_MIN_H + 1));

            /* random vertical position, leaving room for floor */
            int max_y = scr_h - FLOOR_H - obs[i].h;
            if (max_y < 0) max_y = 0;
            obs[i].y = rand() % (max_y + 1);
            return;
        }
    }
}

/* ═══════════════════ UPDATE (called every timer tick) ═════════════════════ */

void game_update(void) {
    if (state != STATE_PLAYING) return;

    frame_count++;

    /* ── Player physics ──────────────────────────────────────────────────── */
    if (player.thrusting)
        player.vy += THRUST;
    else
        player.vy += GRAVITY;

    /* clamp velocity */
    if (player.vy > MAX_VY)  player.vy = MAX_VY;
    if (player.vy < MIN_VY)  player.vy = MIN_VY;

    player.y += player.vy;

    /* screen boundaries */
    if (player.y < 0) {
        player.y = 0;
        player.vy = 0;
    }
    int floor_top = scr_h - FLOOR_H - PLAYER_H;
    if (player.y > floor_top) {
        player.y = floor_top;
        player.vy = 0;
    }

    /* ── Move obstacles ──────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obs[i].active) {
            obs[i].x -= scroll_speed;
            if (obs[i].x + obs[i].w < 0)
                obs[i].active = false;
        }
    }

    /* ── Spawn logic ─────────────────────────────────────────────────────── */
    spawn_timer++;
    int spawn_interval = OBS_GAP / scroll_speed; /* frames between spawns */
    if (spawn_interval < 20) spawn_interval = 20;
    if (spawn_timer >= spawn_interval) {
        spawn_obstacle();
        spawn_timer = 0;
    }

    /* ── Score ────────────────────────────────────────────────────────────── */
    score = frame_count / 6; /* roughly 10 points/second at 60fps */

    /* ── Difficulty ramp ─────────────────────────────────────────────────── */
    difficulty_timer++;
    if (difficulty_timer >= 300 && scroll_speed < 10) { /* every ~5 sec */
        scroll_speed++;
        difficulty_timer = 0;
    }

    /* ── Collision detection (AABB) ───────────────────────────────────────── */
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!obs[i].active) continue;
        if (PLAYER_X + PLAYER_W > obs[i].x &&
            PLAYER_X < obs[i].x + obs[i].w &&
            player.y + PLAYER_H > obs[i].y &&
            player.y < obs[i].y + obs[i].h) {
            state = STATE_GAMEOVER;
            return;
        }
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

        /* Title */
        draw_text("JETPACK JOYRIDE", scr_w/2 - 15*6*3/2, scr_h/4, 3, COL_TITLE);

        /* Subtitle */
        draw_text("LCOM EDITION", scr_w/2 - 12*6*2/2, scr_h/4 + 40, 2, COL_TEXT);

        /* Instructions */
        draw_text("PRESS W OR SPACE TO START", scr_w/2 - 25*6*1/2, scr_h/2 + 40, 1, COL_SCORE);
        draw_text("W TO FLY  ESC TO QUIT", scr_w/2 - 21*6*1/2, scr_h/2 + 70, 1, COL_TEXT);

        /* Animated player preview */
        int preview_y = scr_h/2 - 30 + ((frame_count / 8) % 2 == 0 ? -5 : 5);
        draw_rect(scr_w/2 - PLAYER_W/2, preview_y, PLAYER_W, PLAYER_H, COL_PLAYER);

        frame_count++;
    }
    else if (state == STATE_PLAYING) {
        /* ── GAME SCREEN ──────────────────────────────────────────────── */
        clear_buffer(COL_BG);

        /* Floor */
        draw_rect(0, scr_h - FLOOR_H, scr_w, FLOOR_H, COL_FLOOR);

        /* Floor line */
        draw_rect(0, scr_h - FLOOR_H, scr_w, 2, COL_OBS_TOP);

        /* Background particles (parallax-like scrolling dots) */
        for (int i = 0; i < 30; i++) {
            int px = (i * 137 - frame_count * 1) % scr_w;
            if (px < 0) px += scr_w;
            int py = (i * 53) % (scr_h - FLOOR_H);
            draw_rect(px, py, 1, 1, 0x333355);
        }

        /* Obstacles */
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            if (!obs[i].active) continue;
            draw_rect(obs[i].x, obs[i].y, obs[i].w, obs[i].h, COL_OBSTACLE);
            /* Top cap */
            draw_rect(obs[i].x - 2, obs[i].y, obs[i].w + 4, 4, COL_OBS_TOP);
            /* Bottom cap */
            draw_rect(obs[i].x - 2, obs[i].y + obs[i].h - 4, obs[i].w + 4, 4, COL_OBS_TOP);
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
        draw_text("SCORE:", scr_w/2 - 10*6*2/2, scr_h/2 - 20, 2, COL_TEXT);
        draw_text(sbuf, scr_w/2 + 2*6*2, scr_h/2 - 20, 2, COL_SCORE);

        draw_text("PRESS W TO RESTART", scr_w/2 - 18*6*1/2, scr_h/2 + 40, 1, COL_SCORE);
        draw_text("ESC TO QUIT", scr_w/2 - 11*6*1/2, scr_h/2 + 60, 1, COL_TEXT);
    }

    /* Always swap to VRAM */
    swap_buffers();
}

/* ═══════════════════ KEYBOARD INPUT ══════════════════════════════════════ */

void game_handle_kbd(uint8_t scancode) {
    /* ESC always quits */
    if (scancode == ESC_MAKE || scancode == 0x81) {
        state = STATE_EXIT;
        return;
    }

    switch (state) {
        case STATE_MENU:
            if (scancode == W_MAKE || scancode == SPACE_MAKE || scancode == ENTER_MAKE) {
                reset_game();
                state = STATE_PLAYING;
            }
            break;

        case STATE_PLAYING:
            if (scancode == W_MAKE || scancode == SPACE_MAKE)
                player.thrusting = true;
            else if (scancode == W_BREAK || scancode == SPACE_BREAK)
                player.thrusting = false;
            break;

        case STATE_GAMEOVER:
            if (scancode == W_MAKE || scancode == SPACE_MAKE || scancode == ENTER_MAKE) {
                reset_game();
                state = STATE_PLAYING;
            }
            break;

        default:
            break;
    }
}
