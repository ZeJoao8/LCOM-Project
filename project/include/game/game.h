#ifndef _GAME_H_
#define _GAME_H_

/**
 * @file game.h
 * @brief Public game facade called from main.c. Owns the single GameModel,
 *        wires together model/view/controller, drives the state machine, and
 *        exposes the XP/powers and user-authentication APIs.
 */

#include <stdint.h>
#include <stdbool.h>

/* ── Game states ──────────────────────────────────────────────────────────── */
/** High-level screen/state the game is currently in. */
typedef enum {
    STATE_MENU,              /**< Main menu.                                    */
    STATE_INSTRUCTIONS,      /**< Instructions screen.                          */
    STATE_SETTINGS,          /**< Settings screen.                              */
    STATE_WAITING,           /**< 2-player lobby: handshake with the peer over COM1. */
    STATE_PLAYING,           /**< Active gameplay.                              */
    STATE_PAUSE,             /**< Game paused.                                  */
    STATE_GAMEOVER,          /**< Run ended; showing the game-over screen.      */
    STATE_LEADERBOARD_TOP,   /**< Leaderboard — top 5 scores.                   */
    STATE_LEADERBOARD_ALL,   /**< Leaderboard — all stored scores.              */
    STATE_NAME_INPUT,        /**< Leaderboard — prompting the player's name.    */
    STATE_EXIT               /**< Request to quit the program.                  */
} game_state_t;



/* ── Public API called from main.c ────────────────────────────────────────── */
/** @brief Initialise the game: load persistent data, set up the model and view. */
void game_init(void);
/** @brief Advance the game by one tick (update the active state's logic). */
void game_update(void);
/** @brief Render the current frame for the active state. */
void game_draw(void);
/** @brief Route one keyboard scancode to the active state's input handling. */
void game_handle_kbd(uint8_t scancode);
/** @brief Route one mouse packet (relative motion + left button) to the active state. */
void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed);
/** @brief Refresh the on-screen clock; called on each RTC update interrupt. */
void game_on_rtc(void);
/** @brief Release resources and persist state before exiting. */
void game_cleanup(void);

/** @brief Current high-level game state. @return the active ::game_state_t. */
game_state_t game_get_state(void);

/* ── XP API ── */
/**
 * @brief Add XP to the running total and unlock any powers whose thresholds are crossed.
 * @param amount XP to add.
 */
void game_add_xp(int amount);

/** @brief Total XP accumulated. @return the current XP total. */
int  game_get_xp(void);

/* ── Powers API ── */
/**
 * @brief Unlock a power flag (one of the POWER_* bits defined in config.h).
 * @param power_flag Bit mask of the power to unlock.
 */
void game_unlock_power(uint32_t power_flag);

/**
 * @brief Check whether a given power is currently active.
 * @param power_flag Bit mask of the power to test.
 * @return true if the power is active.
 */
bool game_has_power(uint32_t power_flag);

/**
 * @brief Authenticate (or register) a user by name and password.
 * @param name     Player name.
 * @param password Player password.
 * @return Non-negative on success (e.g. the user index/result), negative on failure.
 */
int game_authenticate_user(const char *name, const char *password);

#endif /* _GAME_H_ */
