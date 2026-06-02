/**
 * @file model.c
 * @brief Game simulation: physics, spawning, collision, scoring. Operates only
 *        on a GameModel — no drawing, no device access.
 */
#include <stdlib.h>
#include "game/model.h"

/* ── Integer trigonometry (sin/cos scaled by 1024, ANG_STEPS per turn) ────── */
static const int sin1024[ANG_STEPS] = {
        0,   100,   200,   297,   392,   483,   569,   650,
      724,   792,   851,   903,   946,   980,  1004,  1019,
     1024,  1019,  1004,   980,   946,   903,   851,   792,
      724,   650,   569,   483,   392,   297,   200,   100,
        0,  -100,  -200,  -297,  -392,  -483,  -569,  -650,
     -724,  -792,  -851,  -903,  -946,  -980, -1004, -1019,
    -1024, -1019, -1004,  -980,  -946,  -903,  -851,  -792,
     -724,  -650,  -569,  -483,  -392,  -297,  -200,  -100,
};
int isin(int a) { return sin1024[((a % ANG_STEPS) + ANG_STEPS) % ANG_STEPS]; }
int icos(int a) { return isin(a + ANG_VERT); }

void zap_endpoints(const Zapper *z, int *ax, int *ay, int *bx, int *by) {
    int dx = (z->half * icos(z->ang)) / 1024;
    int dy = (z->half * isin(z->ang)) / 1024;
    *ax = z->cx - dx; *ay = z->cy - dy;
    *bx = z->cx + dx; *by = z->cy + dy;
}

/* Squared distance from point P to segment A-B (integer math). */
static long pt_seg_d2(int px, int py, int ax, int ay, int bx, int by) {
    long abx = bx - ax, aby = by - ay;
    long apx = px - ax, apy = py - ay;
    long len2 = abx * abx + aby * aby;
    long t = (len2 > 0) ? (apx * abx + apy * aby) : 0;
    if (t < 0)        t = 0;
    else if (t > len2) t = len2;
    long qx = ax + (len2 > 0 ? (abx * t) / len2 : 0);
    long qy = ay + (len2 > 0 ? (aby * t) / len2 : 0);
    long dx = px - qx, dy = py - qy;
    return dx * dx + dy * dy;
}

/* True if the player collides with the beam (sampled along the body). */
static bool zap_hits_player(const GameModel *m, const Zapper *z) {
    int ax, ay, bx, by;
    zap_endpoints(z, &ax, &ay, &bx, &by);
    int pcx = PLAYER_X + PLAYER_W / 2;
    long thr = BEAM_THICK + PLAYER_W / 2;
    long thr2 = thr * thr;
    for (int k = 1; k <= 3; k++) {
        int py = m->player.y + (PLAYER_H * k) / 4;
        if (pt_seg_d2(pcx, py, ax, ay, bx, by) < thr2)
            return true;
    }
    return false;
}

/* ── Particles and popups (logic only; drawing lives in the view) ─────────── */
static void spawn_particle(GameModel *m, int x, int y, int vx, int vy,
                           int life, uint32_t col) {
    for (int i = 0; i < MAX_PARTS; i++) {
        if (!m->parts[i].active) {
            m->parts[i].x = x; m->parts[i].y = y;
            m->parts[i].vx = vx; m->parts[i].vy = vy;
            m->parts[i].life = life; m->parts[i].active = 1; m->parts[i].col = col;
            return;
        }
    }
}
static void spawn_popup(GameModel *m, int x, int y) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!m->popups[i].active) {
            m->popups[i].x = x; m->popups[i].y = y;
            m->popups[i].life = POPUP_LIFE; m->popups[i].active = 1;
            return;
        }
    }
}
static void burst(GameModel *m, int x, int y, int n, uint32_t col) {
    for (int i = 0; i < n; i++)
        spawn_particle(m, x, y, (rand() % 13) - 6, (rand() % 13) - 8,
                       14 + (rand() % 12), col);
}
static void update_particles(GameModel *m) {
    for (int i = 0; i < MAX_PARTS; i++) {
        if (!m->parts[i].active) continue;
        m->parts[i].x += m->parts[i].vx;
        m->parts[i].y += m->parts[i].vy;
        m->parts[i].vy += 1;                      /* gravity */
        if (--m->parts[i].life <= 0) m->parts[i].active = 0;
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!m->popups[i].active) continue;
        m->popups[i].y -= 1;
        if (--m->popups[i].life <= 0) m->popups[i].active = 0;
    }
    if (m->hit_flash > 0) m->hit_flash--;
}

