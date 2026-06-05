/**
 * @file view.c
 * @brief View composition: owns the current frame context (M / scr_w / scr_h),
 *        renders each game state by combining the helpers from the sibling
 *        view_*.c files, and presents the frame. Reads a const GameModel and
 *        never mutates game state.
 */
#include <lcom/lcf.h>
#include <stdio.h>                        /* snprintf */
#include "game/view.h"
#include "game/view_internal.h"
#include "game/game.h"
#include "devices/video_gr/video_gr.h"   /* clear_buffer, swap_buffers */

/* Current frame context (declared extern in view_internal.h). */
const GameModel *M = NULL;
int scr_w = 0, scr_h = 0;
int g_scroll_x = 0;   /* world scroll accumulator; advanced only while PLAYING */

/* ── Per-state screens ────────────────────────────────────────────────────── */

/** @brief Render the main menu: stars, animated player preview, title, buttons, RTC clock, best runs, XP bar and unlocked powers. */
static void render_menu(void) {
    clear_buffer(COL_MENU_BG);

    /* Decorative stars */
    if (M->visual_effects) {
        for (int i = 0; i < 50; i++) {
            int sx = (i * 97 + M->anim * (i % 3 + 1)) % scr_w;
            int sy = (i * 73) % (scr_h - 100);
            draw_rect(sx, sy, 2, 2, COL_STAR);
        }
    }

    /* Animated player preview */
    int preview_y = scr_h/4 - 30 + ((M->anim / 16) % 2 == 0 ? -8 : 8);
    draw_flame(scr_w/2 - PLAYER_W/2 + 1, preview_y + 33);
    if (sprites_ok)
        draw_sprite(player_pix, player_img,
                    scr_w/2 - PLAYER_W/2, preview_y, PLAYER_SCALE);
    else
        draw_rect(scr_w/2 - PLAYER_W/2, preview_y, PLAYER_W, PLAYER_H, COL_PLAYER);

    draw_text_sh("JETPACK JOYRIDE", scr_w/2 - 15*6*3/2, scr_h/4 + 10, 3, COL_TITLE);
    draw_text("LCOM EDITION", scr_w/2 - 12*6*2/2, scr_h/4 + 55, 2, COL_TEXT);

    /* Session best score */
    if (M->high_score > 0) {
        char hb[32];
        snprintf(hb, sizeof(hb), "SESSION BEST: %d", M->high_score);
        draw_text_c(hb, scr_w/2, scr_h/4 + 92, 2, COL_SCORE);
    }

    const char *labels[MENU_ITEMS] = {"PLAY", "2-PLAYER", "INSTRUCTIONS", "SETTINGS", "LEADERBOARD", "QUIT"};
    for (int i = 0; i < MENU_ITEMS; i++) {
        int x, y, w, h;
        menu_btn_rect(scr_w, scr_h, i, &x, &y, &w, &h);
        draw_button(x, y, w, h, labels[i], 2, M->menu_sel == i);
    }

    /* Live RTC clock + date (top-right corner) */
    if (M->clock_str[0]) {
        draw_text(M->clock_str, scr_w - 8*6 - 12, 10, 1, COL_TEXT);
        draw_text(M->date_str,  scr_w - 8*6 - 12, 24, 1, COL_STAR);
    }

    /* Persistent leaderboard (left column, loaded from disk) */
    if (M->high_count > 0) {
        int lx = 24, ly = scr_h/2 - 6;
        draw_text("BEST RUNS", lx, ly, 1, COL_TITLE);

        int limit = (M->high_count > 5) ? 5 : M->high_count;

        for (int i = 0; i < limit; i++) {
            uint32_t rc = (i == M->last_rank) ? COL_SCORE : COL_TEXT;
            char num[8], sc[12];
            int_to_str(i + 1, num, sizeof(num));
            int_to_str(M->high_scores[i].score, sc, sizeof(sc));
            int ry = ly + 16 + i * 12;
            draw_text(num, lx, ry, 1, rc);
            draw_text(sc,  lx + 18, ry, 1, rc);
            draw_text(M->high_scores[i].when, lx + 18 + 7*6, ry, 1, COL_STAR);
        }
    }

    /* XP bar bottom-left */
    
        int xp   = M->xp_total;
        int next = XP_THRESH_4;
        if      (xp < XP_THRESH_1) next = XP_THRESH_1;
        else if (xp < XP_THRESH_2) next = XP_THRESH_2;
        else if (xp < XP_THRESH_3) next = XP_THRESH_3;
        else if (xp < XP_THRESH_4) next = XP_THRESH_4;
 
        int bw = 180, bh = 10;
        int bx = 24, by = scr_h - 35;
        draw_text("XP", bx, by - 14, 1, COL_STAR);
        {
            char xpbuf[16];
            int_to_str(xp, xpbuf, sizeof(xpbuf));
            draw_text(xpbuf, bx + 18, by - 14, 1, COL_SCORE);
        }
        draw_rect(bx, by, bw, bh, COL_BTN_BG);
        {
            int fill = (xp >= next) ? bw : (xp * bw / next);
            if (fill > 0) draw_rect(bx, by, fill, bh, COL_SHIELD);
        }
        draw_rect(bx,          by,          bw, 2,  COL_OBS_TOP);
        draw_rect(bx,          by + bh - 2, bw, 2,  COL_OBS_TOP);
        draw_rect(bx,          by,          2,  bh, COL_OBS_TOP);
        draw_rect(bx + bw - 2, by,          2,  bh, COL_OBS_TOP);
    
 
    /* unlocked power icons bottom row */
    {
        int ix = 24, iy = scr_h - 75;
        if (M->powers_unlocked)
            draw_text("POWERS:", ix, iy - 14, 1, COL_STAR);
        if (M->powers_unlocked & POWER_DOUBLE_COINS) {
            draw_text("2xCOIN", ix, iy, 1, COL_COIN);  ix += 7*6 + 4;
        }
        if (M->powers_unlocked & POWER_SHIELD_START) {
            draw_text("SHLD+", ix, iy, 1, COL_SHIELD);  ix += 6*6 + 4;
        }
        if (M->powers_unlocked & POWER_SLOW_MISSILES) {
            draw_text("SLWMSL", ix, iy, 1, COL_MAGNET); ix += 7*6 + 4;
        }
        if (M->powers_unlocked & POWER_EXTRA_LIFE) {
            draw_text("XLIFE", ix, iy, 1, COL_GAMEOVER);
        }
    }

    draw_cursor();
}

