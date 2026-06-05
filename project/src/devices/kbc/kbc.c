/**
 * @file kbc.c
 * @brief Keyboard controller (i8042) driver. Provides IRQ subscription, the
 *        keyboard interrupt handler that captures scancodes, and polled
 *        primitives to read/write the KBC command byte and registers.
 */

#include "devices/kbc/kbc.h"
#include "devices/kbc/i8042.h"
#include "devices/utils/utils.h"
#include <lcom/lcf.h>

#define KBC_MAX_RETRIES 20   /* polling attempts before giving up on the KBC */

int kbd_hook_id = 1;       /**< Hook id used for the keyboard IRQ policy. */
uint8_t scancode = 0;      /**< Last scancode byte captured by ::kbc_ih.  */
bool kbc_error = false;    /**< Error flag set by ::kbc_ih on a bad read. */

/** @brief Subscribe IRQ 1 (reenable + exclusive); returns @p bit_no. @return 0 on success, 1 on failure. */
int kbd_subscribe_int(uint8_t *bit_no) {
  *bit_no = kbd_hook_id;
  if (sys_irqsetpolicy(KBD_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &kbd_hook_id) !=
      0) {
    return 1;
  }
  return 0;
}

/** @brief Remove the keyboard IRQ policy. @return 0 on success, 1 on failure. */
int kbd_unsubscribe_int() {
  if (sys_irqrmpolicy(&kbd_hook_id) != 0) {
    return 1;
  }
  return 0;
}

/**
 * @brief Keyboard interrupt handler. Reads the status register and, if the
 *        output buffer is full, reads one byte into ::scancode; sets
 *        ::kbc_error on an I/O failure or a parity/timeout condition.
 */
void(kbc_ih)() {
  uint8_t status;

  kbc_error = false;

  if (util_sys_inb(KBC_ST_REG, &status) != 0) {
    kbc_error = true;
    return;
  }

  if (status & KBC_OBF) {
    if (util_sys_inb(KBC_OUT_BUF, &scancode) != 0) {
      kbc_error = true;
      return;
    }

    if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) != 0) {
      kbc_error = true;
    }
  }
}

/** @brief Poll the output buffer for a valid data byte, retrying with a delay (see kbc.h). */
int kbc_read_data(uint8_t *data) {
  uint8_t status;
  int retries = KBC_MAX_RETRIES;

  while (retries > 0) {
    if (util_sys_inb(KBC_ST_REG, &status) == 0) {
      if (status & KBC_OBF) {
        if (util_sys_inb(KBC_OUT_BUF, data) == 0) {
          if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) == 0) {
            return 0;
          }
        }
      }
    }
    tickdelay(micros_to_ticks(DELAY_US));
    retries--;
  }

  return 1;
}

/** @brief Wait for an empty input buffer, then write the command to the KBC command register (see kbc.h). */
int kbc_issue_cmd(uint8_t cmd) {
  uint8_t status;
  int retries = KBC_MAX_RETRIES;

  while (retries > 0) {
    if (util_sys_inb(KBC_ST_REG, &status) == 0) {
      if ((status & KBC_IBF) == 0) {
        if (sys_outb(KBC_CMD_REG, cmd) == 0) {
          return 0;
        }
      }
    }
    tickdelay(micros_to_ticks(DELAY_US));
    retries--;
  }
  return 1;
}

/** @brief Read the current KBC command byte: issue KBC_READ_CMD, then read the reply (see kbc.h). */
int kbc_read_cmd_byte(uint8_t *cmd) {
  if (kbc_issue_cmd(KBC_READ_CMD) != 0)
    return 1;
  if (kbc_read_data(cmd) != 0)
    return 1;
  return 0;
}

/** @brief Write a new KBC command byte: issue KBC_WRITE_CMD, then write the argument (see kbc.h). */
int kbc_write_cmd_byte(uint8_t cmd) {
  if (kbc_issue_cmd(KBC_WRITE_CMD) != 0)
    return 1;

  uint8_t status;
  int retries = KBC_MAX_RETRIES;

  while (retries > 0) {
    if (util_sys_inb(KBC_ST_REG, &status) == 0) {
      if ((status & KBC_IBF) == 0) {
        if (sys_outb(KBC_OUT_BUF, cmd) == 0) {
          return 0;
        }
      }
    }
    tickdelay(micros_to_ticks(DELAY_US));
    retries--;
  }

  return 1;
}
