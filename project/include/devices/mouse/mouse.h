#ifndef MOUSE_H
#define MOUSE_H

/**
 * @file mouse.h
 * @brief PS/2 mouse driver: interrupt subscription, the mouse interrupt
 *        handler, 3-byte packet synchronization/parsing, and command issuing
 *        through the KBC. Drives the menu cursor and the click-to-thrust input.
 */

#include <stdint.h>
#include <stdbool.h>

/** Most recently parsed mouse packet (buttons + signed deltas + overflow). */
extern struct packet mouse_packet;
/** Index (0..2) of the next byte expected within the current packet. */
extern uint8_t packet_byte_idx;
/** Set to true when a byte read failed or the packet lost synchronization. */
extern bool mouse_error;
/** Raw byte most recently read from the mouse by ::mouse_ih. */
extern uint8_t mouse_current_byte;

/**
 * @brief Subscribe the mouse interrupt (IRQ 12) in reenable+exclusive mode.
 * @param bit_no Out: receives the bit position used in the interrupt mask.
 * @return 0 on success, 1 on failure.
 */
int (mouse_subscribe_int)(uint8_t *bit_no);

/**
 * @brief Unsubscribe the mouse interrupt previously subscribed.
 * @return 0 on success, 1 on failure.
 */
int (mouse_unsubscribe_int)();

/**
 * @brief Mouse interrupt handler: reads one byte from the KBC output buffer
 *        into ::mouse_current_byte, setting ::mouse_error on failure.
 */
void (mouse_ih)();

/**
 * @brief Store the byte read by ::mouse_ih at ::packet_byte_idx, enforcing the
 *        sync bit on the first byte so the 3-byte packet stays aligned.
 */
void (mouse_sync_packet)();

/**
 * @brief Decode the three collected bytes into ::mouse_packet (button flags and
 *        sign-extended X/Y deltas).
 */
void (mouse_parse_packet)();

/**
 * @brief Send a command byte to the mouse through the KBC, retrying until it
 *        acknowledges (MOUSE_ACK).
 * @param cmd Mouse command byte (e.g. MOUSE_ENABLE_DATA_REP).
 * @return 0 on success, 1 on failure.
 */
int (mouse_write_cmd)(uint8_t cmd);

#endif