/** @brief Render the 2-player lobby shown until both peers exchange HELLO + READY. */
static void render_waiting(void) {
    clear_buffer(COL_MENU_BG);

    /* Same decorative stars as the menu, for visual continuity. */
    for (int i = 0; i < 50; i++) {
        int sx = (i * 97 + M->anim * (i % 3 + 1)) % scr_w;
        int sy = (i * 73) % (scr_h - 100);
        draw_rect(sx, sy, 2, 2, COL_STAR);
    }

    /* Floating player preview (same idle bob as the title screen). */
    int preview_y = scr_h/4 - 30 + ((M->anim / 16) % 2 == 0 ? -8 : 8);
    draw_flame(scr_w/2 - PLAYER_W/2 + 1, preview_y + 33);
    if (sprites_ok)
        draw_sprite(player_pix, player_img,
                    scr_w/2 - PLAYER_W/2, preview_y, PLAYER_SCALE);
    else
        draw_rect(scr_w/2 - PLAYER_W/2, preview_y, PLAYER_W, PLAYER_H, COL_PLAYER);

    draw_text_c_sh("WAITING FOR PLAYER 2", scr_w/2, scr_h/2 - 10, 3, COL_TITLE);

    const char *dots = ((M->anim / 20) % 3 == 0) ? "."
                     : ((M->anim / 20) % 3 == 1) ? ".."
                     :                              "...";
    draw_text_c(dots, scr_w/2, scr_h/2 + 30, 3, COL_TEXT);

    if (M->peer_seen && !M->peer_ready)
        draw_text_c("PEER FOUND", scr_w/2, scr_h/2 + 70, 1, COL_P2);

    draw_text_c("PRESS ESC TO CANCEL", scr_w/2, scr_h - 60, 1, COL_STAR);

    draw_cursor();
}

