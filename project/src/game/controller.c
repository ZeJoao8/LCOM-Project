/**
 * @file controller.c
 * @brief Input handling: maps keyboard scancodes and mouse packets onto the
 *        model. Button geometry is shared with the view through config.h.
 */
#include "game/controller.h"
#include "game/model.h"      /* model_reset */
#include "game/net.h"        /* net_send_bye, net_reset_parser */
#include "devices/serial/serial.h"  /* serial_rx_drain */

/* Scancodes */
#define ESC_MAKE    0x01
#define ESC_BREAK   0x81
#define W_MAKE      0x11
#define W_BREAK     0x91
#define SPACE_MAKE  0x39
#define SPACE_BREAK 0xB9
#define ENTER_MAKE  0x1C
#define UP_MAKE     0x48
#define DOWN_MAKE   0x50
#define P_MAKE      0x19

/* Start a fresh run, solo or serial 2-player. Solo jumps straight into play;
 * 2-player enters a lobby/waiting state and only transitions to PLAYING once
 * both peers have exchanged HELLO + READY. */
static void start_game(GameModel *m, bool versus) {
    model_reset(m);
    m->versus = versus;     /* set after reset, which clears the round state */
    if (versus) {
        m->peer_seen  = false;
        m->peer_ready = false;
        m->wait_ctr   = 0;
        serial_rx_drain();      /* discard any stale bytes from a previous run */
        net_reset_parser();     /* clear the frame parser's sliding window     */
        m->state = STATE_WAITING;
    } else {
        m->state = STATE_PLAYING;
    }
}

/* Run the action behind a top-level menu entry. */
static void menu_activate(GameModel *m, int idx) {
    switch (idx) {
        case 0: start_game(m, false);            break;  /* PLAY (solo)      */
        case 1: start_game(m, true);             break;  /* 2-PLAYER (serial)*/
        case 2: m->state = STATE_INSTRUCTIONS;   break;
        case 3: m->state = STATE_EXIT;           break;
        default:                                 break;
    }
}

void controller_handle_kbd(GameModel *m, uint8_t scancode) {
    /* ESC returns to main menu from any screen; if already on menu, it exits the game. */
    if (scancode == ESC_MAKE) {
        if (m->state == STATE_MENU) {
            m->state = STATE_EXIT;
        } else {
            if (m->state == STATE_WAITING && m->versus)
                net_send_bye();      /* tell the peer we cancelled the lobby */
            m->state = STATE_MENU;
        }
        return;
    }
    if (scancode == ESC_BREAK) {
        return;
    }

    switch (m->state) {
        case STATE_MENU:
            if (scancode == UP_MAKE)
                m->menu_sel = (m->menu_sel - 1 + MENU_ITEMS) % MENU_ITEMS;
            else if (scancode == DOWN_MAKE)
                m->menu_sel = (m->menu_sel + 1) % MENU_ITEMS;
            else if (scancode == ENTER_MAKE || scancode == SPACE_MAKE)
                menu_activate(m, m->menu_sel);
            break;

        case STATE_INSTRUCTIONS:
            /* Only the BACK button (mouse) returns to the menu. */
            break;

        case STATE_PLAYING:
            if (scancode == P_MAKE && !m->versus) {   /* pause (solo only) */
                m->player.thrusting = false;
                m->state = STATE_PAUSE;
            } else if (scancode == W_MAKE || scancode == SPACE_MAKE)
                m->player.thrusting = true;
            else if (scancode == W_BREAK || scancode == SPACE_BREAK)
                m->player.thrusting = false;
            break;

        case STATE_PAUSE:
            if (scancode == P_MAKE)
                m->state = STATE_PLAYING;
            break;

        case STATE_GAMEOVER:
            if (scancode == W_MAKE || scancode == SPACE_MAKE || scancode == ENTER_MAKE) {
                model_reset(m);
                m->state = STATE_PLAYING;
            }
            break;

        default:
            break;
    }
}

void controller_handle_mouse(GameModel *m, int16_t dx, int16_t dy, bool lb_pressed) {
    /* Update cursor, clamped to the screen. Y is inverted (up = negative dy). */
    m->mouse_x += dx;
    m->mouse_y -= dy;
    if (m->mouse_x < 0)            m->mouse_x = 0;
    if (m->mouse_x >= m->scr_w)    m->mouse_x = m->scr_w - 1;
    if (m->mouse_y < 0)            m->mouse_y = 0;
    if (m->mouse_y >= m->scr_h)    m->mouse_y = m->scr_h - 1;

    /* During gameplay the left button is the thrust (hold = fly). */
    if (m->state == STATE_PLAYING)
        m->player.thrusting = lb_pressed;

    /* Sync keyboard selection with mouse hover on the menu. */
    if (m->state == STATE_MENU) {
        for (int i = 0; i < MENU_ITEMS; i++) {
            int x, y, w, h;
            menu_btn_rect(m->scr_w, m->scr_h, i, &x, &y, &w, &h);
            if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) { m->menu_sel = i; break; }
        }
    }

    /* Edge detection: act only on the press, not the hold. */
    bool clicked = lb_pressed && !m->lb_prev;
    m->lb_prev = lb_pressed;
    if (!clicked) return;

    if (m->state == STATE_MENU) {
        for (int i = 0; i < MENU_ITEMS; i++) {
            int x, y, w, h;
            menu_btn_rect(m->scr_w, m->scr_h, i, &x, &y, &w, &h);
            if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
                menu_activate(m, i);
                return;
            }
        }
    }
    else if (m->state == STATE_INSTRUCTIONS) {
        int x, y, w, h;
        back_btn_rect(m->scr_w, m->scr_h, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h))
            m->state = STATE_MENU;
    }
    else if (m->state == STATE_GAMEOVER) {
        int x, y, w, h;
        back_btn_rect(m->scr_w, m->scr_h, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
            m->state = STATE_MENU;
        } else {
            model_reset(m);        /* click anywhere else restarts */
            m->state = STATE_PLAYING;
        }
    }
}
