/**
 * @file model_spawn.c
 * @brief Spawning of obstacles and pickups: coin rows, zappers and power-ups.
 */
#include <stdlib.h>            /* rand, abs */
#include "game/model.h"
#include "game/model_internal.h"

/* Drop a short row of coins inside a safe vertical band [lo, hi]. */
static void spawn_coins(GameModel *m, int cx, int band_lo, int band_hi, int pattern) {
    if (band_hi - band_lo < COIN_SIZE) return;        /* corridor too tight */

    int cy    = (band_lo + band_hi) / 2;
    int count = COIN_ROW_MIN + (rand() % COIN_ROW_VAR);   /* 3-6 coins */
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

/* Spawn a single beam zapper from the right edge. */
void spawn_zapper(GameModel *m) {
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (m->zaps[i].active) continue;

        Zapper *z = &m->zaps[i];
        int pf = m->scr_h - FLOOR_H;              /* play-field height */

        z->active = true;
        z->type   = rand() % 4;
        z->half   = BEAM_MIN_HALF + (rand() % (BEAM_MAX_HALF - BEAM_MIN_HALF + 1));

        switch (z->type) {
            case ZT_HORIZ: z->ang = 0;        z->spin = 0; break;
            case ZT_VERT:  z->ang = ANG_VERT; z->spin = 0; break;
            case ZT_DIAG:  z->ang = (rand() % 2) ? ANG_DIAG : (ANG_STEPS - ANG_DIAG);
                           z->spin = 0; break;
            default:       z->ang = rand() % ANG_STEPS; z->spin = ZAP_SPIN_SPEED;
                           z->half = 60 + (rand() % 50);   /* smaller, fairer sweep */
                           break;
        }

        /* Worst-case vertical extent — keep a playable corridor. */
        int vh = (z->type == ZT_HORIZ) ? BEAM_THICK : z->half;
        int lo = vh + 20, hi = pf - vh - 20;
        if (hi < lo) hi = lo;
        z->cy = lo + (rand() % (hi - lo + 1));
        z->cx = m->scr_w + z->half + 20;          /* whole beam starts off-screen */

        /* Drop a coin row in the wider corridor, away from the beam. */
        int topgap = z->cy - vh;
        int botgap = pf - (z->cy + vh);
        int band_lo, band_hi;
        if (topgap >= botgap) { band_lo = 6;                  band_hi = z->cy - vh - COIN_CLEAR; }
        else                  { band_lo = z->cy + vh + COIN_CLEAR; band_hi = pf - 6; }
        spawn_coins(m, z->cx + z->half + 30, band_lo, band_hi, rand() % 4);
        return;
    }
}

/* Spawn one floating power-up from the right edge, inside the playable band. */
void spawn_powerup(GameModel *m) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (m->powerups[i].active) continue;
        int pf = m->scr_h - FLOOR_H;
        m->powerups[i].active = true;
        m->powerups[i].type   = rand() % PU_TYPES;
        m->powerups[i].x      = m->scr_w + PU_SIZE;
        m->powerups[i].y      = 30 + (rand() % (pf - 60 - PU_SIZE));
        return;
    }
}
