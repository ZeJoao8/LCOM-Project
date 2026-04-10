#include <lcom/lcf.h>

#include <lcom/lab3.h>

#include <stdbool.h>
#include <stdint.h>

#include "i8042.h"
#include "kbc.h"

#ifdef LAB3
extern uint32_t sys_inb_counter;
#endif

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need it]
  lcf_trace_calls("/home/lcom/labs/lab3/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab3/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

int(kbd_test_scan)() {
  int ipc_status;
  message msg;
  uint8_t irq_set;

  if (kbd_subscribe_int(&irq_set) != 0) {
    return 1;
  }

  uint8_t bytes[2];
  uint8_t size = 0;

  while (scancode != ESC_BREAKCODE) {
    if (driver_receive(ANY, &msg, &ipc_status) != 0) {
      continue;
    }

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
      case HARDWARE:
        if (msg.m_notify.interrupts & irq_set) {
          kbc_ih();

          if (kbc_error) {
            continue;
          }

          bytes[size] = scancode;
          size++;

          if (scancode == TWO_BYTE_SCANCODE) {
            continue; // Wait for the second byte
          }

          bool make = !(scancode & BREAK_CODE_BIT);
          kbd_print_scancode(make, size, bytes);
          size = 0;
        }
        break;
      default:
        break;
      }
    }
  }

  if (kbd_unsubscribe_int() != 0) {
    return 1;
  }

  return 0;
}

int(kbd_test_poll)() {
  uint8_t bytes[2];
  uint8_t size = 0;

  while (scancode != ESC_BREAKCODE) {
    if (kbc_read_data(&scancode) != 0) {
      continue;
    }

    bytes[size] = scancode;
    size++;

    if (scancode == TWO_BYTE_SCANCODE) {
      continue;
    }

    bool make = !(scancode & BREAK_CODE_BIT);
    kbd_print_scancode(make, size, bytes);
    size = 0;
  }

  // Restore keyboard interrupts
  uint8_t cmd_byte;
  if (kbc_read_cmd_byte(&cmd_byte) != 0)
    return 1;
  cmd_byte |= KBC_INT_KBD;
  if (kbc_write_cmd_byte(cmd_byte) != 0)
    return 1;

  return 0;
}

extern int timer_counter;

int(kbd_test_timed_scan)(uint8_t n) {
  int ipc_status;
  message msg;
  uint8_t kbd_irq_bit, timer_irq_bit;

  if (kbd_subscribe_int(&kbd_irq_bit) != 0)
    return 1;
  if (timer_subscribe_int(&timer_irq_bit) != 0)
    return 1;

  uint8_t bytes[2];
  uint8_t size = 0;

  timer_counter = 0;

  while (scancode != ESC_BREAKCODE && (timer_counter / 60) < n) {
    if (driver_receive(ANY, &msg, &ipc_status) != 0) {
      continue;
    }

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
      case HARDWARE:
        // Timer interrupt
        if (msg.m_notify.interrupts & BIT(timer_irq_bit)) {
          timer_int_handler();
        }

        // Keyboard interrupt
        if (msg.m_notify.interrupts & kbd_irq_bit) {
          kbc_ih();

          if (kbc_error) {
            continue;
          }

          bytes[size] = scancode;
          size++;

          if (scancode == TWO_BYTE_SCANCODE) {
            continue;
          }

          bool make = !(scancode & BREAK_CODE_BIT);
          kbd_print_scancode(make, size, bytes);
          size = 0;

          // Reset the timeout counter whenever a key is pressed
          timer_counter = 0;
        }
        break;
      default:
        break;
      }
    }
  }

  if (kbd_unsubscribe_int() != 0)
    return 1;
  if (timer_unsubscribe_int() != 0)
    return 1;

  return 0;
}
