/**
 * @file view_ui.c
 * @brief UI widgets shared by the menu/instructions/game-over screens: the mouse
 *        cursor and the outlined buttons.
 */
#include "game/view_internal.h"

/* Small mouse crosshair. */
void draw_cursor(void) {
    draw_rect(M->mouse_x - 5, M->mouse_y,     11, 2, COL_SCORE);
    draw_rect(M->mouse_x,     M->mouse_y - 5, 2, 11, COL_SCORE);
}

/* Outlined button with centred label. */
void draw_button(int x, int y, int w, int h, const char *label,
                 int tscale, bool highlight) {
    uint32_t bg  = highlight ? COL_TITLE  : COL_BTN_BG;
    uint32_t txt = highlight ? COL_BTN_HI : COL_TEXT;
    draw_rect(x, y, w, h, bg);
    draw_rect(x,         y,         w, 2, COL_OBS_TOP);
    draw_rect(x,         y + h - 2, w, 2, COL_OBS_TOP);
    draw_rect(x,         y,         2, h, COL_OBS_TOP);
    draw_rect(x + w - 2, y,         2, h, COL_OBS_TOP);
    int llen = 0;
    for (const char *p = label; *p; p++) llen++;
    int lx = x + (w - llen * 6 * tscale) / 2;
    int ly = y + (h - 7 * tscale) / 2;
    draw_text(label, lx, ly, tscale, txt);
}