/** @brief Render the instructions screen (controls and goal) with a BACK button. */
static void render_instructions(void) {
    clear_buffer(COL_MENU_BG);

    int title_s = 3;
    draw_sprite(instructions_pix, instructions_img,
                scr_w/2 - (int) instructions_img.width * title_s / 2, 40, title_s);

    const char *lines[] = {
        "GOAL: FLY AS FAR AS POSSIBLE",
        "AVOID ALL OBSTACLES AND COLLECT COINS",
        "W OR SPACE OR LEFT CLICK - FLY UP",
        "RELEASE - FALL DOWN",
        "P - PAUSE      ESC - QUIT",
        "SPEED INCREASES OVER TIME",
        "GRAB S-SHIELD AND M-MAGNET POWER-UPS",
        "2-PLAYER RACES OVER THE SERIAL PORT",
    };
    int nlines = 8;
    for (int i = 0; i < nlines; i++)
        draw_text(lines[i], scr_w/2 - 30*6/2, 140 + i * 40, 1, COL_TEXT);

    int x, y, w, h;
    back_btn_rect(scr_w, scr_h, &x, &y, &w, &h);
    draw_button(x, y, w, h, "BACK", 2, pt_in_rect(M->mouse_x, M->mouse_y, x, y, w, h));

    draw_cursor();
}

/** @brief Render the settings screen with the current difficulty, control, speed and effects options. */
static void render_settings(void) {
    clear_buffer(COL_MENU_BG);

    draw_text_sh("SETTINGS", scr_w/2 - 8*6*3/2, 40, 3, COL_TITLE);

    char diff_str[32];
    const char *diff_names[3] = {"EASY", "NORMAL", "HARD"};
    sprintf(diff_str, "DIFFICULTY: %s", diff_names[M->difficulty]);

    char ctrl_str[32];
    const char *ctrl_names[3] = {"W / SPACE", "UP / ENTER", "Z / X"};
    sprintf(ctrl_str, "CONTROL: %s", ctrl_names[M->control_mode]);

    char spd_str[32];
    sprintf(spd_str, "START SPEED: %s", M->initial_speed_fast ? "FAST" : "NORMAL");

    char eff_str[32];
    sprintf(eff_str, "EFFECTS: %s", M->visual_effects ? "ON" : "OFF");

    const char *labels[5] = {diff_str, ctrl_str, spd_str, eff_str, "BACK"};

    for (int i = 0; i < 5; i++) {
        int x, y, w, h;
        settings_btn_rect(scr_w, scr_h, i, &x, &y, &w, &h);
        draw_button(x, y, w, h, labels[i], 2, M->menu_sel == i);
    }

    draw_cursor();
}

