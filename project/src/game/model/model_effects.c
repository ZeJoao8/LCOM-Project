/**
 * @file model_effects.c
 * @brief Visual "juice" that lives in the model as pure state: particles, score
 *        popups and the player's death explosion. Drawing happens in the view.
 */
#include <stdlib.h>            /* rand */
#include "game/model.h"
#include "game/model_internal.h"

void spawn_particle(GameModel *m, int x, int y, int vx, int vy,
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

void spawn_popup(GameModel *m, int x, int y) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!m->popups[i].active) {
            m->popups[i].x = x; m->popups[i].y = y;
            m->popups[i].life = POPUP_LIFE; m->popups[i].active = 1;
            return;
        }
    }
}

void burst(GameModel *m, int x, int y, int n, uint32_t col) {
    for (int i = 0; i < n; i++)
        spawn_particle(m, x, y, (rand() % 13) - 6, (rand() % 13) - 8,
                       14 + (rand() % 12), col);
}

void update_particles(GameModel *m) {
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
void player_explode(GameModel *m) {
    int cx = PLAYER_X + PLAYER_W / 2, cy = m->player.y + PLAYER_H / 2;
    burst(m, cx, cy, EXPLODE_BURST_RED,  COL_GAMEOVER);
    burst(m, cx, cy, EXPLODE_BURST_GOLD, COL_SCORE);
    burst(m, cx, cy, EXPLODE_BURST_FIRE, 0xFF8800);
    m->hit_flash = 8;
    if (m->score > m->high_score) m->high_score = m->score;
    m->state = STATE_GAMEOVER;
}
