/**
 * @file mouse.c
 * @brief PS/2 mouse driver. Subscribes IRQ 12, captures bytes in the interrupt
 *        handler, reassembles and parses the 3-byte packets, and issues mouse
 *        commands through the KBC.
 */

#include "devices/mouse/mouse.h"
#include "devices/kbc/i8042.h"
#include "devices/kbc/kbc.h"
#include "devices/utils/utils.h"
#include <lcom/lcf.h>
#include <stdint.h>

int mouse_hook_id = 12;        /**< Hook id for the mouse IRQ (12) policy.       */
uint8_t mouse_bytes[3];        /**< Bytes of the packet currently being built.   */
struct packet mouse_packet;    /**< Most recently parsed packet.                 */
uint8_t packet_byte_idx = 0;   /**< Index of the next byte expected (0..2).      */
uint8_t mouse_current_byte;    /**< Last raw byte read by ::mouse_ih.            */
bool mouse_error = false;      /**< Error flag set by ::mouse_ih on a bad read.  */

/** @brief Subscribe IRQ 12 (reenable + exclusive); returns @p bit_no. @return 0 on success, 1 on failure. */
int(mouse_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL)
    return 1;
  *bit_no = mouse_hook_id;
  if (sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE,
                       &mouse_hook_id) != 0)
    return 1;
  return 0;
}

/** @brief Remove the mouse IRQ policy. @return 0 on success, 1 on failure. */
int(mouse_unsubscribe_int)() {
  if (sys_irqrmpolicy(&mouse_hook_id) != 0)
    return 1;
  return 0;
}

/**
 * @brief Mouse interrupt handler. Validates the KBC status (output buffer full
 *        and AUX bit set), reads one byte into ::mouse_current_byte, and sets
 *        ::mouse_error on any failure or parity/timeout condition.
 */
void(mouse_ih)() {
  uint8_t status;
  mouse_error = false;

  if (util_sys_inb(KBC_ST_REG, &status) != 0) {
    mouse_error = true;
    return;
  }

  if (status & KBC_OBF) {
    if ((status & KBC_AUX) == 0) {
      mouse_error = true; // not a mouse byte
      return;
    }
    if (util_sys_inb(KBC_OUT_BUF, &mouse_current_byte) != 0) {
      mouse_error = true;
      return;
    }
    if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) != 0) {
      mouse_error = true;
      return;
    }
  } else {
    mouse_error = true;
  }
}

/**
 * @brief Append the last read byte to the packet being assembled. The first
 *        byte is only accepted if its sync bit is set, which keeps the 3-byte
 *        packet aligned; otherwise the byte is dropped.
 */
void(mouse_sync_packet)() {
  if (packet_byte_idx == 0 && (mouse_current_byte & MOUSE_SYNC) == 0) {
    // Out of sync
    return;
  }

  mouse_bytes[packet_byte_idx] = mouse_current_byte;
  packet_byte_idx++;
}

/**
 * @brief Decode the three assembled bytes into ::mouse_packet: button flags,
 *        sign-extended X/Y deltas, and overflow flags.
 */
void(mouse_parse_packet)() {
  mouse_packet.bytes[0] = mouse_bytes[0];
  mouse_packet.bytes[1] = mouse_bytes[1];
  mouse_packet.bytes[2] = mouse_bytes[2];

  mouse_packet.lb = mouse_bytes[0] & MOUSE_LB;
  mouse_packet.rb = mouse_bytes[0] & MOUSE_RB;
  mouse_packet.mb = mouse_bytes[0] & MOUSE_MB;

  if (mouse_bytes[0] & MOUSE_X_SIGN) {
    mouse_packet.delta_x = 0xFF00 | mouse_bytes[1];
  } else {
    mouse_packet.delta_x = mouse_bytes[1];
  }

  if (mouse_bytes[0] & MOUSE_Y_SIGN) {
    mouse_packet.delta_y = 0xFF00 | mouse_bytes[2];
  } else {
    mouse_packet.delta_y = mouse_bytes[2];
  }

  mouse_packet.x_ov = mouse_bytes[0] & MOUSE_X_OVF;
  mouse_packet.y_ov = mouse_bytes[0] & MOUSE_Y_OVF;
}

/** @brief Send a command to the mouse via the KBC and wait for ACK (NACK retries, ERR aborts; see mouse.h). */
int(mouse_write_cmd)(uint8_t cmd) {
  uint8_t ack_byte;
  int retries = 10;
  while (retries > 0) {
    if (kbc_issue_cmd(KBC_WRITE_MOUSE) != 0)
      return 1;

    // Wait for IBF to be 0 to write arguments
    uint8_t status;
    int wait_retries = 20;
    bool args_written = false;
    while (wait_retries > 0) {
      if (util_sys_inb(KBC_ST_REG, &status) == 0) {
        if ((status & KBC_IBF) == 0) {
          if (sys_outb(KBC_OUT_BUF, cmd) == 0) {
            args_written = true;
            break;
          }
        }
      }
      tickdelay(micros_to_ticks(DELAY_US));
      wait_retries--;
    }
    if (!args_written)
      return 1;

    if (kbc_read_data(&ack_byte) == 0) {
      if (ack_byte == MOUSE_ACK)
        return 0;
      if (ack_byte == MOUSE_ERR)
        return 1;
      // NACK -> retry
    }
    retries--;
  }
  return 1;
}