/** @brief Render active gameplay: background, obstacles, coins, power-ups, missiles, the player and the HUD (score, coins, speed, powers, XP, P2). */
static void render_playing(void) {
    draw_parallax();                 /* sky gradient + scrolling skylines */
    draw_floor();                    /* scrolling ground for a sense of speed  */

    /* Zappers */
    for (int i = 0; i < MAX_ZAPPERS; i++)
        if (M->zaps[i].active) draw_beam(&M->zaps[i]);

    /* Coins (spin animation) */
    int cf = coin_frame();
    for (int i = 0; i < MAX_COINS; i++) {
        if (!M->coins[i].active) continue;
        if (sprites_ok) {
            draw_sprite(coin_pix[cf], coin_img[cf],
                        M->coins[i].x, M->coins[i].y, COIN_SCALE);
        } else {
            int cx = M->coins[i].x + COIN_SIZE/2;
            int cy = M->coins[i].y + COIN_SIZE/2;
            int r = COIN_SIZE/2;
            for (int dy = -r; dy <= r; dy++) {
                int half_w = r - abs(dy);
                draw_rect(cx - half_w, cy + dy, half_w * 2, 1, COL_COIN);
                if (dy > -r && dy < r)
                    draw_rect(cx - half_w + 1, cy + dy, half_w * 2 - 2, 1, COL_COIN_SHINE);
            }
        }
    }

    /* Power-ups */
    for (int i = 0; i < MAX_POWERUPS; i++)
        if (M->powerups[i].active) draw_powerup(&M->powerups[i]);

    /* Missiles + warnings */
    for (int i = 0; i < MAX_MISSILES; i++) {
        const Missile *mi = &M->missiles[i];
        if (mi->warning > 0) {
            int blink = (mi->warning / 5) % 2;
            if (blink) {
                int ay = mi->y + MISSILE_H / 2;
                for (int t = 0; t < 10; t++)               /* ◄ arrow */
                    draw_rect(scr_w - 16 + t, ay - t, 2, 2 * t + 1, COL_WARN);
                draw_text("!", scr_w - 34, mi->y, 2, COL_WARN);
            }
        }
        if (mi->active) {
            if (sprites_ok)
                draw_sprite(missile_pix, missile_img, mi->x, mi->y, MISSILE_SCALE);
            else {
                draw_rect(mi->x, mi->y, MISSILE_W, MISSILE_H, COL_GAMEOVER);
                draw_rect(mi->x + MISSILE_W - 6, mi->y - 2, 6, MISSILE_H + 4, COL_SCORE);
            }
        }
    }

    /* Opponent ghost (2-player) — drawn behind the local player */
    if (M->versus && M->opp_alive)
        draw_ghost(M->opp_y);

    /* Shield aura behind the player while invulnerable */
    if (M->shield_timer > 0)
        draw_shield_aura(PLAYER_X, M->player.y);

    /* Jetpack flame behind the player */
    if (M->player.thrusting)
        draw_flame(PLAYER_X + 1, M->player.y + 33);

    /* Player — tilts with vertical velocity */
    if (sprites_ok)
        draw_sprite_tilt(player_pix, player_img, PLAYER_X, M->player.y,
                         M->player.vy * 6);
    else
        draw_rect(PLAYER_X, M->player.y, PLAYER_W, PLAYER_H, COL_PLAYER);

    draw_particles();

    /* HUD: score */
    char sbuf[16];
    int_to_str(M->score, sbuf, sizeof(sbuf));
    draw_text("SCORE:", 10, 10, 2, COL_TEXT);
    draw_text(sbuf, 10 + 6*6*2, 10, 2, COL_SCORE);

    /* HUD: coins */
    char cbuf[16];
    int_to_str(M->coins_collected, cbuf, sizeof(cbuf));
    draw_text("x", 34, 40, 2, COL_COIN);
    draw_text(cbuf, 50, 40, 2, COL_COIN);
    if (sprites_ok) {
        int hf = coin_frame();
        draw_sprite(coin_pix[hf], coin_img[hf], 10, 36, COIN_SCALE);
    } else {
        int ci = COIN_SIZE;
        for (int dy = -ci/2; dy <= ci/2; dy++) {
            int hw = ci/2 - abs(dy);
            draw_rect(10 + ci/2 - hw, 36 + ci/2 + dy, hw*2, 1, COL_COIN);
        }
    }

    /* HUD: speed */
    char spdbuf[16];
    int_to_str(M->scroll_speed, spdbuf, sizeof(spdbuf));
    draw_text("SPD:", scr_w - 120, 10, 1, COL_TEXT);
    draw_text(spdbuf, scr_w - 80, 10, 1, COL_SCORE);

    /* HUD: active power-ups, with whole seconds remaining */
    int hud_y = 26;
    if (M->shield_timer > 0) {
        char t[8]; int_to_str((M->shield_timer + 59) / 60, t, sizeof(t));
        draw_text("SHIELD", scr_w - 120, hud_y, 1, COL_SHIELD);
        draw_text(t, scr_w - 120 + 7 * 6, hud_y, 1, COL_SHIELD);
        hud_y += 14;
    }
    if (M->magnet_timer > 0) {
        char t[8]; int_to_str((M->magnet_timer + 59) / 60, t, sizeof(t));
        draw_text("MAGNET", scr_w - 120, hud_y, 1, COL_MAGNET);
        draw_text(t, scr_w - 120 + 7 * 6, hud_y, 1, COL_MAGNET);
        hud_y += 14;
    }

    /* extra life indicator */
    if ((M->powers_unlocked & POWER_EXTRA_LIFE) && !M->extra_life_used) {
            draw_text("1UP", scr_w - 120, hud_y, 1, COL_GAMEOVER);
    }

    /* XP mini-bar — below coin counter */
    {
        int xp   = M->xp_total;
        int next = XP_THRESH_4;
        if      (xp < XP_THRESH_1) next = XP_THRESH_1;
        else if (xp < XP_THRESH_2) next = XP_THRESH_2;
        else if (xp < XP_THRESH_3) next = XP_THRESH_3;

        int bw = 80, bh = 6;
        int bx = 10, by = 58;

        draw_text("XP:", bx, by - 10, 1, COL_TITLE);

        draw_rect(bx, by, bw, bh, COL_BTN_BG);
        int fill = (xp >= next) ? bw : (xp * bw / next);
        if (fill > 0) draw_rect(bx, by, fill, bh, COL_SHIELD);
        draw_rect(bx,          by,          bw, 1, COL_OBS_TOP);
        draw_rect(bx,          by + bh - 1, bw, 1, COL_OBS_TOP);
        draw_rect(bx,          by,          1, bh, COL_OBS_TOP);
        draw_rect(bx + bw - 1, by,          1, bh, COL_OBS_TOP);
    }



    /* HUD: opponent score (2-player) */
    if (M->versus) {
        char p2[16];
        int_to_str(M->opp_score, p2, sizeof(p2));
        draw_text_c("P2", scr_w/2 - 30, 10, 1, COL_P2);
        draw_text(p2, scr_w/2 - 10, 10, 1, COL_P2);
        if (!M->opp_alive)
            draw_text_c("P2 OUT", scr_w/2, 26, 1, COL_WARN);
    }
}

