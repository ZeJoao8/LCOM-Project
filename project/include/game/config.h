#ifndef _GAME_CONFIG_H_
#define _GAME_CONFIG_H_

/**
 * @file config.h
 * @brief Central tuning table for the game: every gameplay constant, colour and
 *        UI dimension lives here so there are no magic numbers scattered across
 *        the model / view / controller modules.
 */

#include <stdint.h>
#include <stdbool.h>

/* ── Physics ──────────────────────────────────────────────────────────────── */
#define GRAVITY        1     /* downward acceleration per frame                */
#define THRUST        -3     /* upward acceleration while the jetpack is on    */
#define MAX_VY         6     /* terminal velocity falling                      */
#define MIN_VY        -6     /* terminal velocity rising                       */

/* ── Player / layout ──────────────────────────────────────────────────────── */
#define PLAYER_SCALE   1
#define PLAYER_W      24
#define PLAYER_H      40
#define PLAYER_X      90
#define FLOOR_H       40

/* ── Zappers (electric beams between two orbs) ────────────────────────────── */
#define ZT_HORIZ       0     /* static horizontal beam                         */
#define ZT_VERT        1     /* static vertical beam                           */
#define ZT_DIAG        2     /* static 45° beam                                */
#define ZT_SPIN        3     /* beam rotating 360° around its centre           */

#define MAX_ZAPPERS   12
#define ANG_STEPS     64     /* angular resolution of a full circle            */
#define ANG_VERT      16     /* 90° in angle steps                             */
#define ANG_DIAG       8     /* 45°                                            */
#define BEAM_MIN_HALF 70     /* half-length min -> beams of 140..280px         */
#define BEAM_MAX_HALF 140
#define BEAM_THICK    10     /* beam half-thickness for collision              */
#define ORB_R         11     /* radius of the orbs at the beam tips            */
#define ZAP_SPIN_SPEED 1     /* angle steps per frame while spinning           */
#define ZAPPER_GAP    210    /* horizontal spacing between spawns              */
#define COIN_CLEAR    34     /* min vertical gap between coins and a beam       */

/* ── Coins ────────────────────────────────────────────────────────────────── */
#define MAX_COINS     30
#define COIN_SCALE     1
#define COIN_SIZE     16
#define COIN_STEP     22     /* horizontal spacing between coins in a row       */
#define COIN_FRAMES    4     /* spin animation frames                          */

/* ── Missiles ─────────────────────────────────────────────────────────────── */
#define MAX_MISSILES   4
#define MISSILE_SCALE  2
#define MISSILE_H    (8  * MISSILE_SCALE)   /* 16 */
#define MISSILE_W    (20 * MISSILE_SCALE)   /* 40 */
#define MISSILE_WARN  50     /* warning frames before a missile spawns          */
#define MISSILE_SPEED  8

/* ── Particles / popups (juice) ───────────────────────────────────────────── */
#define MAX_PARTS     48
#define MAX_POPUPS     6
#define EXPLODE_BURST_RED    24   /* explosion: red shrapnel particles  */
#define EXPLODE_BURST_GOLD   12   /* explosion: gold particles          */
#define EXPLODE_BURST_FIRE    8   /* explosion: orange embers           */
#define COIN_PICK_PARTS       8   /* sparkles spawned per coin picked    */
#define POPUP_LIFE           36   /* frames a "+10" popup lives          */

/* ── Coin rows (how many coins a spawn drops) ─────────────────────────────── */
#define COIN_ROW_MIN          3   /* fewest coins in a row               */
#define COIN_ROW_VAR          4   /* extra coins: rand() % COIN_ROW_VAR  */

/* ── Power-ups (floating pickups with a timed effect) ─────────────────────── */
#define MAX_POWERUPS          2
#define PU_SHIELD             0   /* temporary invulnerability           */
#define PU_MAGNET             1   /* pulls nearby coins to the player    */
#define PU_TYPES              2
#define PU_SIZE              20   /* pickup hitbox / sprite size (px)     */
#define PU_DURATION         300   /* effect length in frames (~5 s)      */
#define POWERUP_INTERVAL    250   /* frames between power-up spawns       */
#define MAGNET_RANGE        150   /* magnet pull radius (px)             */
#define MAGNET_PULL           5   /* px a coin moves toward player/frame  */

/* ── Persistent high-score table (RTC-timestamped, saved to disk) ─────────── */
#define MAX_SCORES            5   /* leaderboard entries kept            */
#define SCORE_WHEN_LEN       20   /* "DD/MM/YY HH:MM" + NUL              */
#ifndef SCORES_PATH
#define SCORES_PATH "/home/lcom/labs/project/data/jetpack_scores.txt"  /* change for your VM */
#endif

/* ── Difficulty / scoring ─────────────────────────────────────────────────── */
#define SCROLL_SPEED_MENU    2   /* idle scroll speed on menus                  */
#define SCROLL_SPEED_START   3   /* scroll speed at the start of a run          */
#define SCROLL_SPEED_MAX     9   /* cap                                         */
#define DIFFICULTY_INTERVAL 300  /* frames between speed bumps                  */
#define SPAWN_INTERVAL_MIN   20  /* floor on the zapper spawn interval          */
#define SCORE_DIST_DIV        6  /* distance score = frame_count / this         */
#define SCORE_PER_COIN       10

