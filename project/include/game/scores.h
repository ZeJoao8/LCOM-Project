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


/* ── Leaderboard — top N and all ──────────────────────────────────────────── */

/**
 * @brief Copy the best @p n entries of @p table into @p out.
 * @param table     Source leaderboard (sorted best-first).
 * @param count     Number of valid entries in @p table.
 * @param n         Maximum number of entries to copy.
 * @param out       Destination array.
 * @param out_count Out: number actually copied (< @p n if @p table has fewer).
 */
void scores_get_top(const ScoreEntry *table, int count,
                    int n,
                    ScoreEntry *out, int *out_count);

/**
 * @brief Copy every entry of @p table into @p out (already sorted).
 * @param table     Source leaderboard.
 * @param count     Number of valid entries in @p table.
 * @param out       Destination array.
 * @param out_count Out: total number copied.
 */
void scores_get_all(const ScoreEntry *table, int count,
                    ScoreEntry *out, int *out_count);


#endif /* _GAME_SCORES_H_ */
