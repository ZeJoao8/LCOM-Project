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
/** Spawn one particle with the given position, velocity, lifetime and colour. */
void spawn_particle(GameModel *m, int x, int y, int vx, int vy,
                    int life, uint32_t col);
/** Spawn a "+score" popup at (x,y). */
void spawn_popup(GameModel *m, int x, int y);
/** Emit a burst of @p n particles of colour @p col from (x,y). */
void burst(GameModel *m, int x, int y, int n, uint32_t col);
/** Advance and age all active particles and popups by one frame. */
void update_particles(GameModel *m);
/** Trigger the player death explosion (multi-colour particle burst). */
void player_explode(GameModel *m);

/* ── model_spawn.c ────────────────────────────────────────────────────────── */
/** Spawn a new zapper (and any coins/missiles tied to it) at the right edge. */
void spawn_zapper(GameModel *m);
/** Spawn a floating power-up pickup at the right edge. */
void spawn_powerup(GameModel *m);

#endif /* _GAME_MODEL_INTERNAL_H_ */
