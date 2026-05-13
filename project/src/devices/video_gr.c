#include <lcom/lcf.h>
#include <string.h>
#include "devices/video_gr.h"

/* ── Module globals ──────────────────────────────────────────────────────── */
void    *video_mem      = NULL;
uint16_t h_res          = 0;
uint16_t v_res          = 0;
uint8_t  bits_per_pixel = 0;

static uint32_t vram_size = 0;

/* ── vg_init ─────────────────────────────────────────────────────────────── */
/* Overrides the weak symbol in liblcf.a                                     */

void *vg_init(uint16_t mode) {
    /* 1. Get mode info via LCF vbe_get_mode_info() */
    vbe_mode_info_t info;
    if (vbe_get_mode_info(mode, &info) != 0) {
        printf("vg_init: vbe_get_mode_info failed\n");
        return NULL;
    }

    h_res          = info.XResolution;
    v_res          = info.YResolution;
    bits_per_pixel = info.BitsPerPixel;
    vram_size      = (uint32_t) v_res * info.BytesPerScanLine;

    /* 2. Map VRAM into process address space (BEFORE set mode - LCF rule) */
    struct minix_mem_range mr;
    mr.mr_base  = info.PhysBasePtr;
    mr.mr_limit = mr.mr_base + vram_size;

    if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
        printf("vg_init: sys_privctl failed\n");
        return NULL;
    }

    video_mem = vm_map_phys(SELF, (void *)(phys_bytes) info.PhysBasePtr,
                            vram_size);
    if (video_mem == MAP_FAILED) {
        printf("vg_init: vm_map_phys failed\n");
        return NULL;
    }

    /* 3. Set VBE mode with Linear Frame Buffer (BIT(14)) */
    reg86_t reg;
    memset(&reg, 0, sizeof(reg));
    reg.intno = 0x10;
    reg.ah    = 0x4F;
    reg.al    = 0x02;
    reg.bx    = mode | BIT(14);

    if (sys_int86(&reg) != OK) {
        printf("vg_init: sys_int86 failed\n");
        return NULL;
    }

    if (reg.ah != 0x00 || reg.al != 0x4F) {
        printf("vg_init: VBE Set Mode failed (AX=0x%04X)\n", reg.ax);
        return NULL;
    }

    return video_mem;
}

/* ── vg_draw_pixel ───────────────────────────────────────────────────────── */

int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= h_res || y >= v_res)
        return 1;

    uint8_t bytes_per_pixel = (bits_per_pixel + 7) / 8;

    uint8_t *pixel = (uint8_t *) video_mem
                     + ((uint32_t) y * h_res + x) * bytes_per_pixel;

    for (uint8_t b = 0; b < bytes_per_pixel; b++, pixel++)
        *pixel = (color >> (8 * b)) & 0xFF;

    return 0;
}

/* ── vg_draw_hline ───────────────────────────────────────────────────────── */
/* Overrides the weak symbol in liblcf.a                                     */

int vg_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
    for (uint16_t col = x; col < x + len; col++)
        if (vg_draw_pixel(col, y, color) != 0)
            return 1;
    return 0;
}

/* ── vg_draw_rectangle ───────────────────────────────────────────────────── */
/* Overrides the weak symbol in liblcf.a                                     */

int vg_draw_rectangle(uint16_t x, uint16_t y, uint16_t width,
                      uint16_t height, uint32_t color) {
    for (uint16_t row = y; row < y + height; row++)
        if (vg_draw_hline(x, row, width, color) != 0)
            return 1;
    return 0;
}

/* ── vg_draw_pixmap ──────────────────────────────────────────────────────── */

int vg_draw_pixmap(uint8_t *pixmap, xpm_image_t img, uint16_t x, uint16_t y) {
    if (pixmap == NULL)
        return 1;

    for (uint16_t row = 0; row < (uint16_t) img.height; row++)
        for (uint16_t col = 0; col < (uint16_t) img.width; col++) {
            uint32_t color = pixmap[(uint32_t) row * img.width + col];
            if (vg_draw_pixel(x + col, y + row, color) != 0)
                return 1;
        }
    return 0;
}

