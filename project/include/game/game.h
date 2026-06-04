#ifndef _GAME_H_
#define _GAME_H_

#include <stdint.h>
#include <stdbool.h>

/* ── Game states ──────────────────────────────────────────────────────────── */
typedef enum {
    STATE_MENU,
    STATE_INSTRUCTIONS,
    STATE_SETTINGS,
    STATE_WAITING,          /* 2-player lobby: handshake with the peer over COM1 */
    STATE_PLAYING,
    STATE_PAUSE,
    STATE_GAMEOVER,
    STATE_LEADERBOARD_TOP,   /* Leaderboard — mostra top 5          */
    STATE_LEADERBOARD_ALL,   /* Leaderboard — mostra todos os scores */
    STATE_NAME_INPUT,        /* Leaderboard — pede nome ao jogador   */
    STATE_EXIT
} game_state_t;



/* ── Public API called from main.c ────────────────────────────────────────── */
void game_init(void);
void game_update(void);
void game_draw(void);
void game_handle_kbd(uint8_t scancode);
void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed);
void game_on_rtc(void);   /* refresh the on-screen clock (called on RTC IRQ) */
void game_cleanup(void);

game_state_t game_get_state(void);

/* ── XP API ── */
/* Adiciona XP ao total acumulado; verifica limiares e desbloqueia poderes */
void game_add_xp(int amount);

/* Devolve o XP total acumulado nesta run */
int  game_get_xp(void);

/* ── Powers API ── */
/* Desbloqueia uma flag de poder (usando os bits definidos no config.h) */
void game_unlock_power(uint32_t power_flag);

/* Verifica se um poder específico está ativo */
bool game_has_power(uint32_t power_flag);

int game_authenticate_user(const char *name, const char *password);

#endif /* _GAME_H_ */