/* Player death: explosion + red flash, then game over. */
static void player_explode(GameModel *m) {
    int cx = PLAYER_X + PLAYER_W / 2, cy = m->player.y + PLAYER_H / 2;
    burst(m, cx, cy, EXPLODE_BURST_RED,  COL_GAMEOVER);
    burst(m, cx, cy, EXPLODE_BURST_GOLD, COL_SCORE);
    burst(m, cx, cy, EXPLODE_BURST_FIRE, 0xFF8800);
    m->hit_flash = 8;
    if (m->score > m->high_score) m->high_score = m->score;
    m->state = STATE_GAMEOVER;
}

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
static void spawn_zapper(GameModel *m) {
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
static void spawn_powerup(GameModel *m) {
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

/* ── Public API ───────────────────────────────────────────────────────────── */

void model_init(GameModel *m, int scr_w, int scr_h, unsigned seed) {
    m->scr_w = scr_w;
    m->scr_h = scr_h;
    m->state = STATE_MENU;
    m->menu_sel = 0;
    m->mouse_x = scr_w / 2;
    m->mouse_y = scr_h / 2;
    m->lb_prev = false;
    m->high_score = 0;
    m->scroll_speed = SCROLL_SPEED_MENU;
    m->frame_count = 0;
    m->anim = 0;
    m->hit_flash = 0;
    m->clock_str[0] = '\0';
    m->date_str[0] = '\0';
    m->versus = false;
    m->opp_alive = false;
    m->opp_y = scr_h / 2;
    m->opp_score = 0;
    m->dead_sent = false;
    m->net_silence = 0;
    m->shield_timer = 0;
    m->magnet_timer = 0;
    m->powerup_timer = 0;
    m->high_count = 0;        /* the glue layer loads the saved table */
    m->last_rank = -1;
    for (int i = 0; i < MAX_PARTS; i++)    m->parts[i].active    = 0;
    for (int i = 0; i < MAX_POPUPS; i++)   m->popups[i].active   = 0;
    for (int i = 0; i < MAX_POWERUPS; i++) m->powerups[i].active = false;
    srand(seed);
}

void model_reset(GameModel *m) {
    m->player.y  = m->scr_h / 2;
    m->player.vy = 0;
    m->player.thrusting = false;

    for (int i = 0; i < MAX_ZAPPERS; i++)  m->zaps[i].active     = false;
    for (int i = 0; i < MAX_COINS; i++)    m->coins[i].active    = false;
    for (int i = 0; i < MAX_MISSILES; i++) m->missiles[i].active = 0;
    for (int i = 0; i < MAX_PARTS; i++)    m->parts[i].active    = 0;
    for (int i = 0; i < MAX_POPUPS; i++)   m->popups[i].active   = 0;
    for (int i = 0; i < MAX_POWERUPS; i++) m->powerups[i].active = false;
    m->hit_flash = 0;

    m->scroll_speed = SCROLL_SPEED_START;
    m->score = 0;
    m->coins_collected = 0;
    m->frame_count = 0;
    m->spawn_timer = 0;
    m->missile_timer = 0;
    m->difficulty_timer = 0;
    m->shield_timer = 0;
    m->magnet_timer = 0;
    m->powerup_timer = 0;
    m->last_rank = -1;

    /* Opponent state for this round (versus flag is set by the controller). */
    m->opp_alive = true;
    m->opp_y = m->scr_h / 2;
    m->opp_score = 0;
    m->dead_sent = false;
    m->net_silence = 0;
}

void model_update(GameModel *m) {
    m->anim++;                       /* free-running animation clock */
    update_particles(m);             /* keep effects alive even on game over */
    if (m->state != STATE_PLAYING) return;

    m->frame_count++;

    /* Power-up effect timers tick down */
    if (m->shield_timer > 0) m->shield_timer--;
    if (m->magnet_timer > 0) m->magnet_timer--;

    /* Player physics */
    if (m->player.thrusting) m->player.vy += THRUST;
    else m->player.vy += GRAVITY;

    if (m->player.vy > MAX_VY)  m->player.vy = MAX_VY;
    if (m->player.vy < MIN_VY)  m->player.vy = MIN_VY;
    m->player.y += m->player.vy;

    if (m->player.y < 0) { m->player.y = 0; m->player.vy = 0; }
    int floor_top = m->scr_h - FLOOR_H - PLAYER_H;
    if (m->player.y > floor_top) { m->player.y = floor_top; m->player.vy = 0; }

    /* Jetpack exhaust particles while thrusting */
    if (m->player.thrusting) {
        spawn_particle(m, PLAYER_X + 2, m->player.y + 34, -(2 + rand() % 3),
                       1 + rand() % 2, 8 + rand() % 6,
                       (rand() % 2) ? COL_SCORE : 0xFF8800);
    }

    /* Move and rotate zappers */
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!m->zaps[i].active) continue;
        m->zaps[i].cx -= m->scroll_speed;
        if (m->zaps[i].spin)
            m->zaps[i].ang = (m->zaps[i].ang + m->zaps[i].spin) % ANG_STEPS;
        if (m->zaps[i].cx + m->zaps[i].half < -20) m->zaps[i].active = false;
    }

    /* Move coins; cull any that drift too close to a beam */
    long coin_safe2 = (long)(BEAM_THICK + COIN_SIZE / 2 + 18) *
                      (BEAM_THICK + COIN_SIZE / 2 + 18);
    for (int i = 0; i < MAX_COINS; i++) {
        if (!m->coins[i].active) continue;
        m->coins[i].x -= m->scroll_speed;
        if (m->coins[i].x + COIN_SIZE < 0) { m->coins[i].active = false; continue; }

        /* Magnet power-up: drag nearby coins toward the player. */
        if (m->magnet_timer > 0) {
            int pcx = PLAYER_X + PLAYER_W / 2, pcy = m->player.y + PLAYER_H / 2;
            int ccx0 = m->coins[i].x + COIN_SIZE / 2, ccy0 = m->coins[i].y + COIN_SIZE / 2;
            int ddx = pcx - ccx0, ddy = pcy - ccy0;
            if ((long)ddx * ddx + (long)ddy * ddy < (long)MAGNET_RANGE * MAGNET_RANGE) {
                int sx = (ddx > 0) ? 1 : -1, sy = (ddy > 0) ? 1 : -1;
                int ax = abs(ddx), ay = abs(ddy);
                m->coins[i].x += sx * (ax < MAGNET_PULL ? ax : MAGNET_PULL);
                m->coins[i].y += sy * (ay < MAGNET_PULL ? ay : MAGNET_PULL);
            }
        }

        int ccx = m->coins[i].x + COIN_SIZE / 2;
        int ccy = m->coins[i].y + COIN_SIZE / 2;
        for (int j = 0; j < MAX_ZAPPERS; j++) {
            if (!m->zaps[j].active) continue;
            int ax, ay, bx, by;
            zap_endpoints(&m->zaps[j], &ax, &ay, &bx, &by);
            if (pt_seg_d2(ccx, ccy, ax, ay, bx, by) < coin_safe2) {
                m->coins[i].active = false;
                break;
            }
        }
    }

    /* Missiles */
    for (int i = 0; i < MAX_MISSILES; i++) {
        Missile *mi = &m->missiles[i];
        if (mi->warning > 0) {
            mi->warning--;
            if (mi->warning == 0) {
                mi->active = 1;
                mi->x = m->scr_w;
                mi->y = 30 + (rand() % (m->scr_h - FLOOR_H - 60));
                mi->speed = MISSILE_SPEED + (m->scroll_speed / 2);
            }
        }
        if (mi->active) {
            mi->x -= mi->speed;
            if (mi->x + MISSILE_W < 0) mi->active = 0;
        }
    }

    m->missile_timer++;
    if (m->missile_timer >= MISSILE_WARN * 2) {
        m->missile_timer = 0;
        for (int i = 0; i < MAX_MISSILES; i++) {
            if (m->missiles[i].warning == 0 && !m->missiles[i].active) {
                m->missiles[i].warning = MISSILE_WARN;
                m->missiles[i].y = 30 + (rand() % (m->scr_h - FLOOR_H - 60));
                break;
            }
        }
    }

    /* Zapper spawn logic */
    m->spawn_timer++;
    int spawn_interval = ZAPPER_GAP / m->scroll_speed;
    if (spawn_interval < SPAWN_INTERVAL_MIN) spawn_interval = SPAWN_INTERVAL_MIN;
    if (m->spawn_timer >= spawn_interval) {
        spawn_zapper(m);
        m->spawn_timer = 0;
    }

    /* Coin collection */
    for (int i = 0; i < MAX_COINS; i++) {
        if (!m->coins[i].active) continue;
        if (PLAYER_X + PLAYER_W > m->coins[i].x &&
            PLAYER_X < m->coins[i].x + COIN_SIZE &&
            m->player.y + PLAYER_H > m->coins[i].y &&
            m->player.y < m->coins[i].y + COIN_SIZE) {
            m->coins[i].active = false;
            m->coins_collected++;
            int ccx = m->coins[i].x + COIN_SIZE / 2, ccy = m->coins[i].y + COIN_SIZE / 2;
            for (int p = 0; p < COIN_PICK_PARTS; p++)
                spawn_particle(m, ccx, ccy, (rand() % 9) - 4, (rand() % 7) - 5,
                               10 + rand() % 8, COL_COIN_SHINE);
            spawn_popup(m, m->coins[i].x, m->coins[i].y - 6);
        }
    }

    /* Power-ups: spawn on a cadence, scroll left, collect on overlap */
    m->powerup_timer++;
    if (m->powerup_timer >= POWERUP_INTERVAL) {
        m->powerup_timer = 0;
        spawn_powerup(m);
    }
    for (int i = 0; i < MAX_POWERUPS; i++) {
        PowerUp *pu = &m->powerups[i];
        if (!pu->active) continue;
        pu->x -= m->scroll_speed;
        if (pu->x + PU_SIZE < 0) { pu->active = false; continue; }

        if (PLAYER_X + PLAYER_W > pu->x && PLAYER_X < pu->x + PU_SIZE &&
            m->player.y + PLAYER_H > pu->y && m->player.y < pu->y + PU_SIZE) {
            if (pu->type == PU_SHIELD) m->shield_timer = PU_DURATION;
            else                        m->magnet_timer = PU_DURATION;
            uint32_t col = (pu->type == PU_SHIELD) ? COL_SHIELD : COL_MAGNET;
            int pcx = pu->x + PU_SIZE / 2, pcy = pu->y + PU_SIZE / 2;
            for (int p = 0; p < 10; p++)
                spawn_particle(m, pcx, pcy, (rand() % 11) - 5, (rand() % 11) - 5,
                               12 + rand() % 8, col);
            pu->active = false;
        }
    }

    /* Scoring */
    m->score = (m->frame_count / SCORE_DIST_DIV) + m->coins_collected * SCORE_PER_COIN;

    /* Difficulty ramp */
    m->difficulty_timer++;
    if (m->difficulty_timer >= DIFFICULTY_INTERVAL && m->scroll_speed < SCROLL_SPEED_MAX) {
        m->scroll_speed++;
        m->difficulty_timer = 0;
    }

    /* Collision: zappers (point-to-beam distance). Shield absorbs the hit. */
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!m->zaps[i].active) continue;
        if (zap_hits_player(m, &m->zaps[i])) {
            if (m->shield_timer > 0) break;          /* invulnerable: shrug it off */
            player_explode(m); return;
        }
    }

    /* Collision: missiles. Shield destroys the missile instead of the player. */
    for (int i = 0; i < MAX_MISSILES; i++) {
        if (!m->missiles[i].active) continue;
        Missile *mi = &m->missiles[i];
        if (PLAYER_X + PLAYER_W > mi->x &&
            PLAYER_X < mi->x + MISSILE_W &&
            m->player.y + PLAYER_H > mi->y &&
            m->player.y < mi->y + MISSILE_H) {
            if (m->shield_timer > 0) {
                mi->active = 0;
                burst(m, mi->x, mi->y + MISSILE_H / 2, EXPLODE_BURST_FIRE, COL_SHIELD);
                continue;
            }
            player_explode(m); return;
        }
    }
}
