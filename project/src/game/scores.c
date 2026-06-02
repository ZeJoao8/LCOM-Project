/**
 * @file scores.c
 * @brief Persistent leaderboard backed by a tiny text file. Each line is
 *        "score;DD/MM/YY HH:MM". The table is always kept sorted best-first so
 *        loading never has to re-sort. No device access — only stdio.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/scores.h"

void scores_load(ScoreEntry *table, int *count, int max) {
    *count = 0;
    FILE *f = fopen(SCORES_PATH, "r");
    if (f == NULL) return;                 /* no file yet: empty board */

    char line[64];
    while (*count < max && fgets(line, sizeof(line), f) != NULL) {
        char *sep = strchr(line, ';');
        if (sep == NULL) continue;         /* skip malformed lines */
        *sep = '\0';

        char *when = sep + 1;
        char *nl = strchr(when, '\n');
        if (nl) *nl = '\0';

        table[*count].score = atoi(line);
        strncpy(table[*count].when, when, SCORE_WHEN_LEN - 1);
        table[*count].when[SCORE_WHEN_LEN - 1] = '\0';
        (*count)++;
    }
    fclose(f);
}

void scores_save(const ScoreEntry *table, int count) {
    FILE *f = fopen(SCORES_PATH, "w");
    if (f == NULL) return;                 /* read-only FS: fail quietly */
    for (int i = 0; i < count; i++)
        fprintf(f, "%d;%s\n", table[i].score, table[i].when);
    fclose(f);
}

int scores_insert(ScoreEntry *table, int *count, int max,
                  int score, const char *when) {
    /* Find the first slot whose score we beat (sorted best-first). */
    int pos = *count;
    for (int i = 0; i < *count; i++) {
        if (score > table[i].score) { pos = i; break; }
    }
    if (pos >= max) return -1;             /* board full and we're worse */

    int newcount = (*count < max) ? *count + 1 : max;
    for (int i = newcount - 1; i > pos; i--)
        table[i] = table[i - 1];           /* shift the tail down one slot */

    table[pos].score = score;
    strncpy(table[pos].when, when, SCORE_WHEN_LEN - 1);
    table[pos].when[SCORE_WHEN_LEN - 1] = '\0';
    *count = newcount;
    return pos;
}
