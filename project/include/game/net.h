#ifndef _GAME_NET_H_
#define _GAME_NET_H_

/**
 * @file net.h
 * @brief 2-player netplay protocol over the serial port. A tiny framed
 *        protocol exchanges each player's position/score so every machine can
 *        draw the opponent as a "ghost" and decide the winner.
 *
 * Frame layout (7 bytes): SYNC(0xA5) TYPE B0 B1 B2 B3 CHK
 *   CHK = TYPE ^ B0 ^ B1 ^ B2 ^ B3
 *   TYPE_STATE: B0:B1 = player y, B2:B3 = score
 *   TYPE_DEAD : B0:B1 = final score
 */

#include "game/entities.h"

/** Drain received bytes, parse frames and update the opponent fields of @p m. */
void net_poll(GameModel *m);

/** Periodically transmit our state, and send a DEAD frame once on death. */
void net_tick(GameModel *m);

#endif /* _GAME_NET_H_ */
