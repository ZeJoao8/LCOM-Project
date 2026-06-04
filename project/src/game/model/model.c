/**
 * @file model.c
 * @brief Game simulation entry points: lifecycle (init/reset) and the one-tick
 *        update that drives physics, spawning, collection, scoring and
 *        collisions. The heavy lifting lives in the sibling model_*.c files;
 *        this file orchestrates them. No drawing, no device access.
 */
#include <stdlib.h>
#include "game/model.h"
#include "game/model_internal.h"
#include "game/game.h"

/* ── Power-aware helpers ──────────────────────────────────────────────────── */
 
int model_coin_value(const GameModel *m) {
    return game_has_power(POWER_DOUBLE_COINS) ? 2 : 1;
}
 
int model_missile_speed(const GameModel *m, int base_speed) {
    if (game_has_power(POWER_SLOW_MISSILES))
        return base_speed * 3 / 4;   /* 25% mais lento */
    return base_speed;
}


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

    m->xp_total        = 0;
    m->powers_unlocked = 0;
    m->extra_life_used = false;
      /* Login state  */
    m->input_name[0]     = '\0';  m->input_name_len     = 0;
    m->input_password[0] = '\0';  m->input_password_len = 0;
    m->typing_password   = false;
    m->login_error       = false;
    m->logged_in         = false;

    m->lb_count        = 0;
    m->lb_scroll       = 0;


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
    m->extra_life_used = false;   /*reset extra life por run */

    /* POWER_SHIELD_START — dá shield no arranque de cada run */
    if (game_has_power(POWER_SHIELD_START))
        m->shield_timer = PU_DURATION;


    /* Opponent state for this round (versus flag is set by the controller). */
    m->opp_alive = true;
    m->opp_y = m->scr_h / 2;
    m->opp_score = 0;
    m->dead_sent = false;
    m->net_silence = 0;

    if (m->input_name_len == 0) {
        m->input_password[0] = '\0';
        m->input_password_len = 0;
        m->typing_password = false;
    }
    m->login_error = false;
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
        if (m->zaps[i].spin && (m->frame_count % 3 == 0))
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

                int base = MISSILE_SPEED + (m->scroll_speed / 2);

                mi->speed = model_missile_speed(m, base);
            }
        }
        if (mi->active) {
            mi->x -= mi->speed;
            if (mi->x + MISSILE_W < 0){
                 mi->active = 0;
                game_add_xp(XP_PER_MISSILE);  /* XP por míssil esquivado */
            } 
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

            int val = model_coin_value(m);           /* FEATURE: 1 ou 2      */

            m->coins_collected+= val;
            
            game_add_xp(XP_PER_COIN * val);           /* FEATURE: XP          */

            int ccx = m->coins[i].x + COIN_SIZE / 2, ccy = m->coins[i].y + COIN_SIZE / 2;
            for (int p = 0; p < COIN_PICK_PARTS; p++) {
                uint32_t c = (p & 1) ? COL_COIN_SHINE : COL_COIN;   /* gold/white mix */
                spawn_particle(m, ccx, ccy, (rand() % 11) - 5, -(2 + rand() % 6),
                               12 + rand() % 8, c);                 /* upward pop      */
            }
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

            game_add_xp(XP_PER_POWERUP);   /* FEATURE: XP por power-up */
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

                        /* FEATURE: POWER_EXTRA_LIFE */
            if (game_has_power(POWER_EXTRA_LIFE) && !m->extra_life_used) {
                m->extra_life_used = true;
                m->shield_timer = 90;   /* invulnerabilidade temporária */
                break;
            }


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

            /* FEATURE: POWER_EXTRA_LIFE */
            if (game_has_power(POWER_EXTRA_LIFE) && !m->extra_life_used) {
                m->extra_life_used = true;
                mi->active = 0;
                m->shield_timer = 90;
                continue;
            }


            player_explode(m); return;
        }
    }
}
