/**
 * @file game.c
 * @brief Composition root / state-machine glue for the Jetpack-Joyride game.
 *        Owns the single GameModel instance and wires the public API used by
 *        main.c to the model (logic), view (rendering) and controller (input).
 */
#include <stdio.h>
#include <string.h>
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

/** The whole game state lives here. */
static GameModel M;


/**
 * @brief Authenticate a user against USERS_PATH, creating the account if new
 *        (see game.h). Scans the users file for the name: if found, checks the
 *        password; if absent, appends a new account with 0 XP.
 */
int game_authenticate_user(const char *name, const char *password) {
    /* Use USERS_PATH directly so the file is created inside the game folder. */
    FILE *f = fopen(USERS_PATH, "r");

    char f_name[32], f_pass[32];
    int f_xp;

    if (f) {
        while (fscanf(f, "%s %s %d", f_name, f_pass, &f_xp) == 3) {
            if (strcmp(f_name, name) == 0) {
                if (strcmp(f_pass, password) == 0) {
                    fclose(f);
                    return f_xp; /* user exists and password matches */
                } else {
                    fclose(f);
                    return -1; /* wrong password */
                }
            }
        }
        fclose(f);
    }

    /* The account does not exist: create it (append mode). */
    f = fopen(USERS_PATH, "a");
    if (!f) return 0; /* fail safe if the disk is write-protected */

    fprintf(f, "%s %s %d\n", name, password, 0);
    fclose(f);
    return 0;
}

/**
 * @brief Persist a user's XP to USERS_PATH, rewriting the whole users file.
 *
 * Reads every account, updates @p name's XP (adding the account if missing),
 * then writes the list back.
 *
 * @param name   Player name to update.
 * @param new_xp New XP value to store.
 */
void game_save_user_xp_to_disk(const char *name, int new_xp) {
    char names[100][32], passes[100][32];
    int xps[100], count = 0;
    bool found = false;

    /* 1. Read every existing user. */
    FILE *f = fopen(USERS_PATH, "r");
    if (f) {
        while (count < 100 && fscanf(f, "%s %s %d", names[count], passes[count], &xps[count]) == 3) {
            if (strcmp(names[count], name) == 0) {
                xps[count] = new_xp; /* update the current user's XP */
                found = true;
            }
            count++;
        }
        fclose(f);
    }

    /* 2. If the current user was not found, add them to the list. */
    if (!found && count < 100) {
        strncpy(names[count], name, 31);
        strcpy(passes[count], "1234"); /* fallback default password */
        xps[count] = new_xp;
        count++;
    }

    /* 3. Write the updated list back to the file. */
    f = fopen(USERS_PATH, "w");
    if (f) {
        for (int i = 0; i < count; i++) {
            fprintf(f, "%s %s %d\n", names[i], passes[i], xps[i]);
        }
        fclose(f);
    }
}

/**
 * @brief Initialise the game: seed the model from the RTC, load the persistent
 *        leaderboard and settings, reset XP/login state, load sprites, and start
 *        at the name-input screen with the clock primed.
 */
void game_init(void) {
    model_init(&M, h_res, v_res, rtc_seed());   /* logic + RNG seeded by RTC */
    scores_load(M.high_scores, &M.high_count, MAX_SCORES);  /* persistent leaderboard */
    settings_load(&M);
    M.xp_total        = 0;
    M.powers_unlocked = 0;
    M.extra_life_used = false;
    M.input_name[0]      = '\0';  M.input_name_len      = 0;
    M.input_password[0]  = '\0';  M.input_password_len  = 0;
    M.typing_password    = false;
    M.login_error        = false;
    M.logged_in          = false;
    M.lb_count  = 0;
    M.lb_scroll = 0;
    M.state = STATE_NAME_INPUT;
    view_init();                                /* load sprites (mode already set) */
    game_on_rtc();                              /* prime the clock immediately */
}


/**
 * @brief Reload the leaderboard from disk, cache the top 5 for display, and
 *        switch to the top-scores screen.
 * @param m Game model to update.
 */
void open_leaderboard_top(GameModel *m) {
    /* Reload the file to make sure the board is up to date. */
    scores_load(m->high_scores, &m->high_count, MAX_SCORES);

    /* Copy the 5 best entries into the display cache. */
    scores_get_top(m->high_scores, m->high_count, 5, m->lb_entries, &m->lb_count);

    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_TOP;
}

/**
 * @brief Reload the leaderboard from disk, cache every entry for display, and
 *        switch to the all-scores screen.
 * @param m Game model to update.
 */
void open_leaderboard_all(GameModel *m) {
    /* Reload the file. */
    scores_load(m->high_scores, &m->high_count, MAX_SCORES);

    /* Copy every (already sorted) entry into the display cache. */
    scores_get_all(m->high_scores, m->high_count, m->lb_entries, &m->lb_count);

    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_ALL;
}

/**
 * @brief Refresh the on-screen clock and date strings from the RTC. Called on
 *        each RTC update-ended interrupt.
 */
