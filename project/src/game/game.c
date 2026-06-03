/**
 * @file game.c
 * @brief Composition root / state-machine glue for the Jetpack-Joyride game.
 *        Owns the single GameModel instance and wires the public API used by
 *        main.c to the model (logic), view (rendering) and controller (input).
 */
#include <stdio.h>
#include "game/game.h"
#include "game/entities.h"
#include "game/model.h"
#include "game/view.h"
#include "game/controller.h"
#include "game/net.h"
#include "game/scores.h"
#include "game/settings.h"
#include "devices/video_gr/video_gr.h"   /* h_res, v_res */
#include "devices/rtc/rtc.h"

/* The whole game state lives here. */
static GameModel M;

void game_init(void) {
    model_init(&M, h_res, v_res, rtc_seed());   /* logic + RNG seeded by RTC */
    scores_load(M.high_scores, &M.high_count, MAX_SCORES);  /* persistent leaderboard */
    settings_load(&M);                          /* load persistent settings */
    view_init();                                /* load sprites (mode already set) */
    game_on_rtc();                              /* prime the clock immediately */
}

void game_on_rtc(void) {
    rtc_datetime dt;
    if (rtc_read_datetime(&dt) != 0) return;
    snprintf(M.clock_str, sizeof(M.clock_str), "%02d:%02d:%02d",
             dt.hour, dt.min, dt.sec);
    snprintf(M.date_str, sizeof(M.date_str), "%02d/%02d/%02d",
             dt.day, dt.month, dt.year);
}

void game_cleanup(void) {
    /* Everything is statically allocated — nothing to free. */
}

void game_update(void) {
    static game_state_t prev = STATE_MENU;

    model_update(&M);
    net_poll(&M);    /* apply any opponent packets that arrived */
    net_tick(&M);    /* broadcast our state in 2-player mode    */

    /* WAITING → PLAYING: both peers have HELLOed and READYed. Refresh the
     * round (model_reset clears handshake fields too, hence the restore) and
     * start playing on the same frame on both sides. */
    if (M.state == STATE_WAITING && M.peer_seen && M.peer_ready) {
        bool was_versus = M.versus;
        model_reset(&M);
        M.versus = was_versus;
        M.state = STATE_PLAYING;
    }

    /* On entering game-over from a solo run, persist the score with the RTC
     * timestamp captured for this run. */
    if (M.state == STATE_GAMEOVER && prev != STATE_GAMEOVER && !M.versus) {
        char when[SCORE_WHEN_LEN];
        snprintf(when, sizeof(when), "%s %s",
                 M.date_str[0]  ? M.date_str  : "--/--/--",
                 M.clock_str[0] ? M.clock_str : "--:--:--");
        M.last_rank = scores_insert(M.high_scores, &M.high_count,
                                    MAX_SCORES, M.score, when);
        if (M.last_rank >= 0) scores_save(M.high_scores, M.high_count);
    }
    prev = M.state;
}

void game_draw(void) {
    view_render(&M);
}

void game_handle_kbd(uint8_t scancode) {
    controller_handle_kbd(&M, scancode);
}

void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed) {
    controller_handle_mouse(&M, dx, dy, lb_pressed);
}

game_state_t game_get_state(void) {
    return M.state;
}