/** @brief Render the game-over screen: score/rank (solo) or the 2-player result, the run timestamp and a BACK button. */
static void render_gameover(void) {
    clear_buffer(M->hit_flash > 0 ? 0x661111 : COL_MENU_BG);   /* red flash */

    draw_particles();

    draw_text_sh("GAME OVER", scr_w/2 - 9*6*4/2, scr_h/4, 4, COL_GAMEOVER);

    if (!M->versus && M->last_rank >= 0) {
        if (M->last_rank == 0)
            draw_text_c_sh("NEW HIGH SCORE!", scr_w/2, scr_h/4 + 44, 2, COL_SCORE);
        char rk[8];
        int_to_str(M->last_rank + 1, rk, sizeof(rk));
        draw_text("RANK", scr_w/2 - 18, M->last_rank == 0 ? scr_h/4 + 68 : scr_h/4 + 44, 1, COL_SCORE);
        draw_text(rk,     scr_w/2 + 14, M->last_rank == 0 ? scr_h/4 + 68 : scr_h/4 + 44, 1, COL_SCORE);
    }

    char sbuf[16];
    int_to_str(M->score, sbuf, sizeof(sbuf));
    draw_text("SCORE:", scr_w/2 - 10*6*2/2, scr_h/2 - 30, 2, COL_TEXT);
    draw_text(sbuf, scr_w/2 + 2*6*2, scr_h/2 - 30, 2, COL_SCORE);

    char cbuf[16];
    int_to_str(M->coins_collected, cbuf, sizeof(cbuf));
    draw_text("COINS:", scr_w/2 - 10*6*1/2, scr_h/2 - 2, 1, COL_TEXT);
    draw_text(cbuf, scr_w/2 + 2*6*1, scr_h/2 - 2, 1, COL_COIN);

    /* Session best score (solo) */
    if (!M->versus && M->high_score > 0) {
        char hb[16];
        int_to_str(M->high_score, hb, sizeof(hb));
        draw_text("BEST:", scr_w/2 - 10*6*1/2, scr_h/2 + 12, 1, COL_SCORE);
        draw_text(hb, scr_w/2 + 2*6*1, scr_h/2 + 12, 1, COL_SCORE);
    }

    /* 2-player result (updates live as the opponent's DEAD packet arrives) */
    if (M->versus) {
        char p2line[32];
        snprintf(p2line, sizeof(p2line), "P2 SCORE: %d", M->opp_score);
        draw_text_c(p2line, scr_w/2, scr_h/2 + 14, 1, COL_P2);

        const char *res; uint32_t rc;
        if (M->opp_alive)                  { res = "P2 STILL FLYING"; rc = COL_TEXT; }
        else if (M->score > M->opp_score)  { res = "YOU WIN";  rc = COL_SCORE; }
        else if (M->score < M->opp_score)  { res = "YOU LOSE"; rc = COL_GAMEOVER; }
        else                               { res = "DRAW";     rc = COL_TEXT; }
        draw_text_c(res, scr_w/2, scr_h/2 + 30, 2, rc);
    } else {
        draw_text("PRESS W OR CLICK TO RESTART", scr_w/2 - 27*6*1/2, scr_h/2 + 30, 1, COL_SCORE);
    }

    /* Timestamp of this run from the RTC */
    if (M->clock_str[0]) {
        draw_text(M->date_str,  scr_w/2 - 8*6/2, scr_h/2 + 52, 1, COL_TEXT);
        draw_text(M->clock_str, scr_w/2 - 8*6/2, scr_h/2 + 66, 1, COL_STAR);
    }

    int x, y, w, h;
    back_btn_rect(scr_w, scr_h, &x, &y, &w, &h);
    draw_button(x, y, w, h, "BACK", 2, pt_in_rect(M->mouse_x, M->mouse_y, x, y, w, h));

    draw_cursor();
}

