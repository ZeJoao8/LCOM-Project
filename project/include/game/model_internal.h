#ifndef _GAME_MODEL_INTERNAL_H_
#define _GAME_MODEL_INTERNAL_H_

/**
 * @file model_internal.h
 * @brief Private declarations shared between the model_*.c translation units.
 *        NOT part of the public model API (game/model.h). Like the rest of the
 *        model, these touch no devices and do no drawing.
 *
 *        (isin / icos / zap_endpoints are public and live in game/model.h.)
 */

#include "game/entities.h"   /* GameModel, Zapper, PowerUp, config constants */

/* ── model_geometry.c ─────────────────────────────────────────────────────── */
/** Squared distance from point P to segment A-B (integer math). */
long pt_seg_d2(int px, int py, int ax, int ay, int bx, int by);
/** True if the player collides with the beam (sampled along the body). */
bool zap_hits_player(const GameModel *m, const Zapper *z);

/* ── model_effects.c ──────────────────────────────────────────────────────── */
void spawn_particle(GameModel *m, int x, int y, int vx, int vy,
                    int life, uint32_t col);
void spawn_popup(GameModel *m, int x, int y);
void burst(GameModel *m, int x, int y, int n, uint32_t col);
void update_particles(GameModel *m);
void player_explode(GameModel *m);

/* ── model_spawn.c ────────────────────────────────────────────────────────── */
void spawn_zapper(GameModel *m);
void spawn_powerup(GameModel *m);

#endif /* _GAME_MODEL_INTERNAL_H_ */
