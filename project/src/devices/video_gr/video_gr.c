/**
 * @file video_gr.c
 * @brief VBE graphics driver implementation. Sets a linear frame-buffer mode,
 *        maps VRAM, and renders into a malloc'd back buffer that is copied to
 *        the screen by ::swap_buffers. Provides pixel/line/rectangle primitives
 *        and a transparency-aware XPM blitter.
 */

#include <lcom/lcf.h>
#include <string.h>
#include <stdlib.h>
#include "devices/video_gr/video_gr.h"

/* Module globals */
void    *video_mem      = NULL;   /**< Mapped physical VRAM.                     */
uint8_t *double_buffer  = NULL;   /**< Off-screen back buffer.                  */
uint16_t h_res          = 0;      /**< Horizontal resolution (pixels).          */
uint16_t v_res          = 0;      /**< Vertical resolution (pixels).            */
uint8_t  bits_per_pixel = 0;      /**< Colour depth (bits per pixel).           */
static uint8_t  bytes_pp        = 0;   /**< Bytes per pixel (derived from depth). */

static uint32_t vram_size = 0;    /**< Size of the frame buffer in bytes.       */

/** @brief Set the VBE mode, map its linear frame buffer, and allocate the back buffer (see video_gr.h). */
void *vg_init(uint16_t mode) {
    /* 1. Query the mode info via vbe_get_mode_info(). */
    vbe_mode_info_t info;
    if (vbe_get_mode_info(mode, &info) != 0) {
        printf("vg_init: vbe_get_mode_info failed\n");
        return NULL;
    }

    h_res          = info.XResolution;
    v_res          = info.YResolution;
    bits_per_pixel = info.BitsPerPixel;
    vram_size      = (uint32_t) v_res * info.BytesPerScanLine;

    /* 2. Map VRAM into our address space (BEFORE setting the mode — LCF rule). */
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

    /* 3. Set the VBE mode with the Linear Frame Buffer bit (BIT(14)). */
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

    bytes_pp = (bits_per_pixel + 7) / 8;
    double_buffer = (uint8_t *) malloc(vram_size);
    if (double_buffer == NULL) {
        printf("vg_init: malloc for double buffer failed\n");
        return NULL;
    }
    memset(double_buffer, 0, vram_size);

    return video_mem;
}

/** @brief Plot one pixel in the back buffer, writing bytes_pp colour bytes (see video_gr.h). */
int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= h_res || y >= v_res)
        return 1;

    uint8_t *pixel = double_buffer
                     + ((uint32_t) y * h_res + x) * bytes_pp;

    for (uint8_t b = 0; b < bytes_pp; b++, pixel++)
        *pixel = (color >> (8 * b)) & 0xFF;

    return 0;
}

/** @brief Draw a horizontal line; overrides the weak LCF symbol (see video_gr.h). */
int vg_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color) {
    for (uint16_t col = x; col < x + len; col++)
        if (vg_draw_pixel(col, y, color) != 0)
            return 1;
    return 0;
}

/** @brief Draw a filled rectangle as stacked hlines; overrides the weak LCF symbol (see video_gr.h). */
int vg_draw_rectangle(uint16_t x, uint16_t y, uint16_t width,
                      uint16_t height, uint32_t color) {
    for (uint16_t row = y; row < y + height; row++)
        if (vg_draw_hline(x, row, width, color) != 0)
            return 1;
    return 0;
}

#define XPM_TRANSPARENT 0xFF00FFu   /**< Colour key treated as fully transparent. */

/**
 * @brief Blit an XPM image into the back buffer with colour-key transparency.
 *
 * The pixmap must be loaded with xpm_load() in a direct-colour format. Pixels
 * equal to ::XPM_TRANSPARENT (magenta) are skipped; pixels falling outside the
 * screen are silently clipped by ::vg_draw_pixel instead of aborting the blit.
 * See video_gr.h for the parameter list.
 */
int vg_draw_pixmap(uint8_t *pixmap, xpm_image_t img, uint16_t x, uint16_t y) {
    if (pixmap == NULL)
        return 1;

    uint8_t bpp;
    switch (img.type) {
        case XPM_INDEXED:               bpp = 1; break;
        case XPM_1_5_5_5:
        case XPM_5_6_5:                 bpp = 2; break;
        case XPM_8_8_8:                 bpp = 3; break;
        case XPM_8_8_8_8:               bpp = 4; break;
        default:                        bpp = bytes_pp; break;
    }

    for (uint16_t row = 0; row < (uint16_t) img.height; row++) {
        for (uint16_t col = 0; col < (uint16_t) img.width; col++) {
            const uint8_t *px = pixmap + ((uint32_t) row * img.width + col) * bpp;
            uint32_t color = 0;
            for (uint8_t b = 0; b < bpp; b++)
                color |= (uint32_t) px[b] << (8 * b);

            if (color == XPM_TRANSPARENT)
                continue;
            /* vg_draw_pixel clips out-of-bounds pixels by returning non-zero */
            vg_draw_pixel((uint16_t)(x + col), (uint16_t)(y + row), color);
        }
    }
    return 0;
}

/** @brief Fill the whole back buffer with @p color. */
void clear_buffer(uint32_t color) {
    uint8_t *p = double_buffer;
    for (uint32_t i = 0; i < (uint32_t)h_res * v_res; i++) {
        for (uint8_t b = 0; b < bytes_pp; b++)
            *p++ = (color >> (8 * b)) & 0xFF;
    }
}

/** @brief Copy the back buffer to VRAM, presenting the rendered frame. */
void swap_buffers(void) {
    memcpy(video_mem, double_buffer, vram_size);
}
