#include <lcom/lcf.h>
#include <lcom/timer.h>
#include <minix/syslib.h>
#include <stdint.h>

#include "i8254.h"
int timer_counter = 0; 
int hook_id_timer = 0;

int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
  
    uint8_t st;

    if (timer_get_conf(timer, &st) != 0) return 1;

    uint8_t control = (st & 0x0F) | TIMER_LSB_MSB;

    control |= TIMER_SEL0 << timer;

    if (sys_outb(TIMER_CTRL, control) != OK) return 1;

    uint16_t div = TIMER_FREQ / freq;

    uint8_t lsb, msb;
    util_get_LSB(div, &lsb);
    util_get_MSB(div, &msb);

    if (sys_outb(TIMER_0 + timer, lsb) != OK) return 1;
    if (sys_outb(TIMER_0 + timer, msb) != OK) return 1;

  return 0;

}

int (timer_subscribe_int)(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;

    hook_id_timer = TIMER0_IRQ;   

    *bit_no = hook_id_timer;

    if (sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &hook_id_timer) != OK)
        return 1;

    if (sys_irqenable(&hook_id_timer) != OK)
        return 1;

    

    return 0; // sucesso
}

int (timer_unsubscribe_int)() {
    if (sys_irqdisable(&hook_id_timer) != OK) return 1;
    if (sys_irqrmpolicy(&hook_id_timer) != OK) return 1;
    return 0;
}

void (timer_int_handler)() {
    timer_counter++;
}



int (timer_get_conf)(uint8_t timer, uint8_t *st) {
    
    if (timer > 2) return 1;

    
    uint8_t cmd = 0xE0 | (1 << (timer + 1) );

    if (sys_outb(TIMER_CTRL, cmd) != OK) return 1;

    return util_sys_inb(TIMER_0 + timer, st);
}

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

