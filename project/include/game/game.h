#ifndef _GAME_H_
#define _GAME_H_

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Game states ──────────────────────────────────────────────────────────── */
typedef enum {
    STATE_MENU,
    STATE_INSTRUCTIONS,
    STATE_PLAYING,
    STATE_GAMEOVER,
    STATE_EXIT
} game_state_t;

/* ── Public API called from main.c ────────────────────────────────────────── */
void game_init(void);
void game_update(void);
void game_draw(void);
void game_handle_kbd(uint8_t scancode);
void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed);
void game_cleanup(void);

game_state_t game_get_state(void);

#endif /* _GAME_H_ */
