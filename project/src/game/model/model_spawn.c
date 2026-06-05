/**
 * @file model_spawn.c
 * @brief Spawning of obstacles and pickups: coin rows, zappers and power-ups.
 */
#include <stdlib.h>            /* abs */
#include "game/model.h"
#include "game/model_internal.h"

/**
 * @brief Drop a short row of coins inside the safe vertical band [band_lo, band_hi].
 * @param m       Game model.
 * @param cx      X position of the first coin.
 * @param band_lo Top of the safe band.
 * @param band_hi Bottom of the safe band.
 * @param pattern Row shape: 0 arc up, 1 arc down, 2 straight, 3 zig-zag.
 */
static void spawn_coins(GameModel *m, int cx, int band_lo, int band_hi, int pattern) {
    if (band_hi - band_lo < COIN_SIZE) return;        /* corridor too tight */

    int cy    = (band_lo + band_hi) / 2;
    int count = COIN_ROW_MIN + (world_rand() % COIN_ROW_VAR);   /* 3-6 coins */
    int span  = (count - 1) * COIN_STEP;
    int placed = 0;
    for (int i = 0; i < MAX_COINS && placed < count; i++) {
        if (m->coins[i].active) continue;
        m->coins[i].active = true;

        int off = placed * COIN_STEP;
        int d   = off - span / 2;                 /* distance from row centre */
        int x   = cx + off;
        int y   = cy - COIN_SIZE / 2;             /* centre the coin on cy */

        switch (pattern) {
            case 0: y -= 20 - abs(d) / 4; break;  /* arc up   (∩) */
            case 1: y += 20 - abs(d) / 4; break;  /* arc down (∪) */
            case 2: break;                        /* straight line */
            case 3: y += (placed % 2) ? 12 : -12; break; /* zig-zag */
        }
        if (y < band_lo)              y = band_lo;
        if (y > band_hi - COIN_SIZE)  y = band_hi - COIN_SIZE;

        m->coins[i].x = x;
        m->coins[i].y = y;
        placed++;
    }
}

/**
 * @brief Spawn one beam zapper at the right edge with a random type, length and
 *        position, then drop a coin row in the wider corridor beside it.
 * @param m Game model.
 */
void spawn_zapper(GameModel *m) {
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (m->zaps[i].active) continue;

        Zapper *z = &m->zaps[i];
        int pf = m->scr_h - FLOOR_H;              /* play-field height */

        z->active = true;
        z->type   = world_rand() % 4;
        z->half   = BEAM_MIN_HALF + (world_rand() % (BEAM_MAX_HALF - BEAM_MIN_HALF + 1));

        switch (z->type) {
            case ZT_HORIZ: z->ang = 0;        z->spin = 0; break;
            case ZT_VERT:  z->ang = ANG_VERT; z->spin = 0; break;
            case ZT_DIAG:  z->ang = (world_rand() % 2) ? ANG_DIAG : (ANG_STEPS - ANG_DIAG);
                           z->spin = 0; break;
            default:       z->ang = world_rand() % ANG_STEPS; z->spin = ZAP_SPIN_SPEED;
                           z->half = 60 + (world_rand() % 50);
                           break;
        }

        /* Worst-case vertical extent — keep a playable corridor. */
        int vh = (z->type == ZT_HORIZ) ? BEAM_THICK : z->half;
        int lo = vh + 20, hi = pf - vh - 20;
        if (hi < lo) hi = lo;
        z->cy = lo + (world_rand() % (hi - lo + 1));
        z->cx = m->scr_w + z->half + 20;          /* whole beam starts off-screen */

        /* Drop a coin row in the wider corridor, away from the beam. */
        int topgap = z->cy - vh;
        int botgap = pf - (z->cy + vh);
        int band_lo, band_hi;
        if (topgap >= botgap) { band_lo = 6;                  band_hi = z->cy - vh - COIN_CLEAR; }
        else                  { band_lo = z->cy + vh + COIN_CLEAR; band_hi = pf - 6; }
        spawn_coins(m, z->cx + z->half + 30, band_lo, band_hi, world_rand() % 4);
        return;
    }
}

/**
 * @brief Spawn one floating power-up of a random type at the right edge, within
 *        the playable vertical band.
 * @param m Game model.
 */
void spawn_powerup(GameModel *m) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (m->powerups[i].active) continue;
        int pf = m->scr_h - FLOOR_H;
        m->powerups[i].active = true;
        m->powerups[i].type   = world_rand() % PU_TYPES;
        m->powerups[i].x      = m->scr_w + PU_SIZE;
        m->powerups[i].y      = 30 + (world_rand() % (pf - 60 - PU_SIZE));
        return;
    }
}
