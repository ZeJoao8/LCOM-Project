#ifndef _GAME_SETTINGS_H_
#define _GAME_SETTINGS_H_

/**
 * @file settings.h
 * @brief Persistent settings management. Saves and loads game difficulty,
 *        control mode, speed settings, and visual effects configurations from disk.
 */

#include "game/entities.h"

/** Load game settings from SETTINGS_PATH. */
void settings_load(GameModel *m);

/** Save game settings to SETTINGS_PATH. */
void settings_save(const GameModel *m);

#endif /* _GAME_SETTINGS_H_ */
