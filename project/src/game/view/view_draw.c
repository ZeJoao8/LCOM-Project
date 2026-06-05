/**
 * @file view_draw.c
 * @brief Low-level drawing primitives every view module builds on: clipped
 *        rectangles, the 5x7 bitmap font and text helpers, plus small numeric /
 *        colour utilities.
 */
#include <lcom/lcf.h>
#include "game/view_internal.h"
#include "devices/video_gr/video_gr.h"   /* vg_draw_rectangle */

/**
 * @brief Draw a filled rectangle, clipped to the screen bounds.
 * @param x,y   Top-left corner.
 * @param w,h   Size in pixels.
 * @param color Packed colour value.
 */
void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0)      { w += x; x = 0; }
    if (y < 0)      { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    vg_draw_rectangle((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, color);
}

/* 5x7 bitmap font — each row is a 5-bit pattern. */
static const uint8_t font_5x7[][7] = {
    ['0'] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    ['1'] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    ['2'] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    ['3'] = {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
    ['4'] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    ['5'] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    ['6'] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    ['7'] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    ['8'] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    ['9'] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    ['A'] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['B'] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    ['C'] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    ['D'] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    ['E'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    ['F'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    ['G'] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    ['H'] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['I'] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    ['J'] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    ['K'] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    ['L'] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    ['M'] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    ['N'] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    ['O'] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    ['P'] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    ['Q'] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    ['R'] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    ['S'] = {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
    ['T'] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    ['U'] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    ['V'] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    ['W'] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    ['X'] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    ['Y'] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    ['Z'] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    [':'] = {0x00,0x04,0x04,0x00,0x04,0x04,0x00},
    ['!'] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    ['/'] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    ['-'] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
};

/**
 * @brief Draw one glyph of the 5x7 bitmap font, scaled by @p s.
 * @param c     Character to draw (unmapped characters are skipped).
 * @param x,y   Top-left position.
 * @param s     Integer pixel scale.
 * @param color Packed colour value.
 */
void draw_char(char c, int x, int y, int s, uint32_t color) {
    if (c < 0 || (unsigned char)c >= sizeof(font_5x7)/sizeof(font_5x7[0]))
        return;
    const uint8_t *glyph = font_5x7[(unsigned char)c];
    bool empty = true;
    for (int r = 0; r < 7; r++) if (glyph[r]) { empty = false; break; }
    if (empty && c != ' ')
        return; /* no glyph for this character */
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (glyph[row] & (0x10 >> col)) {
                draw_rect(x + col*s, y + row*s, s, s, color);
            }
        }
    }
}

/** @brief Draw a left-aligned string at (x,y), 6*s pixels per glyph. */
void draw_text(const char *str, int x, int y, int s, uint32_t color) {
    while (*str) {
        draw_char(*str, x, y, s, color);
        x += 6 * s; /* 5 wide + 1 space */
        str++;
    }
}

/** @brief Draw a string with a 1-unit drop shadow for legibility over the backdrop. */
void draw_text_sh(const char *str, int x, int y, int s, uint32_t color) {
    draw_text(str, x + s, y + s, s, COL_SHADOW);
    draw_text(str, x,     y,     s, color);
}

/** @brief Draw a string horizontally centred on @p cx. */
void draw_text_c(const char *s, int cx, int y, int scale, uint32_t col) {
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    draw_text(s, cx - n * 6 * scale / 2, y, scale, col);
}

/** @brief Draw a horizontally-centred string with a drop shadow. */
void draw_text_c_sh(const char *s, int cx, int y, int scale, uint32_t col) {
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    draw_text_sh(s, cx - n * 6 * scale / 2, y, scale, col);
}

/**
 * @brief Format integer @p val as a decimal string into @p buf.
 * @param val   Value to convert.
 * @param buf   Destination buffer.
 * @param bufsz Size of @p buf (output is always NUL-terminated).
 */
void int_to_str(int val, char *buf, int bufsz) {
    if (bufsz < 2) return;
    if (val < 0) { buf[0] = '-'; buf++; bufsz--; val = -val; }
    int len = 0;
    char tmp[16];
    do { tmp[len++] = '0' + (val % 10); val /= 10; } while (val > 0 && len < 15);
    for (int i = len - 1; i >= 0 && bufsz > 1; i--) { *buf++ = tmp[i]; bufsz--; }
    *buf = '\0';
}

/** @brief Integer (floor) square root of @p v. @return floor(sqrt(v)). */
int isqrt_i(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

/**
 * @brief Linearly interpolate two 24-bit colours per channel.
 * @param a,b Endpoint colours.
 * @param t   Blend factor in [0,255] (0 = @p a, 255 = @p b).
 * @return The interpolated colour.
 */
uint32_t lerp_col(uint32_t a, uint32_t b, int t) {
    int ra=(a>>16)&0xFF, ga=(a>>8)&0xFF, ba=a&0xFF;
    int rb=(b>>16)&0xFF, gb=(b>>8)&0xFF, bb=b&0xFF;
    int r = ra + (rb - ra) * t / 255;
    int g = ga + (gb - ga) * t / 255;
    int bl= ba + (bb - ba) * t / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

/**
 * @brief Darken the whole back buffer toward black by scaling every colour byte.
 *
 * Works on any direct-colour layout because each channel byte is scaled
 * independently.
 *
 * @param t Surviving brightness: 0 = full black, 255 = unchanged (fast path).
 */
void fade_buffer(int t) {
    if (t >= 255) return;
    if (t < 0) t = 0;
    uint32_t n = (uint32_t) h_res * v_res * ((bits_per_pixel + 7) / 8);
    uint8_t *p = double_buffer;
    for (uint32_t i = 0; i < n; i++)
        p[i] = (uint8_t) ((p[i] * t) / 255);
}
