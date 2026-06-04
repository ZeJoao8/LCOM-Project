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
#include "game/entities.h"


#ifndef XP_PATH
#define XP_PATH "/home/lcom/labs/project/data/xp.txt"
#endif

void scores_load(ScoreEntry *table, int *count, int max) {
    *count = 0;
    FILE *f = fopen(SCORES_PATH, "r");
    if (f == NULL) return;                 /* no file yet: empty board */

    char line[80];
    while (*count < max && fgets(line, sizeof(line), f) != NULL) {
                char *p1 = strchr(line, ';');
        if (p1 == NULL) continue;
        *p1 = '\0';
 
        /* Verifica se o primeiro campo é um número (formato antigo). */
        bool legacy = true;
        for (char *c = line; *c; c++) {
            if (*c < '0' || *c > '9') { legacy = false; break; }
        }
 
        ScoreEntry *e = &table[*count];
 
        if (legacy) {
            /* Formato antigo: "score;when" — usa nome genérico */
            strncpy(e->name, "???", MAX_NAME_LEN - 1);
            e->name[MAX_NAME_LEN - 1] = '\0';
            e->score = atoi(line);
            char *when = p1 + 1;
            char *nl = strchr(when, '\n');
            if (nl) *nl = '\0';
            strncpy(e->when, when, SCORE_WHEN_LEN - 1);
            e->when[SCORE_WHEN_LEN - 1] = '\0';
        } else {
            /* Novo formato: "name;score;when" */
            strncpy(e->name, line, MAX_NAME_LEN - 1);
            e->name[MAX_NAME_LEN - 1] = '\0';
 
            char *p2 = strchr(p1 + 1, ';');
            if (p2 == NULL) continue;
            *p2 = '\0';
            e->score = atoi(p1 + 1);
 
            char *when = p2 + 1;
            char *nl = strchr(when, '\n');
            if (nl) *nl = '\0';
            strncpy(e->when, when, SCORE_WHEN_LEN - 1);
            e->when[SCORE_WHEN_LEN - 1] = '\0';
        }
        (*count)++;
    }
    fclose(f);

}

void scores_save(const ScoreEntry *table, int count) {
    FILE *f = fopen(SCORES_PATH, "w");
    if (f == NULL) return;                 /* read-only FS: fail quietly */
    for (int i = 0; i < count; i++)
        fprintf(f, "%s;%d;%s\n", table[i].name, table[i].score, table[i].when);
    fclose(f);
}

int scores_insert(ScoreEntry *table, int *count, int max,
                  const char *name, int score, const char *when) {
    /* Find the first slot whose score we beat (sorted best-first). */
    int pos = *count;
    for (int i = 0; i < *count; i++) {
        if (score > table[i].score) { pos = i; break; }
    }
    if (pos >= max) {
        if (*count < max) {
            pos = *count; /* Adiciona ao fim se ainda houver espaço */
        } else {
            return -1; /* Tabela totalmente cheia na memória */
        }
    }         

    int newcount = (*count < max) ? *count + 1 : max;
    for (int i = newcount - 1; i > pos; i--)
        table[i] = table[i - 1];           /* shift the tail down one slot */


    strncpy(table[pos].name, name, MAX_NAME_LEN - 1);
    table[pos].name[MAX_NAME_LEN - 1] = '\0';    
    table[pos].score = score;
    strncpy(table[pos].when, when, SCORE_WHEN_LEN - 1);
    table[pos].when[SCORE_WHEN_LEN - 1] = '\0';
    *count = newcount;
    return pos;
}

void scores_get_top(const ScoreEntry *table, int count, int n, ScoreEntry *out, int *out_count) {
    *out_count = (count < n) ? count : n;
    for (int i = 0; i < *out_count; i++) out[i] = table[i];   
}

void scores_get_all(const ScoreEntry *table, int count, ScoreEntry *out, int *out_count) {
    *out_count = count;
    for (int i = 0; i < count; i++) out[i] = table[i];
}

int scores_load_xp(void) {
    FILE *f = fopen(XP_PATH, "r");
    if (f == NULL) return 0;
    int xp = 0;
    if (fscanf(f, "%d", &xp) != 1) xp = 0;
    fclose(f);
    return xp;
}
 
void scores_save_xp(int xp) {
    FILE *f = fopen(XP_PATH, "w");
    if (f == NULL) return;
    fprintf(f, "%d\n", xp);
    fclose(f);
}