void game_on_rtc(void) {
    rtc_datetime dt;
    if (rtc_read_datetime(&dt) != 0) return;
    snprintf(M.clock_str, sizeof(M.clock_str), "%02d:%02d:%02d",
             dt.hour, dt.min, dt.sec);
    snprintf(M.date_str, sizeof(M.date_str), "%02d/%02d/%02d",
             dt.day, dt.month, dt.year);
}

/** @brief Release resources before exit. Everything is static, so this is a no-op. */
void game_cleanup(void) {
    /* Everything is statically allocated — nothing to free. */
}


/** @brief Add XP, unlock any newly-reached power thresholds, and persist the total (see game.h). */
void game_add_xp(int amount) {
    M.xp_total += amount;

    /* Unlock powers as thresholds are crossed (only once each). */
    if (M.xp_total >= XP_THRESH_1 && !(M.powers_unlocked & POWER_DOUBLE_COINS))
        game_unlock_power(POWER_DOUBLE_COINS);
    if (M.xp_total >= XP_THRESH_2 && !(M.powers_unlocked & POWER_SHIELD_START))
        game_unlock_power(POWER_SHIELD_START);
    if (M.xp_total >= XP_THRESH_3 && !(M.powers_unlocked & POWER_SLOW_MISSILES))
        game_unlock_power(POWER_SLOW_MISSILES);
    if (M.xp_total >= XP_THRESH_4 && !(M.powers_unlocked & POWER_EXTRA_LIFE))
        game_unlock_power(POWER_EXTRA_LIFE);

    /* The running total is held in memory and persisted per-user (users.txt)
       on game over by game_save_user_xp_to_disk(). */
}

/** @brief @return the accumulated XP total. */
int game_get_xp(void) {
    return M.xp_total;
}

/* ── Powers API ───────────────────────────────────────────────────────────── */
/** @brief Set the @p power_flag bit in the unlocked-powers mask. */
void game_unlock_power(uint32_t power_flag) {
    M.powers_unlocked |= power_flag;
}

/** @brief @return true if @p power_flag is currently unlocked/active. */
bool game_has_power(uint32_t power_flag) {
    return (M.powers_unlocked & power_flag) != 0;
}


/** @brief @return the name the player is currently typing (STATE_NAME_INPUT). */
const char *game_get_input_name(void) {
    return M.input_name;
}


/**
 * @brief Advance the game one tick: update the model, exchange 2-player packets,
 *        drive the WAITING→PLAYING handshake, and on a solo game-over award XP
 *        and insert the run into the persistent leaderboard.
 */
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
        uint32_t agreed = (uint32_t)M.my_seed ^ (uint32_t)M.peer_seed;
        world_rand_seed(agreed);    /* sync obstacle RNG on both machines */
        model_reset(&M);
        M.versus = was_versus;
        M.state = STATE_PLAYING;
    }

    /* On entering game-over from a solo run, persist the score with the RTC
     * timestamp captured for this run. */
    if (M.state == STATE_GAMEOVER && prev != STATE_GAMEOVER && !M.versus) {
        /* Award the distance XP for this run before prompting for the name. */
        int dist_xp = (M.frame_count / 60) * XP_PER_METER;  /* roughly per second */
        game_add_xp(dist_xp);   /* coins/power-ups were already added during the run */

        const char *current_user = (M.input_name_len > 0) ? M.input_name : "PLAYER";
        game_save_user_xp_to_disk(current_user, M.xp_total);


        /* Update the session high score. */
        if (M.score > M.high_score) M.high_score = M.score;

        char tstr[SCORE_WHEN_LEN];
        rtc_datetime dt;
        if (rtc_read_datetime(&dt) == 0) {
            sprintf(tstr, "%02d/%02d/%02d %02d:%02d:%02d",
                    dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec);
        } else {
            snprintf(tstr, sizeof(tstr), "%s %s",
                     M.date_str[0]  ? M.date_str  : "01/01/26",
                     M.clock_str[0] ? M.clock_str : "12:00:00");
        }

        /* Insert the run into the leaderboard with the timestamp string. */
        M.last_rank = scores_insert(M.high_scores, &M.high_count, MAX_SCORES, current_user, M.score, tstr);

        scores_save(M.high_scores, M.high_count);

        scores_get_top(M.high_scores, M.high_count, 5, M.lb_entries, &M.lb_count);
    }

    prev = M.state;
}

/** @brief Render the current frame by delegating to the view. */
void game_draw(void) {
    view_render(&M);
}

/** @brief Forward a keyboard scancode to the controller. @param scancode Make/break code. */
void game_handle_kbd(uint8_t scancode) {
    controller_handle_kbd(&M, scancode);
}

/** @brief Forward a mouse packet to the controller. @param dx,dy Relative motion. @param lb_pressed Left-button state. */
void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed) {
    controller_handle_mouse(&M, dx, dy, lb_pressed);
}

/** @brief @return the current high-level game state. */
game_state_t game_get_state(void) {
    return M.state;
}
