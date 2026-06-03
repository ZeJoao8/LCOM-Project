#ifndef _GAME_CONTROLLER_H_
#define _GAME_CONTROLLER_H_

/**
 * @file controller.h
 * @brief Input handling (the "C" of MVC). Translates raw keyboard scancodes
 *        and mouse packets into changes on the model.
 */

#include <stdint.h>
#include <stdbool.h>
#include "game/entities.h"

/** Handle one keyboard scancode (make or break). */
void controller_handle_kbd(GameModel *m, uint8_t scancode);

/** Handle one parsed mouse packet (relative motion + left-button state). */
void controller_handle_mouse(GameModel *m, int16_t dx, int16_t dy, bool lb_pressed);

#endif /* _GAME_CONTROLLER_H_ */
