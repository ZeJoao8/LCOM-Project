#ifndef _GAME_MODEL_H_
#define _GAME_MODEL_H_

/**
 * @file model.h
 * @brief Game simulation (the "M" of MVC). Pure logic: physics, spawning,
 *        collision, scoring. No drawing and no device access.
 */

#include "game/entities.h"

/** Initialise the model for a given screen size and seed the RNG. */
void model_init(GameModel *m, int scr_w, int scr_h, unsigned seed);

/** Reset all per-round state to start a fresh run. */
void model_reset(GameModel *m);

/** Advance the simulation by one timer tick. */
void model_update(GameModel *m);

/* ── World RNG (obstacle generation only — synced in 2-player mode) ───────── */
/** Seed the world RNG; called once per match with the agreed seed. */
void world_rand_seed(uint32_t s);
/** Draw one value from the world RNG (LCG, independent of rand()). */
int  world_rand(void);

/* ── Integer geometry shared with the view (read-only helpers) ────────────── */

/** Integer sine, scaled by 1024, ANG_STEPS per full turn. */
int isin(int a);
/** Integer cosine, scaled by 1024. */
int icos(int a);
/** Compute the two screen-space endpoints of a zapper beam. */
void zap_endpoints(const Zapper *z, int *ax, int *ay, int *bx, int *by);

/** Value of a single coin for this model (doubled while POWER_DOUBLE_COINS is active). */
int model_coin_value(const GameModel *m);

/**
 * @brief Adjust a missile's base speed for the active powers.
 * @param m          Game model (queried for active powers).
 * @param base_speed Nominal missile speed.
 * @return @p base_speed normally, or @p base_speed * 3 / 4 while
 *         POWER_SLOW_MISSILES is active.
 */
int model_missile_speed(const GameModel *m, int base_speed);


#endif /* _GAME_MODEL_H_ */
