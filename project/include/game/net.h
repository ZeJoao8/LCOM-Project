#ifndef _GAME_NET_H_
#define _GAME_NET_H_

/**
 * @file net.h
 * @brief 2-player netplay protocol over the serial port. A small framed
 *        protocol exchanges each player's position/score so every machine can
 *        draw the opponent and decide the winner. Frames are atomic on TX
 *        (the whole frame is enqueued only if it fits in the TX ring) and the
 *        receiver tolerates corruption byte-by-byte.
 *
 * Frame layout: SYNC0(0xA5) SYNC1(0x5A) TYPE LEN B0..B(LEN-1) CRC8
 *   - CRC8 (poly 0x07, init 0) covers TYPE, LEN, and the LEN payload bytes.
 *   - TYPE_STATE : 4-byte payload (y_hi, y_lo, score_hi, score_lo)
 *   - TYPE_DEAD  : 2-byte payload (score_hi, score_lo)
 *   - TYPE_HELLO : 0-byte payload (handshake beacon)
 *   - TYPE_READY : 0-byte payload (sent after the peer's HELLO has been seen)
 *   - TYPE_BYE   : 0-byte payload (one-shot, cancels a WAITING or PLAYING peer)
 */

#include "game/entities.h"

/** Drain received bytes, parse complete frames and update @p m. */
void net_poll(GameModel *m);

/** Periodically transmit our state in WAITING/PLAYING and a DEAD frame on death. */
void net_tick(GameModel *m);

/** Reset the receive parser; call on every connection transition. */
void net_reset_parser(void);

/** Send a one-shot BYE so the peer can drop the link cleanly. */
void net_send_bye(void);

#endif /* _GAME_NET_H_ */
