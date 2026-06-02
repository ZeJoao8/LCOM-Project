#ifndef _SERIAL_H_
#define _SERIAL_H_

/**
 * @file serial.h
 * @brief Interrupt-driven UART (16550) driver for COM1. Uses RX/TX ring buffers
 *        filled and drained from the interrupt handler. Used by the 2-player
 *        mode to exchange game state between two MINIX machines.
 */

#include <stdint.h>
#include <stdbool.h>

/** Subscribe the serial interrupt (IRQ 4). Returns 0 on success. */
int serial_subscribe_int(uint8_t *bit_no);

/** Unsubscribe the serial interrupt. Returns 0 on success. */
int serial_unsubscribe_int(void);

/** Configure COM1: 8N1, FIFO on, RX interrupt enabled. Returns 0 on success. */
int serial_config(void);

/** Interrupt handler: drains the receiver and feeds the transmitter. */
void serial_ih(void);

/** Queue one byte for transmission. Returns 0 on success, 1 if the TX ring is full. */
int serial_putc(uint8_t byte);

/** Pop one received byte, or -1 if none are available. */
int serial_getc(void);

/** True if there is at least one received byte waiting. */
bool serial_available(void);

#endif /* _SERIAL_H_ */