/**
 * @brief Render the account-login screen (STATE_NAME_INPUT): clickable username
 *        and masked-password fields (the active one highlighted) with a blinking
 *        caret, the mouse cursor, and error/instruction feedback.
 */
static void render_name_input(void) {
    clear_buffer(COL_MENU_BG);

    int box_w = LOGIN_BOX_W, box_h = LOGIN_BOX_H;
    int box_x = scr_w/2 - box_w/2;
    int box_y = scr_h/2 - box_h/2;

    /* Login box background. */
    draw_rect(box_x, box_y, box_w, box_h, COL_MENU_BG);

    draw_text_c("ACCOUNT LOGIN", scr_w/2, box_y + 15, 2, COL_TITLE);

    /* Clickable field boxes: outline both, highlight the active one so it is
       clear the username can be re-selected with the mouse. */
    for (int i = 0; i < 2; i++) {
        int fx, fy, fw, fh;
        login_field_rect(scr_w, scr_h, i, &fx, &fy, &fw, &fh);
        bool active = (i == 0) ? !M->typing_password : M->typing_password;
        bool hover  = pt_in_rect(M->mouse_x, M->mouse_y, fx, fy, fw, fh);
        uint32_t edge = active ? COL_SCORE : (hover ? COL_BTN_HI : COL_OBS_TOP);
        draw_rect(fx,          fy,          fw, 1,  edge);
        draw_rect(fx,          fy + fh - 1, fw, 1,  edge);
        draw_rect(fx,          fy,          1,  fh, edge);
        draw_rect(fx + fw - 1, fy,          1,  fh, edge);
    }

    /* Username field. */
    draw_text("USER:", box_x + 20, box_y + 60, 1, COL_TITLE);
    draw_text(M->input_name, box_x + 80, box_y + 60, 1, COL_TEXT);
    if (!M->typing_password && (M->anim / 20) % 2 == 0) {
        draw_rect(box_x + 80 + M->input_name_len * 6, box_y + 60, 2, 8, COL_SCORE);
    }

    /* Password field (masked). */
    draw_text("PASS:", box_x + 20, box_y + 85, 1, COL_TITLE);
    char masked_pass[16];
    int p_len = M->input_password_len;
    for(int i = 0; i < p_len; i++) masked_pass[i] = '*';
    masked_pass[p_len] = '\0';
    draw_text(masked_pass, box_x + 80, box_y + 85, 1, COL_TEXT);
    if (M->typing_password && (M->anim / 20) % 2 == 0) {
        draw_rect(box_x + 80 + p_len * 6, box_y + 85, 2, 8, COL_SCORE);
    }

    /* Error feedback or next-step instruction. */
    if (M->login_error) {
        draw_text_c("WRONG PASSWORD!", scr_w/2, box_y + 115, 1, COL_TITLE);
    } else {
        if (!M->typing_password)
            draw_text_c("PRESS ENTER FOR PASSWORD", scr_w/2, box_y + 115, 1, COL_TEXT);
        else
            draw_text_c("ENTER TO LOGIN  -  CLICK USER TO EDIT", scr_w/2, box_y + 115, 1, COL_TEXT);
    }

    draw_cursor();
}
/**
 * @brief Render the leaderboard table (shared by STATE_LEADERBOARD_TOP and
 *        STATE_LEADERBOARD_ALL): rank/name/score/date rows with scrolling and a
 *        BACK button.
 */
