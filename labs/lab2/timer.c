#include <lcom/lcf.h>
#include <lcom/timer.h>

#include <stdint.h>

#include "i8254.h"

static int timer_hook_id = 0;
static uint32_t timer_counter = 0;

int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
  if (timer > 2 || freq < 19 || freq > TIMER_FREQ) return 1;

  uint8_t st;
  if (timer_get_conf(timer, &st) != 0) return 1;

  /* Build control word: select timer, LSB+MSB access, preserve current mode, binary */
  uint8_t ctrl;
  switch (timer) {
    case 0: ctrl = TIMER_SEL0; break;
    case 1: ctrl = TIMER_SEL1; break;
    case 2: ctrl = TIMER_SEL2; break;
    default: return 1;
  }

  /* Preserve operating mode bits [3:1] from status byte */
  ctrl |= TIMER_LSB_MSB | (st & 0x0E) | TIMER_BIN;

  uint16_t divisor = (uint16_t)(TIMER_FREQ / freq);

  uint8_t lsb, msb;
  if (util_get_LSB(divisor, &lsb) != 0) return 1;
  if (util_get_MSB(divisor, &msb) != 0) return 1;

  if (sys_outb(TIMER_CTRL, ctrl) != 0) return 1;
  if (sys_outb(TIMER_0 + timer, lsb) != 0) return 1;
  if (sys_outb(TIMER_0 + timer, msb) != 0) return 1;

  return 0;
}

int (timer_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL) return 1;
  *bit_no = timer_hook_id;
  return sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &timer_hook_id);
}

int (timer_unsubscribe_int)() {
  return sys_irqrmpolicy(&timer_hook_id);
}

void (timer_int_handler)() {
  timer_counter++;
}

int (timer_get_conf)(uint8_t timer, uint8_t *st) {

  if (st == NULL) return 1;

  uint8_t cmd = TIMER_RB_CMD | TIMER_RB_COUNT_ | TIMER_RB_SEL(timer);

  if (sys_outb(TIMER_CTRL, cmd) != 0) {
    return 1;
  }

  if (util_sys_inb(TIMER_0 + timer, st) != 0) {
    return 1;
  }

  return 0;
}

int (timer_display_conf)(uint8_t timer, uint8_t st,
                        enum timer_status_field field) {
  union timer_status_field_val val;

  switch (field) {
    case tsf_all:
      val.byte = st;
      break;
    case tsf_initial:
      val.in_mode = (st >> 4) & 0x03;
      break;
    case tsf_mode:
      val.count_mode = (st >> 1) & 0x07;
      break;
    case tsf_base:
      val.bcd = st & BIT(0);
      break;
    default:
      return 1;
  }

  return timer_print_config(timer, field, val);
}
