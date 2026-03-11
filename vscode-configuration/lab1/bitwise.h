#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MSK_END -1

/**
 * Yields a new bitmask equal to `msk` with bit at `pos` set to 0.
 */
uint8_t clear(uint8_t msk, int pos);

/**
 * Yields a new bitmask equal to `msk` with bit at `pos` set to 1.
 */
uint8_t set(uint8_t msk, int pos);

/**
 * Returns true if bit at `pos` in `msk` is set to 1, false otherwise.
 */
bool is_set(uint8_t msk, int pos);

/**
 * Returns the least significant byte of a 16-bit wide bitmask.
 */
uint8_t lsb(uint16_t wide_msk);

/**
 * Returns the most significant byte of a 16-bit wide bitmask.
 */
uint8_t msb(uint16_t wide_msk);

/**
 * Builds a bitmask with bits set at given positions. The variable argument list
 * ends at the sentinel `MSK_END` value, by convention.
 */
uint8_t mask(int pos, ...);
