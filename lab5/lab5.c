#include <lcom/lcf.h>
#include <lcom/lab5.h>
#include <stdint.h>
#include <stdio.h>

#include "video_gr.h"

#include "../lab3/kbc.h"



#define ESC_BREAKCODE 0x81

// Any header files included below this line should have been created by you

int main(int argc, char *argv[]) {
    // sets the language of LCF messages (can be either EN-US or PT-PT)
    lcf_set_language("EN-US");

    // enables to log function invocations that are being "wrapped" by LCF
    // [comment this out if you don't want/need it]
    lcf_trace_calls("/home/lcom/labs/lab5/trace.txt");

    // enables to save the output of printf function calls on a file
    // [comment this out if you don't want/need it]
    lcf_log_output("/home/lcom/labs/lab5/output.txt");

    // handles control over to LCF
    // [LCF handles command line arguments and invokes the right function]
    if (lcf_start(argc, argv))
        return 1;

    // LCF clean up tasks
    // [must be the last statement before return]
    lcf_cleanup();

    return 0;
}



int(video_test_init)(uint16_t mode, uint8_t delay) {
    reg86_t reg;
    memset(&reg, 0, sizeof(reg));    
    
    reg.intno = 0x10;
    reg.ah = 0x4F;
    reg.al = 0x02;
    reg.bx = mode | BIT(14);
    
    if (sys_int86(&reg) != OK) {
        printf("video_test_init: sys_int86 failed\n");
        return 1;
    }

    if (reg.ah != 0x00 || reg.al != 0x4F) {
        printf("video_test_init: VBE set mode failed\n");
        return 1;
    }

    sleep(delay);
    
    if (vg_exit() != OK) {
        printf("video_test_init: vg_exit failed\n");
        return 1;
    }

    return 0;
}
int(video_test_rectangle)(uint16_t mode, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height, uint32_t color) {
            if (vg_init(mode) == NULL) {
        printf("video_test_rectangle: vg_init failed\n");
        return 1;
    }
 
    if (vg_draw_rectangle(x, y, width, height, color) != 0) {
        printf("video_test_rectangle: vg_draw_rectangle failed\n");
        vg_exit();
        return 1;
    }
 
    uint8_t kbd_bit_no;
    if (kbd_subscribe_int(&kbd_bit_no) != 0) {
        printf("video_test_rectangle: kbd_subscribe_int failed\n");
        vg_exit();
        return 1;
    }
 
    int     ipc_status;
    message msg;
    scancode = 0;
 
    while (scancode != ESC_BREAKCODE) {
        if (driver_receive(ANY, &msg, &ipc_status) != OK)
            continue;
 
        if (is_ipc_notify(ipc_status) &&
            _ENDPOINT_P(msg.m_source) == HARDWARE &&
            (msg.m_notify.interrupts & kbd_bit_no)) {
            kbc_ih();
        }
    }
 
    if (kbd_unsubscribe_int() != 0) {
        printf("video_test_rectangle: kbd_unsubscribe_int failed\n");
        vg_exit();
        return 1;
    }
 
    if (vg_exit() != OK) {
        printf("video_test_rectangle: vg_exit failed\n");
        return 1;
    }
    return 0;





}

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
        if (vg_init(0x105) == NULL) {
        printf("video_test_xpm: vg_init failed\n");
        return 1;
    }
 
    xpm_image_t img;
    uint8_t *pixmap = xpm_load(xpm, XPM_INDEXED, &img);
    if (pixmap == NULL) {
        printf("video_test_xpm: xpm_load failed\n");
        vg_exit();
        return 1;
    }
 
    if (vg_draw_pixmap(pixmap, img, x, y) != 0) {
        printf("video_test_xpm: vg_draw_pixmap failed\n");
        vg_exit();
        return 1;
    }
 
    uint8_t kbd_bit_no;
    if (kbd_subscribe_int(&kbd_bit_no) != 0) {
        printf("video_test_xpm: kbd_subscribe_int failed\n");
        vg_exit();
        return 1;
    }
 
    int     ipc_status;
    message msg;
    scancode = 0;
 
    while (scancode != ESC_BREAKCODE) {
        if (driver_receive(ANY, &msg, &ipc_status) != OK)
            continue;
 
        if (is_ipc_notify(ipc_status) &&
            _ENDPOINT_P(msg.m_source) == HARDWARE &&
            (msg.m_notify.interrupts & kbd_bit_no)) {
            kbc_ih();
        }
    }
 
    if (kbd_unsubscribe_int() != 0) {
        printf("video_test_xpm: kbd_unsubscribe_int failed\n");
        vg_exit();
        return 1;
    }
 
    if (vg_exit() != OK) {
        printf("video_test_xpm: vg_exit failed\n");
        return 1;
    }
    return 0;




}
