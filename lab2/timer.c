#include <lcom/lcf.h>
#include <lcom/timer.h>

#include <stdint.h>

#include "i8254.h"

int timer_hook_id = 0; 
uint32_t timer_counter = 0;

int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
    if (timer > 2 || freq > TIMER_FREQ || freq < 19) {
        return 1;
    }

    uint8_t st;
    if (timer_get_conf(timer, &st) != 0) {
        return 1;
    }

    uint8_t ctrl_word = (st & 0x0F) | TIMER_LSB_MSB;

    switch (timer) {
        case 0: ctrl_word |= TIMER_SEL0; break;
        case 1: ctrl_word |= TIMER_SEL1; break;
        case 2: ctrl_word |= TIMER_SEL2; break;
    }

    if (sys_outb(TIMER_CTRL, ctrl_word) != 0) {
        return 1;
    }

    uint16_t div = TIMER_FREQ / freq;

    uint8_t lsb, msb;
    util_get_LSB(div, &lsb);
    util_get_MSB(div, &msb);

    int timer_port = TIMER_0 + timer;
    if (sys_outb(timer_port, lsb) != 0) return 1;
    if (sys_outb(timer_port, msb) != 0) return 1;

    return 0;
}

int (timer_subscribe_int)(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;
    
    *bit_no = BIT(timer_hook_id); 
    
    return sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &timer_hook_id);
}

int (timer_unsubscribe_int)() {
    return sys_irqrmpolicy(&timer_hook_id);
}

void (timer_int_handler)() {
    timer_counter++;
}


int (timer_get_conf)(uint8_t timer, uint8_t *st) {
    if (timer > 2 || st == NULL) {
        return 1;
    }

    uint8_t read_back_cmd = 0xE0 | (1 << (timer + 1));

    if (sys_outb(0x43, read_back_cmd) != 0) {
        return 1;
    }
    
    return util_sys_inb(0x40 + timer, st);
}

int (timer_display_conf)(uint8_t timer, uint8_t st, enum timer_status_field field) {
    union timer_status_field_val val;

    switch (field) {
        case tsf_all:
            val.byte = st; 
            break;

        case tsf_initial: {
            uint8_t init_mode = (st & 0x30) >> 4;
            if (init_mode == 1) val.in_mode = LSB_only;
            else if (init_mode == 2) val.in_mode = MSB_only;
            else if (init_mode == 3) val.in_mode = MSB_after_LSB;
            else val.in_mode = INVAL_val;
            break;
        }

        case tsf_mode: {
            uint8_t count_mode = (st & 0x0E) >> 1;
            
            if (count_mode == 6) count_mode = 2;
            if (count_mode == 7) count_mode = 3;
            
            val.count_mode = count_mode;
            break;
        }

        case tsf_base:
            val.bcd = (st & 0x01);
            break;

        default:
            return 1;
    }

    return timer_print_config(timer, field, val);
}
