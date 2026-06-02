#ifndef _GAME_VIEW_H_
#define _GAME_VIEW_H_

/**
 * @file view.h
 * @brief Rendering (the "V" of MVC). Reads the model and draws a frame; it
 *        never mutates game state.
 */

#include "game/entities.h"

/** Load sprites. Call once after the video mode is set. */
void view_init(void);

/** Draw the current frame for the given model and present it to VRAM. */
void view_render(const GameModel *m);

#endif /* _GAME_VIEW_H_ */
