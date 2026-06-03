#ifndef _GAME_ENTITIES_H_
#define _GAME_ENTITIES_H_

/**
 * @file entities.h
 * @brief Plain data structures for every game object plus the aggregate
 *        GameModel that holds the whole game state. This is the "M" of MVC:
 *        no behaviour, no device access — just data.
 */

#include <stdint.h>
#include <stdbool.h>
#include "game/game.h"      /* game_state_t */
#include "game/config.h"    /* MAX_* sizes  */

/** Vertical-only player; horizontal position is fixed at PLAYER_X. */
typedef struct {
    int  y, vy;
    bool thrusting;
} Player;

/** Electric beam between two orbs. cx scrolls left over time. */
typedef struct {
    bool active;
    int  type;   /* ZT_HORIZ, ZT_VERT, ZT_DIAG, ZT_SPIN */
    int  cx, cy; /* centre of the beam (cx scrolls left)  */
    int  half;   /* half length of the beam               */
    int  ang;    /* current angle (0..ANG_STEPS-1)        */
    int  spin;   /* angle increment per frame (0 = still)  */
} Zapper;

/** Collectible coin. */
typedef struct {
    int  x, y;
    bool active;
} Coin;

/** Homing-less missile that flies straight left after a warning. */
typedef struct {
    int y;
    int warning;  /* frames left until the missile spawns */
    int active;
    int x;
    int speed;
} Missile;

/** Short-lived visual particle (jetpack exhaust, coin shine, explosion). */
typedef struct {
    int x, y, vx, vy, life, active;
    uint32_t col;
} Particle;

/** "+10" score popup. */
typedef struct {
    int x, y, life, active;
} Popup;

/** Floating power-up pickup; scrolls left like the other obstacles. */
typedef struct {
    int  x, y;
    bool active;
    int  type;   /* PU_SHIELD, PU_MAGNET */
} PowerUp;

/** One leaderboard row: a score and when it was achieved (from the RTC). */
typedef struct {
    int  score;
    char when[SCORE_WHEN_LEN];   /* "DD/MM/YY HH:MM" */
} ScoreEntry;

/**
 * Whole game state. A single instance lives in game.c; the model mutates it,
 * the view reads it (const), the controller feeds input into it.
 */
typedef struct {
    game_state_t state;

    Player   player;
    Zapper   zaps[MAX_ZAPPERS];
    Coin     coins[MAX_COINS];
    Missile  missiles[MAX_MISSILES];
    Particle parts[MAX_PARTS];
    Popup    popups[MAX_POPUPS];
    PowerUp  powerups[MAX_POWERUPS];

    int hit_flash;            /* red flash frames after a hit                  */
    int scroll_speed;
    int score;
    int coins_collected;
    int high_score;           /* best score this session                      */

    /* Active power-up effects (remaining frames; 0 = inactive) */
    int shield_timer;
    int magnet_timer;
    int powerup_timer;        /* cadence counter for spawning power-ups        */

    int frame_count;          /* advances only while PLAYING (distance/score)  */
    int anim;                 /* advances every frame (menus + animations)     */
    int spawn_timer;
    int missile_timer;
    int difficulty_timer;

    int scr_w, scr_h;         /* screen size, set at init                      */

    /* UI / input state */
    int  menu_sel;
    int  mouse_x, mouse_y;
    bool lb_prev;

    /* RTC clock strings, refreshed once per second by the glue layer */
    char clock_str[12];   /* "HH:MM:SS" */
    char date_str[12];    /* "DD/MM/YY" */

    /* 2-player (serial) state */
    bool versus;          /* this run is a serial 2-player race        */
    bool opp_alive;       /* opponent still playing                    */
    int  opp_y;           /* opponent's reported vertical position     */
    int  opp_score;       /* opponent's reported score                 */
    bool dead_sent;       /* our DEAD packet already sent this round   */
    int  net_silence;     /* frames since the last packet from the peer */

    /* Handshake state, valid while STATE_WAITING. Owned by start_game. */
    bool peer_seen;       /* we received a HELLO since entering WAITING */
    bool peer_ready;      /* we received a READY since entering WAITING */
    int  wait_ctr;        /* frames spent in WAITING (UI + send cadence) */

    /* Persistent leaderboard (loaded/saved by the glue layer). */
    ScoreEntry high_scores[MAX_SCORES];
    int        high_count;    /* number of valid entries in high_scores  */
    int        last_rank;     /* rank (0-based) this run earned, or -1   */

    /* Settings options */
    int        difficulty;         /* 0 = EASY, 1 = NORMAL, 2 = HARD */
    int        control_mode;       /* 0 = W / SPACE, 1 = UP / ENTER, 2 = Z / X */
    bool       visual_effects;     /* true = ON, false = OFF */
    bool       initial_speed_fast; /* true = FAST, false = NORMAL */
} GameModel;

#endif /* _GAME_ENTITIES_H_ */
