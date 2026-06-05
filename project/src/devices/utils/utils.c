/**
 * @file utils.c
 * @brief Low-level helpers shared by the device drivers: 16-bit byte extraction
 *        and a byte-wide wrapper around sys_inb().
 */

#include <lcom/lcf.h>

#include <stdint.h>

/** @brief Write the least significant byte of @p val to @p lsb. @return 0 on success, 1 if @p lsb is NULL. */
int(util_get_LSB)(uint16_t val, uint8_t *lsb) {

  if (lsb == NULL) return 1;
  *lsb = val & 0xFF;
  return 0;

}

/** @brief Write the most significant byte of @p val to @p msb. @return 0 on success, 1 if @p msb is NULL. */
int(util_get_MSB)(uint16_t val, uint8_t *msb) {

  if (msb == NULL) return 1;
  *msb = (val >> 8) & 0xFF;
  return 0;

}

/** @brief Read one byte from I/O @p port into @p value via sys_inb(). @return 0 on success, 1 on failure. */
int util_sys_inb(int port, uint8_t *value) {
    uint32_t temp;

    if (sys_inb(port, &temp) != 0)
        return 1;

    *value = (uint8_t) temp;

    return 0;
}