/* ── Colours (24-bit, mode 0x115 = 800x600x24) ────────────────────────────── */
#define COL_BG         0x1A1A2E
#define COL_FLOOR      0x16213E
#define COL_PLAYER     0xE94560
#define COL_ZAP_BODY   0xFFCC00  /* amber beam body  */
#define COL_ZAP_TIP    0x888888  /* grey beam tip    */
#define COL_ZAP_GLOW   0xFFFF88  /* bright yellow glow*/
#define COL_MENU_BG    0x0F0F23
#define COL_TITLE      0xE94560
#define COL_TEXT       0xEEEEEE
#define COL_GAMEOVER   0xFF2222
#define COL_SCORE      0xFFCC00
#define COL_STAR       0xFFFFAA
#define COL_COIN       0xFFD700
#define COL_COIN_SHINE 0xFFFF88
#define COL_WARN       0xFF0000  /* missile warning red */
#define COL_OBS_TOP    0x4444AA  /* menu / game-over button border */
#define COL_BTN_BG     0x1A1A3A  /* idle button background */
#define COL_BTN_HI     0xFFFFFF  /* highlighted text       */
#define COL_GHOST      0x3CC8FF  /* opponent ghost (2-player) */
#define COL_P2         0x3366FF  /* solid blue for the P2 label / HUD       */
#define COL_SHIELD     0x33CCFF  /* shield power-up + aura */
#define COL_MAGNET     0xC061FF  /* magnet power-up */
#define COL_SHADOW     0x0A0A12  /* text drop-shadow */

/* Parallax background layers (drawn procedurally, no image assets). */
#define COL_SKY_TOP    0x141430  /* top sky band   */
#define COL_SKY_BOT    0x2A2350  /* horizon band   */
#define COL_CITY_FAR   0x1E1E3C  /* distant skyline */
#define COL_CITY_NEAR  0x12203A  /* nearer skyline  */
#define COL_CITY_FORE  0x0C1426  /* closest skyline (darkest, fastest) */
#define COL_MOON       0xF4F1D7  /* moon disc          */
#define COL_MOON_GLOW  0x3A3A5E  /* soft halo + craters */
#define COL_CLOUD      0x232348  /* drifting clouds    */

/* Ground + motion cues (sense of speed). */
#define COL_FLOOR_SEAM 0x0E1730  /* vertical seams between floor tiles */
#define COL_FLOOR_DASH 0x24345C  /* scrolling hazard dashes on the floor */

/* ── Transitions / motion ─────────────────────────────────────────────────── */
#define FADE_FRAMES    12        /* fade-in length (frames) on a state change   */
#define FLOOR_TILE     48        /* width of one scrolling floor tile (px)      */

/* ── Netplay (serial 2-player) ────────────────────────────────────────────── */
#define NET_SEND_INTERVAL  3      /* frames between state broadcasts */
#define NET_PEER_TIMEOUT  240     /* frames without a packet => peer is "out" */
#define NET_HELLO_INTERVAL 10     /* frames between HELLO/READY beacons in WAITING */
#define NET_MAX_PAYLOAD    8      /* upper bound on a packet's payload length */

/* ── Menu / button geometry ───────────────────────────────────────────────── */
#define MENU_ITEMS          6
#define BTN_W             240
#define BTN_H              30
#define BTN_GAP            50
#define BTN_START_Y_OFF    10    /* offset below scr_h/2 for the first button   */
#define BACK_W            140
#define BACK_H             28
#define BACK_Y_FROM_BOTTOM 80

/** Rectangle of menu button @p idx, centred for the given screen size. */
static inline void menu_btn_rect(int scr_w, int scr_h, int idx,
                                 int *x, int *y, int *w, int *h) {
    *w = BTN_W;
    *h = BTN_H;
    *x = scr_w / 2 - BTN_W / 2;
    *y = scr_h / 2 - 40 + idx * 40;
}

/** Rectangle of settings button @p idx, centred for the given screen size. */
static inline void settings_btn_rect(int scr_w, int scr_h, int idx,
                                     int *x, int *y, int *w, int *h) {
    *w = 320;
    *h = BTN_H;
    *x = scr_w / 2 - 320 / 2;
    *y = scr_h / 2 - 100 + idx * 45;
}

/** Rectangle of the BACK button (instructions / game-over screens). */
static inline void back_btn_rect(int scr_w, int scr_h,
                                 int *x, int *y, int *w, int *h) {
    *w = BACK_W;
    *h = BACK_H;
    *x = scr_w / 2 - BACK_W / 2;
    *y = scr_h - BACK_Y_FROM_BOTTOM;
}

/** True if point (px,py) is inside the rectangle (x,y,w,h). */
static inline bool pt_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

#endif /* _GAME_CONFIG_H_ */
