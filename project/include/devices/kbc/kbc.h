#ifndef _KBC_H_
#define _KBC_H_

/**
 * @file kbc.h
 * @brief Keyboard controller (i8042) driver: interrupt subscription, the
 *        keyboard interrupt handler, and polled read/write of the KBC command
 *        byte. The same controller is also used to relay commands to the mouse.
 */

#include <stdint.h>
#include <stdbool.h>

/** Last scancode byte read by ::kbc_ih (one byte of a make/break code). */
extern uint8_t scancode;
/** Set to true by ::kbc_ih when the last read failed or reported a KBC error. */
extern bool kbc_error;

/**
 * @brief Subscribe the keyboard interrupt (IRQ 1) in reenable+exclusive mode.
 * @param bit_no Out: receives the bit position used in the interrupt mask.
 * @return 0 on success, 1 on failure.
 */
int kbd_subscribe_int(uint8_t *bit_no);

/**
 * @brief Unsubscribe the keyboard interrupt previously subscribed.
 * @return 0 on success, 1 on failure.
 */
int kbd_unsubscribe_int();

/**
 * @brief Keyboard interrupt handler: reads one byte from the output buffer into
 *        ::scancode, setting ::kbc_error on an empty buffer or a parity/timeout
 *        error.
 */
void (kbc_ih)();

/**
 * @brief Poll the KBC output buffer for one data byte (with retries/delay).
 * @param data Out: receives the byte read.
 * @return 0 on success, 1 if no valid byte was read after all retries.
 */
int kbc_read_data(uint8_t *data);

/**
 * @brief Issue a command to the KBC command register once the input buffer is
 *        free (polled, with retries).
 * @param cmd Command byte to write to ::KBC_CMD_REG.
 * @return 0 on success, 1 on timeout/failure.
 */
int kbc_issue_cmd(uint8_t cmd);

/**
 * @brief Read the KBC command byte (issues KBC_READ_CMD then reads the reply).
 * @param cmd Out: receives the current command byte.
 * @return 0 on success, 1 on failure.
 */
int kbc_read_cmd_byte(uint8_t *cmd);

/**
 * @brief Write a new KBC command byte (issues KBC_WRITE_CMD then the argument).
 * @param cmd New command byte value.
 * @return 0 on success, 1 on timeout/failure.
 */
int kbc_write_cmd_byte(uint8_t cmd);

#endif
