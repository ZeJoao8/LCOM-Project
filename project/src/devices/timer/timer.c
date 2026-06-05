/**
 * @file timer.c
 * @brief i8254 timer driver. Configures the timer frequency, subscribes its
 *        interrupt, counts ticks in the interrupt handler, and reads/displays
 *        timer configuration. Timer 0 drives the game's fixed update/render loop.
 */

#include <lcom/lcf.h>
#include <lcom/timer.h>
#include <minix/syslib.h>
#include <stdint.h>

#include "devices/timer/i8254.h"
int timer_counter = 0;     /**< Number of timer-0 interrupts since subscription. */
int hook_id_timer = 0;     /**< Hook id used for the timer IRQ policy.           */


/**
 * @brief Program a timer's output frequency, preserving its counting mode.
 *
 * Reads the current configuration to keep the low nibble, sets LSB+MSB access,
 * selects the timer, and writes the computed divisor (TIMER_FREQ / @p freq).
 *
 * @param timer Timer to configure (0, 1 or 2).
 * @param freq  Desired frequency in Hz.
 * @return 0 on success, 1 on failure or invalid timer.
 */
int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
  
    uint8_t st;

    if (timer_get_conf(timer, &st) != 0) return 1;

    uint8_t control = (st & 0x0F) | TIMER_LSB_MSB;

    switch(timer) {
        case 0: control |= TIMER_SEL0; break;
        case 1: control |= TIMER_SEL1; break;
        case 2: control |= TIMER_SEL2; break;
        default: return 1; 
    }

    if (sys_outb(TIMER_CTRL, control) != OK) return 1;

    uint16_t div = TIMER_FREQ / freq;

    uint8_t lsb, msb;
    util_get_LSB(div, &lsb);
    util_get_MSB(div, &msb);

    if (sys_outb(TIMER_0 + timer, lsb) != OK) return 1;
    if (sys_outb(TIMER_0 + timer, msb) != OK) return 1;

  return 0;

}

/**
 * @brief Subscribe the timer-0 interrupt (IRQ 0) in reenable mode and enable it.
 * @param bit_no Out: receives the bit position used in the interrupt mask.
 * @return 0 on success, 1 on failure.
 */
int (timer_subscribe_int)(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;

    *bit_no = 0; 

    if (sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &hook_id_timer) != OK)
        return 1;

    if (sys_irqenable(&hook_id_timer) != OK)
        return 1;

    return 0; // sucesso
}

/**
 * @brief Disable and remove the timer-0 interrupt policy.
 * @return 0 on success, 1 on failure.
 */
int (timer_unsubscribe_int)() {
    if (sys_irqdisable(&hook_id_timer) != OK) return 1;
    if (sys_irqrmpolicy(&hook_id_timer) != OK) return 1;
    return 0;
}

/** @brief Timer interrupt handler: increments ::timer_counter. */
void (timer_int_handler)() {
    timer_counter++;
}


/**
 * @brief Read a timer's configuration byte via the read-back command.
 * @param timer Timer to query (0, 1 or 2).
 * @param st    Out: receives the status/configuration byte.
 * @return 0 on success, 1 on failure or invalid timer.
 */
int (timer_get_conf)(uint8_t timer, uint8_t *st) {
    
    if (timer > 2) return 1;

    
    uint8_t cmd = 0xE0 | (1 << (timer + 1) );

    if (sys_outb(TIMER_CTRL, cmd) != OK) return 1;

    return util_sys_inb(TIMER_0 + timer, st);
}

/**
 * @brief Decode a requested field from a timer configuration byte and print it.
 * @param timer Timer the byte belongs to (0, 1 or 2).
 * @param st    Configuration byte previously read with ::timer_get_conf.
 * @param field Which field to extract (all/initialization/mode/base).
 * @return 0 on success, 1 on invalid timer or field.
 */
int (timer_display_conf)(uint8_t timer, uint8_t st,
                         enum timer_status_field field) {
    if (timer > 2) return 1;

    union timer_status_field_val val;

    switch (field) {
        case tsf_all:
            val.byte = st;
            break;

        case tsf_initial:
            switch ((st >> 4) & 0x03) {
                case 1:  val.in_mode = LSB_only;      break;
                case 2:  val.in_mode = MSB_only;      break;
                case 3:  val.in_mode = MSB_after_LSB; break;
                default: val.in_mode = INVAL_val;     break;
            }
            break;

        case tsf_mode:
            val.count_mode = (st >> 1) & 0x07;
            break;

        case tsf_base:
            val.bcd = st & 0x01;
            break;

        default:
            return 1;
    }

    return timer_print_config(timer, field, val);
}

