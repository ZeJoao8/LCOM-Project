/**
 * @file controller.c
 * @brief Input handling: maps keyboard scancodes and mouse packets onto the
 *        model. Button geometry is shared with the view through config.h.
 */

#include <string.h>
#include "game/controller.h"
#include "game/model.h"
#include "game/scores.h"
#include "game/game.h"
#include "game/net.h"
#include "game/settings.h"
#include "devices/serial/serial.h"

/* Scancodes */
#define ESC_MAKE    0x01
#define ESC_BREAK   0x81
#define W_MAKE      0x11
#define W_BREAK     0x91
#define SPACE_MAKE  0x39
#define SPACE_BREAK 0xB9
#define ENTER_MAKE  0x1C
#define ENTER_BREAK 0x9C
#define UP_MAKE     0x48
#define UP_BREAK    0xC8
#define DOWN_MAKE   0x50
#define P_MAKE      0x19
#define BACK_MAKE   0x0E

/**
 * @brief Map a make scancode to an uppercase ASCII character for name input.
 * @param sc Scancode (make code).
 * @return The mapped character, or '\0' if the key has no printable mapping.
 */
static char scancode_to_char(uint8_t sc) {
    /* Row 1: numbers */
    static const char row1[] = "1234567890";
    if (sc >= 0x02 && sc <= 0x0B) return row1[sc - 0x02];
    /* Row 2: QWERTY */
    static const char row2[] = "QWERTYUIOP";
    if (sc >= 0x10 && sc <= 0x19) return row2[sc - 0x10];
    /* Row 3: ASDFGH */
    static const char row3[] = "ASDFGHJKL";
    if (sc >= 0x1E && sc <= 0x26) return row3[sc - 0x1E];
    /* Row 4: ZXCVBN */
    static const char row4[] = "ZXCVBNM";
    if (sc >= 0x2C && sc <= 0x32) return row4[sc - 0x2C];
    /* Space */
    if (sc == 0x39) return ' ';
    return '\0';
}



/**
 * @brief Start a fresh run, solo or serial 2-player.
 *
 * Solo jumps straight into play; 2-player enters the WAITING lobby and only
 * transitions to PLAYING once both peers have exchanged HELLO + READY.
 *
 * @param m      Game model.
 * @param versus true for a 2-player serial race, false for solo.
 */
