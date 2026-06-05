#ifndef _VIDEO_GR_H_
#define _VIDEO_GR_H_

/**
 * @file video_gr.h
 * @brief Minimal VBE graphics driver with a malloc'd back buffer. Sets a linear
 *        frame-buffer mode, exposes drawing primitives, and provides
 *        double-buffering (draw into ::double_buffer, then ::swap_buffers).
 *
 * The driver overrides the weak LCF symbols (vg_init/vg_draw_hline/
 * vg_draw_rectangle) so the project's own implementations are used.
 */

#include <stdint.h>

/* Include lcf.h BEFORE undefining macros so all types are available */
#include <lcom/lcf.h>

/* LCF defines macros that wrap our functions for tracing/testing.
 * We must undef them here so our own declarations and call sites
 * refer to the real function names, not the macro expansions.      */
#ifdef vg_init
#undef vg_init
#endif
#ifdef vg_draw_hline
#undef vg_draw_hline
#endif
#ifdef vg_draw_rectangle
#undef vg_draw_rectangle
#endif

/* Globals filled by vg_init() */
extern void     *video_mem;       /**< Base of the mapped physical VRAM.        */
extern uint16_t  h_res;           /**< Horizontal resolution in pixels.         */
extern uint16_t  v_res;           /**< Vertical resolution in pixels.           */
extern uint8_t   bits_per_pixel;  /**< Colour depth (bits per pixel).           */

/**
 * @brief Set the given VBE mode, map its linear frame buffer, and allocate the
 *        back buffer. Fills ::h_res, ::v_res, ::bits_per_pixel and ::video_mem.
 * @param mode VBE mode number (the project uses 0x115: 800x600, 24-bit).
 * @return Pointer to the mapped VRAM on success, NULL on failure.
 */
void *vg_init(uint16_t mode);

/**
 * @brief Draw a horizontal line into the back buffer.
 * @param x,y   Top-left pixel of the line.
 * @param len   Length in pixels.
 * @param color Packed colour value.
 * @return 0 on success, non-zero on failure.
 */
int   vg_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color);

/**
 * @brief Draw a filled rectangle into the back buffer (stacked hlines).
 * @param x,y          Top-left corner.
 * @param width,height Rectangle dimensions in pixels.
 * @param color        Packed colour value.
 * @return 0 on success, non-zero on failure.
 */
int   vg_draw_rectangle(uint16_t x, uint16_t y, uint16_t width,
                        uint16_t height, uint32_t color);

/**
 * @brief Plot a single pixel into the back buffer (bounds-checked).
 * @param x,y   Pixel coordinates.
 * @param color Packed colour value.
 * @return 0 on success, non-zero if out of bounds.
 */
int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color);

/**
 * @brief Blit an XPM pixmap into the back buffer at the given position.
 * @param pixmap Decoded pixel data.
 * @param img    XPM image descriptor (width/height/format).
 * @param x,y    Top-left position to draw at.
 * @return 0 on success, non-zero on failure.
 */
int vg_draw_pixmap(uint8_t *pixmap, xpm_image_t img, uint16_t x, uint16_t y);

/* Double buffering */
extern uint8_t *double_buffer;        /**< Off-screen back buffer drawn into.   */

/**
 * @brief Fill the entire back buffer with a single colour.
 * @param color Packed colour value.
 */
void clear_buffer(uint32_t color);

/**
 * @brief Copy the back buffer to VRAM, making the new frame visible.
 */
void swap_buffers(void);

#endif /* _VIDEO_GR_H_ */


