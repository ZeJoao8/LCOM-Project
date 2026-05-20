#ifndef _VIDEO_GR_H_
#define _VIDEO_GR_H_

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
extern void     *video_mem;
extern uint16_t  h_res;
extern uint16_t  v_res;
extern uint8_t   bits_per_pixel;

/* These override the weak symbols in liblcf.a */
void *vg_init(uint16_t mode);
int   vg_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color);
int   vg_draw_rectangle(uint16_t x, uint16_t y, uint16_t width,
                        uint16_t height, uint32_t color);

/* Our own helpers - no LCF involvement */
int vg_draw_pixel(uint16_t x, uint16_t y, uint32_t color);
int vg_draw_pixmap(uint8_t *pixmap, xpm_image_t img, uint16_t x, uint16_t y);

/* Double buffering */
extern uint8_t *double_buffer;
void clear_buffer(uint32_t color);
void swap_buffers(void);

#endif /* _VIDEO_GR_H_ */


