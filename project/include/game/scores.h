#ifndef _GAME_SCORES_H_
#define _GAME_SCORES_H_

/**
 * @file scores.h
 * @brief Persistent high-score table. Loads/saves a small sorted leaderboard
 *        from disk and inserts new RTC-timestamped runs. Pure file I/O and
 *        logic — no device access — so the model stays clean and the glue
 *        layer (game.c) owns persistence.
 */


#include "game/entities.h"

/** Load up to @p max entries (already sorted, best first) from SCORES_PATH.
 *  Sets *count to how many were read; a missing file yields *count = 0. */
void scores_load(ScoreEntry *table, int *count, int max);

/** Overwrite SCORES_PATH with the first @p count entries of @p table. */
void scores_save(const ScoreEntry *table, int count);

/** Insert ( @p name, @p score, @p when) keeping @p table sorted best-first and capped at
 *  @p max. Returns the 0-based rank it landed on, or -1 if it did not qualify. */
int scores_insert(ScoreEntry *table, int *count, int max,
                  const char *name, int score, const char *when);


/* ── FEATURE: Leaderboard — top N e todos ────────────────────────────────── */
 
/**
 * Copia as melhores @p n entradas de @p table para @p out.
 * Define *out_count com quantas foram copiadas (pode ser < n se table tiver
 * menos de n entradas).
 */
void scores_get_top(const ScoreEntry *table, int count,
                    int n,
                    ScoreEntry *out, int *out_count);
 
/**
 * Copia todas as entradas de @p table para @p out (já ordenadas).
 * Define *out_count com o número total copiado.
 */
void scores_get_all(const ScoreEntry *table, int count,
                    ScoreEntry *out, int *out_count);
 
/* ── FEATURE: XP persistente ─────────────────────────────────────────────── */
 
/** Lê o XP guardado no ficheiro XP_PATH (0 se o ficheiro não existir). */
int  scores_load_xp(void);
 
/** Grava @p xp no ficheiro XP_PATH. */
void scores_save_xp(int xp);


#endif /* _GAME_SCORES_H_ */
