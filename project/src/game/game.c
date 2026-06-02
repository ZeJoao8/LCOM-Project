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
#include "devices/video_gr/video_gr.h"   /* h_res, v_res */
#include "devices/rtc/rtc.h"

/* The whole game state lives here. */
static GameModel M;

void game_init(void) {
    model_init(&M, h_res, v_res, rtc_seed());   /* logic + RNG seeded by RTC */
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
    model_update(&M);
    net_poll(&M);    /* apply any opponent packets that arrived */
    net_tick(&M);    /* broadcast our state in 2-player mode    */
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
