#ifndef _UTILS_H_
#define _UTILS_H_

/**
 * @file utils.h
 * @brief Small low-level helpers shared by the device drivers: a byte-wide
 *        wrapper around sys_inb() and 16-bit MSB/LSB extraction.
 */

#include <stdint.h>

/**
 * @brief Read one byte from an I/O port (wraps sys_inb, narrowing to 8 bits).
 * @param port  I/O port to read from.
 * @param value Out: receives the byte read.
 * @return 0 on success, 1 on failure.
 */
int (util_sys_inb)(int port, uint8_t *value);

/**
 * @brief Extract the most significant byte of a 16-bit value.
 * @param val 16-bit value.
 * @param msb Out: receives bits 15..8 of @p val.
 * @return 0 on success, 1 if @p msb is NULL.
 */
int (util_get_MSB)(uint16_t val, uint8_t *msb);

/**
 * @brief Extract the least significant byte of a 16-bit value.
 * @param val 16-bit value.
 * @param lsb Out: receives bits 7..0 of @p val.
 * @return 0 on success, 1 if @p lsb is NULL.
 */
int (util_get_LSB)(uint16_t val, uint8_t *lsb);

#endif
