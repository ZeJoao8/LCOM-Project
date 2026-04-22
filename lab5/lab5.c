// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <lcom/lab5.h>

#include <stdint.h>
#include <stdio.h>

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
    /* To be completed */
    printf("%s(0x%03X, %u, %u, %u, %u, 0x%08x): under construction\n",
            __func__, mode, x, y, width, height, color);

    return 1;
}

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
    /* To be completed */
    printf("%s(%8p, %u, %u): under construction\n", __func__, xpm, x, y);

    return 1;
}
