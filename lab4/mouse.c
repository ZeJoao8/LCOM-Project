#include "mouse.h"

static int mouse_hook_id = 2; 
uint8_t mouse_byte = 0;
bool mouse_error = false;

int mouse_subscribe_int(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;
    *bit_no = mouse_hook_id;
    return sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &mouse_hook_id);
}

int mouse_unsubscribe_int() {
    return sys_irqrmpolicy(&mouse_hook_id);
}

void (mouse_ih)() {
    uint8_t st;
    mouse_error = false; 

    if (util_sys_inb(KBC_ST_REG, &st) != 0) { mouse_error = true; return; }

    if (st & KBC_OBF) {
        if (util_sys_inb(KBC_OUT_BUF, &mouse_byte) != 0) { mouse_error = true; return; }
        
        // Ensure data is valid AND originated from the mouse (KBC_AUX bit)
        if ((st & KBC_PARITY_ERR) || (st & KBC_TIMEOUT_ERR) || !(st & KBC_AUX)) { 
            mouse_error = true; 
        }
    } else {
        mouse_error = true;
    }
}

void mouse_parse_packet(uint8_t bytes[3], struct packet *pp) {
    for (int i = 0; i < 3; i++) {
        pp->bytes[i] = bytes[i];
    }

    pp->lb = bytes[0] & BIT(0);
    pp->rb = bytes[0] & BIT(1);
    pp->mb = bytes[0] & BIT(2);
    pp->x_ov = bytes[0] & BIT(6);
    pp->y_ov = bytes[0] & BIT(7);

    // Delta X: Check Bit 4 for sign extension
    if (bytes[0] & BIT(4)) {
        pp->delta_x = bytes[1] | 0xFF00; 
    } else {
        pp->delta_x = bytes[1];
    }

    // Delta Y: Check Bit 5 for sign extension
    if (bytes[0] & BIT(5)) {
        pp->delta_y = bytes[2] | 0xFF00;
    } else {
        pp->delta_y = bytes[2];
    }
}

int mouse_write_cmd(uint8_t cmd) {
    uint8_t st, ack_byte;
    int max_attempts = 5; 

    while (max_attempts > 0) {
        if (util_sys_inb(KBC_ST_REG, &st) != 0) return 1;
        if (st & KBC_IBF) { 
            tickdelay(micros_to_ticks(DELAY_US)); 
            continue; // Está ocupado, tenta de novo
        }
        if (sys_outb(KBC_CMD_REG, WRITE_BYTE_MOUSE) != 0) return 1;

        if (util_sys_inb(KBC_ST_REG, &st) != 0) return 1;
        if (st & KBC_IBF) { 
            tickdelay(micros_to_ticks(DELAY_US)); 
            continue; 
        }
        if (sys_outb(KBC_OUT_BUF, cmd) != 0) return 1;

        tickdelay(micros_to_ticks(DELAY_US));
        if (util_sys_inb(KBC_OUT_BUF, &ack_byte) != 0) return 1;

        if (ack_byte == ACK) return 0;
        if (ack_byte == ERROR) return 1;

        max_attempts--;
    }
    return 1; // Falhou todas as tentativas
}