static void render_leaderboard(void) {
    int i;
    int x, y, w, h;
    int lx = scr_w/2 - 220;
    int ly = 80;

    clear_buffer(COL_MENU_BG);

    for (i = 0; i < 50; i++) {
        int sx = (i * 97 + M->anim * (i % 2 + 1)) % scr_w;
        int sy = (i * 73) % (scr_h - 60);
        draw_rect(sx, sy, 2, 2, COL_STAR);
    }

    draw_text_c_sh("ALL SCORES", scr_w/2, 20, 3, COL_TITLE);

    draw_text("RANK", lx,       ly, 1, COL_STAR);
    draw_text("NAME", lx + 50,  ly, 1, COL_STAR);
    draw_text("SCORE",lx + 160, ly, 1, COL_STAR);
    draw_text("DATE", lx + 230, ly, 1, COL_STAR);
    draw_rect(lx, ly + 12, 440, 1, COL_OBS_TOP);

    {
        int n = M->lb_count;
        int start = M->lb_scroll;
        int max_vis = (scr_h - ly - 80) / 16;
        int end = start + max_vis;
        if (end > n) end = n;

        for (i = start; i < end; i++) {
            int row_y = ly + 20 + (i - start) * 16;
            uint32_t rc = (i == M->last_rank) ? COL_SCORE : COL_TEXT;
            char rank_s[4], score_s[12];
            int_to_str(i + 1, rank_s, sizeof(rank_s));
            int_to_str(M->lb_entries[i].score, score_s, sizeof(score_s));
            draw_text(rank_s,                lx,       row_y, 1, rc);
            draw_text(M->lb_entries[i].name, lx + 50,  row_y, 1, rc);
            draw_text(score_s,               lx + 160, row_y, 1, COL_SCORE);
            draw_text(M->lb_entries[i].when, lx + 230, row_y, 1, COL_STAR);
        }

        if (n > max_vis)
            draw_text_c("UP/DOWN TO SCROLL", scr_w/2, scr_h - 100, 1, COL_STAR);
    }

    back_btn_rect(scr_w, scr_h, &x, &y, &w, &h);
    draw_button(x, y, w, h, "BACK", 2,
                pt_in_rect(M->mouse_x, M->mouse_y, x, y, w, h));

    draw_cursor();
}


/** @brief Render the frozen play-field with a dimmed "PAUSED" overlay. */
static void render_pause(void) {
    render_playing();
    for (int y = 0; y < scr_h; y += 2)          /* venetian dim over the scene */
        draw_rect(0, y, scr_w, 1, COL_MENU_BG);
    draw_text_c_sh("PAUSED", scr_w/2, scr_h/2 - 24, 4, COL_TITLE);
    draw_text_c("PRESS P TO RESUME", scr_w/2, scr_h/2 + 24, 1, COL_TEXT);
}

/**
 * @brief Render and present one frame for model @p m.
 *
 * Sets the frame context, advances the ground scroll while playing, dispatches
 * to the per-state renderer, applies the fade-in on a state change, and swaps
 * the back buffer to VRAM. Never mutates @p m.
 *
 * @param m Game model to render (read-only).
 */
void view_render(const GameModel *m) {
    static game_state_t prev = STATE_MENU;
    static int fade = 0;

    M = m;
    scr_w = m->scr_w;
    scr_h = m->scr_h;

    if (m->state == STATE_PLAYING)
        g_scroll_x += m->scroll_speed;   /* drive the ground / speed lines */

    if (m->state != prev) fade = FADE_FRAMES;   /* fade-in on every state change */
    prev = m->state;

    switch (m->state) {
        case STATE_MENU:         render_menu();         break;
        case STATE_INSTRUCTIONS: render_instructions(); break;
        case STATE_SETTINGS:     render_settings();     break;
        case STATE_WAITING:      render_waiting();      break;
        case STATE_PLAYING:      render_playing();      break;
        case STATE_PAUSE:        render_pause();        break;
        case STATE_GAMEOVER:     render_gameover();     break;
        case STATE_NAME_INPUT:        render_name_input();            break;
        case STATE_LEADERBOARD_TOP:
        case STATE_LEADERBOARD_ALL:   render_leaderboard();           break;
        default:                                        break;
    }

    if (fade > 0) {                              /* ramp brightness 0 -> full */
        fade_buffer(255 * (FADE_FRAMES - fade) / FADE_FRAMES);
        fade--;
    }

    swap_buffers();   /* present to VRAM */
}