static void start_game(GameModel *m, bool versus) {
    model_reset(m);
    m->versus = versus;     /* set after reset, which clears the round state */
    if (versus) {
        m->my_seed    = (uint16_t)m->anim;
        m->peer_seed  = 0;
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


/**
 * @brief Reload the leaderboard from disk and open the all-scores view.
 * @param m Game model.
 */
static void open_leaderboard_all(GameModel *m) {
    /* Reload the on-disk file before showing it. */
    scores_load(m->high_scores, &m->high_count, MAX_SCORES);

    scores_get_all(m->high_scores, m->high_count,
                   m->lb_entries, &m->lb_count);
    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_ALL;
}


/**
 * @brief Apply the action of settings entry @p idx (toggle/cycle an option or
 *        go back), saving to disk when an option changed.
 * @param m   Game model.
 * @param idx Settings entry index.
 */
static void settings_activate(GameModel *m, int idx) {
    switch (idx) {
        case 0: m->difficulty = (m->difficulty + 1) % 3;             break;
        case 1: m->control_mode = (m->control_mode + 1) % 3;         break;
        case 2: m->initial_speed_fast = !m->initial_speed_fast;       break;
        case 3: m->visual_effects = !m->visual_effects;               break;
        case 4: m->state = STATE_MENU;            m->menu_sel = 3;   break; /* back to Settings btn */
        default:                                                     break;
    }
    if (idx >= 0 && idx <= 3) {
        settings_save(m);
    }
}

/**
 * @brief Apply the action of main-menu entry @p idx (play, 2-player,
 *        instructions, settings, leaderboard, or exit).
 * @param m   Game model.
 * @param idx Menu entry index (0..MENU_ITEMS-1).
 */
static void menu_activate(GameModel *m, int idx) {
    switch (idx) {
        case 0: start_game(m, false);            break;  /* PLAY (solo)      */
        case 1: start_game(m, true);             break;  /* 2-PLAYER (serial)*/
        case 2: m->state = STATE_INSTRUCTIONS;   break;
        case 3: m->state = STATE_SETTINGS;       m->menu_sel = 0; break;
        case 4: open_leaderboard_all(m);          break;
        case 5: m->state = STATE_EXIT;           break;
        default:                                 break;
    }
}

/**
 * @brief Handle one keyboard scancode according to the current game state:
 *        menu navigation, thrust during play, pause, name/password entry, and
 *        leaderboard scrolling. ESC backs out to the menu (or exits from it).
 * @param m        Game model.
 * @param scancode Make or break code from the keyboard handler.
 */
void controller_handle_kbd(GameModel *m, uint8_t scancode) {
    /* ESC returns to main menu from any screen; if already on menu, it exits the game. */
    if (scancode == ESC_MAKE) {
        if (m->state == STATE_MENU) {
            m->state = STATE_EXIT;
        } else {
            if (m->state == STATE_SETTINGS) {
                m->menu_sel = 3;
            }
            if (m->state == STATE_WAITING && m->versus) {
                net_send_bye();
            }
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
            if (scancode == ENTER_MAKE)
                m->state = STATE_MENU;
            break;

        case STATE_SETTINGS:
            if (scancode == UP_MAKE)
                m->menu_sel = (m->menu_sel - 1 + 5) % 5;
            else if (scancode == DOWN_MAKE)
                m->menu_sel = (m->menu_sel + 1) % 5;
            else if (scancode == ENTER_MAKE || scancode == SPACE_MAKE)
                settings_activate(m, m->menu_sel);
            break;

        case STATE_PLAYING: {
            bool is_make  = (scancode == W_MAKE  || scancode == SPACE_MAKE);
            bool is_break = (scancode == W_BREAK || scancode == SPACE_BREAK);
            if (m->control_mode == 1) {
                is_make  |= (scancode == UP_MAKE    || scancode == ENTER_MAKE);
                is_break |= (scancode == UP_BREAK   || scancode == ENTER_BREAK);
            } else if (m->control_mode == 2) {
                is_make  |= (scancode == 0x2C || scancode == 0x2D);
                is_break |= (scancode == 0xAC || scancode == 0xAD);
            }

            if (scancode == P_MAKE && !m->versus) {   /* pause (solo only) */
                m->player.thrusting = false;
                m->state = STATE_PAUSE;
            } else if (is_make)
                m->player.thrusting = true;
            else if (is_break)
                m->player.thrusting = false;
            break;
        }

        case STATE_PAUSE:
            if (scancode == P_MAKE)
                m->state = STATE_PLAYING;
            break;

        case STATE_GAMEOVER: {
            bool is_action = false;
            if (m->control_mode == 0)      is_action = (scancode == W_MAKE || scancode == SPACE_MAKE);
            else if (m->control_mode == 1) is_action = (scancode == UP_MAKE || scancode == ENTER_MAKE);
            else if (m->control_mode == 2) is_action = (scancode == 0x2C || scancode == 0x2D);
            if (is_action || scancode == ENTER_MAKE)
                start_game(m, m->versus);
            break;
        }

        case STATE_NAME_INPUT:
            if (scancode == ENTER_MAKE) {
                if (!m->typing_password) {
                    if (m->input_name_len > 0) m->typing_password = true;
                } else {
                    int res_xp = game_authenticate_user(m->input_name, m->input_password);
                    if (res_xp == -1) {
                        m->login_error = true;
                        memset(m->input_password, 0, sizeof(m->input_password));
                        m->input_password_len = 0;
                    } else {
                        m->xp_total = res_xp;
                        m->powers_unlocked = 0;
                        if (m->xp_total >= XP_THRESH_1) m->powers_unlocked |= POWER_DOUBLE_COINS;
                        if (m->xp_total >= XP_THRESH_2) m->powers_unlocked |= POWER_SHIELD_START;
                        if (m->xp_total >= XP_THRESH_3) m->powers_unlocked |= POWER_SLOW_MISSILES;
                        if (m->xp_total >= XP_THRESH_4) m->powers_unlocked |= POWER_EXTRA_LIFE;
                        m->login_error = false;
                        scores_load(m->high_scores, &m->high_count, MAX_SCORES);
                        m->state = STATE_MENU;
                    }
                }
            } else if (scancode == BACK_MAKE) {
                m->login_error = false;
                if (m->typing_password) {
                    if (m->input_password_len > 0) m->input_password[--m->input_password_len] = '\0';
                } else {
                    if (m->input_name_len > 0) m->input_name[--m->input_name_len] = '\0';
                }
            } else {
                char c = scancode_to_char(scancode);
                if (c != '\0') {
                    m->login_error = false;
                    if (m->typing_password) {
                        if (m->input_password_len < 15) {
                            m->input_password[m->input_password_len++] = c;
                            m->input_password[m->input_password_len] = '\0';
                        }
                    } else {
                        if (m->input_name_len < 14) {
                            m->input_name[m->input_name_len++] = c;
                            m->input_name[m->input_name_len] = '\0';
                        }
                    }
                }
            }
            break;
 
        case STATE_LEADERBOARD_ALL:
            if (scancode == UP_MAKE && m->lb_scroll > 0)
                m->lb_scroll--;
            else if (scancode == DOWN_MAKE && m->lb_scroll < m->lb_count - 1)
                m->lb_scroll++;
            break;
 
        default:
            break;

    }
}

/**
 * @brief Handle one mouse packet: move the clamped cursor, use the left button
 *        as thrust while playing, sync menu hover with the pointer, and trigger
 *        button actions on the click edge.
 * @param m          Game model.
 * @param dx,dy      Relative motion (dy positive is up, so cursor Y is inverted).
 * @param lb_pressed Current left-button state.
 */
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
    else if (m->state == STATE_SETTINGS) {
        for (int i = 0; i < 5; i++) {
            int x, y, w, h;
            settings_btn_rect(m->scr_w, m->scr_h, i, &x, &y, &w, &h);
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
    else if (m->state == STATE_SETTINGS) {
        for (int i = 0; i < 5; i++) {
            int x, y, w, h;
            settings_btn_rect(m->scr_w, m->scr_h, i, &x, &y, &w, &h);
            if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
                settings_activate(m, i);
                return;
            }
        }
    }
    else if (m->state == STATE_LEADERBOARD_ALL) {
        int x, y, w, h;
        back_btn_rect(m->scr_w, m->scr_h, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h))
            m->state = STATE_MENU;
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
            m->menu_sel = 3;
        } else {
            start_game(m, m->versus);
        }
    }
    else if (m->state == STATE_NAME_INPUT) {
        int x, y, w, h;
        /* Click the USER field to go back and edit the username. */
        login_field_rect(m->scr_w, m->scr_h, 0, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
            m->typing_password = false;
            m->login_error = false;
            return;
        }
        /* Click the PASS field to move on to the password (needs a username). */
        login_field_rect(m->scr_w, m->scr_h, 1, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
            if (m->input_name_len > 0) {
                m->typing_password = true;
                m->login_error = false;
            }
            return;
        }
    }
}
