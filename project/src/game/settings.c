/**
 * @file settings.c
 * @brief Persistent settings implementation. Saves and loads game difficulty,
 *        control mode, speed settings, and visual effects configurations from disk.
 */
#include <stdio.h>
#include "game/settings.h"

void settings_load(GameModel *m) {
    FILE *f = fopen(SETTINGS_PATH, "r");
    if (f == NULL) return; /* File doesn't exist: keep defaults initialized by model_init */
    int diff, ctrl, eff, fast;
    if (fscanf(f, "%d\n%d\n%d\n%d\n", &diff, &ctrl, &eff, &fast) == 4) {
        if (diff >= 0 && diff <= 2) m->difficulty = diff;
        if (ctrl >= 0 && ctrl <= 2) m->control_mode = ctrl;
        m->visual_effects = (eff != 0);
        m->initial_speed_fast = (fast != 0);
    }
    fclose(f);
}

void settings_save(const GameModel *m) {
    FILE *f = fopen(SETTINGS_PATH, "w");
    if (f == NULL) return;
    fprintf(f, "%d\n%d\n%d\n%d\n",
            m->difficulty,
            m->control_mode,
            m->visual_effects ? 1 : 0,
            m->initial_speed_fast ? 1 : 0);
    fclose(f);
}
