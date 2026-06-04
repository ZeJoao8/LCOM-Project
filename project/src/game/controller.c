/**
 * @file controller.c
 * @brief Input handling: maps keyboard scancodes and mouse packets onto the
 *        model. Button geometry is shared with the view through config.h.
 */

#include <string.h>
#include "game/controller.h"
#include "game/model.h"  /* model_reset */
#include "game/scores.h"
#include "game/game.h"
   

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
#define BACK_MAKE   0x0E

/* Scancode -> ASCII (uppercase) for name input.
 * Returns '\0' if the key has no printable mapping we want. */
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



/* Start a fresh run, solo or serial 2-player. */
static void start_game(GameModel *m, bool versus) {
    model_reset(m);
    m->versus = versus;     /* set after reset, which clears the round state */
    m->state = STATE_PLAYING;
}


/* Load leaderboard data e vai para o top-5 view atualizado do disco */
static void open_leaderboard_top(GameModel *m) {
    /* Força a leitura do ficheiro real guardado antes de mostrar no ecrã */
    scores_load(m->high_scores, &m->high_count, 5);
    
    scores_get_top(m->high_scores, m->high_count, 5,
                   m->lb_entries, &m->lb_count);
    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_TOP;
}
 
/* Load all scores e vai para o all-scores view atualizado do disco */
static void open_leaderboard_all(GameModel *m) {
    /* Força a leitura do ficheiro real guardado antes de mostrar no ecrã */
    scores_load(m->high_scores, &m->high_count, MAX_SCORES);
    
    scores_get_all(m->high_scores, m->high_count,
                   m->lb_entries, &m->lb_count);
    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_ALL;
}


/* Run the action behind a top-level menu entry. */
static void menu_activate(GameModel *m, int idx) {
    switch (idx) {
        case 0: start_game(m, false);            break;  /* PLAY (solo)      */
        case 1: start_game(m, true);             break;  /* 2-PLAYER (serial)*/
        case 2: m->state = STATE_INSTRUCTIONS;   break;
        case 3: m->state = STATE_EXIT;           break;
        case 4: open_leaderboard_top(m);         break;   /* LEADERBOARD       */
        default:                                 break;
    }
}

void controller_handle_kbd(GameModel *m, uint8_t scancode) {
    /* ESC returns to main menu from any screen; if already on menu, it exits the game. */
    if (scancode == ESC_MAKE) {
        if (m->state == STATE_MENU) {
            m->state = STATE_EXIT;
        }else
            m->state = STATE_MENU;

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
            if (scancode == ESC_MAKE || scancode == ENTER_MAKE) {
                m->state = STATE_MENU;
            }
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
            /* Se carregar em ESC ou ENTER, volta para o Menu Principal */
            if (scancode == ESC_MAKE || scancode == ENTER_MAKE) {
                model_reset(m); 
                m->state = STATE_MENU; 
            }
            /* SE CARREGAR NO W OU NO ESPAÇO, RECOMEÇA O JOGO IMEDIATAMENTE! */
            else if (scancode == W_MAKE || scancode == SPACE_MAKE) {
                start_game(m, false); /* Inicia uma nova partida a solo */
            }
            break;
 
        /*name input */
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
                        /* Configura os teus poderes baseados no XP carregado */
                        m->powers_unlocked = 0;
                        if (m->xp_total >= XP_THRESH_1) m->powers_unlocked |= POWER_DOUBLE_COINS;
                        if (m->xp_total >= XP_THRESH_2) m->powers_unlocked |= POWER_SHIELD_START;
                        if (m->xp_total >= XP_THRESH_3) m->powers_unlocked |= POWER_SLOW_MISSILES;
                        if (m->xp_total >= XP_THRESH_4) m->powers_unlocked |= POWER_EXTRA_LIFE;
                        
                        m->login_error = false;

                        scores_load(m->high_scores, &m->high_count, MAX_SCORES);

                        m->state = STATE_MENU; /* SALTA PARA O MENU E O TECLADO VOLTA À VIDA! */
                    }
                }
            } 
            else if (scancode == BACK_MAKE) {
                m->login_error = false;
                if (m->typing_password) {
                    if (m->input_password_len > 0) m->input_password[--m->input_password_len] = '\0';
                } else {
                    if (m->input_name_len > 0) m->input_name[--m->input_name_len] = '\0';
                }
            } 
            else {
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
 
        /* leaderboard navigation */
        case STATE_LEADERBOARD_TOP:
            if (scancode == ENTER_MAKE || scancode == SPACE_MAKE)
                open_leaderboard_all(m);
            break;
 
        case STATE_LEADERBOARD_ALL:
            if (scancode == UP_MAKE && m->lb_scroll > 0)
                m->lb_scroll--;
            else if (scancode == DOWN_MAKE && m->lb_scroll < m->lb_count - 1)
                m->lb_scroll++;
            else if (scancode == ENTER_MAKE || scancode == SPACE_MAKE)
                open_leaderboard_top(m);
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
    }else if (m->state == STATE_LEADERBOARD_TOP || m->state == STATE_LEADERBOARD_ALL) {
        int x, y, w, h;
        
        /* Botão BACK */
        back_btn_rect(m->scr_w, m->scr_h, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
            m->state = STATE_MENU;
            return;
        }
        
        /* Botão TOGGLE (Show All / Show Top 5) */
        int bx = m->scr_w/2 - 100, by = m->scr_h - 78, bw = 200, bh = 24;
        if (pt_in_rect(m->mouse_x, m->mouse_y, bx, by, bw, bh)) {
            if (m->state == STATE_LEADERBOARD_TOP)
                open_leaderboard_all(m);
            else
                open_leaderboard_top(m);
        }
    }else if (m->state == STATE_GAMEOVER) {
        int x, y, w, h;
        /* Reutiliza o mesmo botão BACK para regressar ao menu */
        back_btn_rect(m->scr_w, m->scr_h, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
            model_reset(m);    /* Limpa a partida anterior */
            m->state = STATE_MENU;
            return;
        }
    }else if (m->state == STATE_INSTRUCTIONS) {
        int x, y, w, h;
        /* Reutiliza o botão BACK geométrico padrão do teu jogo */
        back_btn_rect(m->scr_w, m->scr_h, &x, &y, &w, &h);
        if (pt_in_rect(m->mouse_x, m->mouse_y, x, y, w, h)) {
            m->state = STATE_MENU; /* Volta para o menu principal */
            return;
        }
    }
}
