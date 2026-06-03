/**
 * @file view_world.c
 * @brief Drawing of in-play objects: particles, coins' spin frame, zapper beams
 *        and orbs, jetpack flame, power-ups, shield aura and the 2-player ghost.
 */
#include <stdlib.h>               /* abs */
#include "game/view_internal.h"
#include "game/model.h"           /* isin / icos / zap_endpoints */

/* Scrolling ground: solid base, bright lip, plus seams and hazard dashes that
 * slide left at the world speed (g_scroll_x) to sell a sense of motion. */
void draw_floor(void) {
    int top = scr_h - FLOOR_H;
    draw_rect(0, top, scr_w, FLOOR_H, COL_FLOOR);
    draw_rect(0, top, scr_w, 2, COL_ZAP_TIP);          /* bright top lip */

    int off = ((g_scroll_x % FLOOR_TILE) + FLOOR_TILE) % FLOOR_TILE;
    for (int x = -off; x < scr_w; x += FLOOR_TILE) {
        draw_rect(x, top + 2, 2, FLOOR_H - 2, COL_FLOOR_SEAM);      /* tile seam   */
        draw_rect(x + 6, top + 5, FLOOR_TILE / 2, 3, COL_FLOOR_DASH); /* hazard dash */
    }
}

/* Draw all live particles and score popups. */
void draw_particles(void) {
    for (int i = 0; i < MAX_PARTS; i++)
        if (M->parts[i].active)
            draw_rect(M->parts[i].x, M->parts[i].y, 3, 3, M->parts[i].col);
    for (int i = 0; i < MAX_POPUPS; i++)
        if (M->popups[i].active)
            draw_text("+10", M->popups[i].x, M->popups[i].y, 1, COL_SCORE);
}

/* Pick the current coin animation frame from the free-running clock. */
int coin_frame(void) {
    static const int seq[6] = { 0, 1, 2, 3, 2, 1 };
    return seq[(M->anim / 4) % 6];
}

/* Glowing orb at a zapper tip. */
static void draw_orb(int cx, int cy) {
    for (int dy = -ORB_R; dy <= ORB_R; dy++) {
        int hw = isqrt_i(ORB_R * ORB_R - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, COL_ZAP_TIP);     /* shell */
    }
    int ir = ORB_R - 4;
    for (int dy = -ir; dy <= ir; dy++) {
        int hw = isqrt_i(ir * ir - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, COL_ZAP_GLOW);    /* glow */
    }
    draw_rect(cx - 2, cy - 2, 4, 4, COL_TEXT);                   /* centre dot */
}

/* Draw a zapper as an animated electric beam between two glowing orbs. */
void draw_beam(const Zapper *z) {
    int ax, ay, bx, by;
    zap_endpoints(z, &ax, &ay, &bx, &by);

    int steps = z->half / 3;
    if (steps < 2) steps = 2;

    int perpx = icos(z->ang + ANG_VERT);
    int perpy = isin(z->ang + ANG_VERT);

    for (int s = 0; s <= steps; s++) {
        int bxp = ax + (bx - ax) * s / steps;
        int byp = ay + (by - ay) * s / steps;
        int w   = (isin(z->ang + M->anim * 3 + s * 6) * 5) / 1024;  /* ±5px wobble */
        int wx  = bxp + (perpx * w) / 1024;
        int wy  = byp + (perpy * w) / 1024;
        draw_rect(wx - 3, wy - 3, 6, 6, COL_ZAP_BODY);   /* amber glow */
        draw_rect(wx - 2, wy - 2, 4, 4, COL_ZAP_GLOW);   /* bright glow */
        draw_rect(wx - 1, wy - 1, 2, 2, COL_TEXT);       /* white core */
    }

    draw_orb(ax, ay);
    draw_orb(bx, by);
}

/* Layered jetpack flame, anchored at the player's back. */
void draw_flame(int x, int y) {
    int f = 8 + (M->anim % 8);
    draw_rect(x,      y,           10, f,     COL_SCORE);     /* yellow core */
    draw_rect(x + 2,  y + f,        6, f / 2, 0xFF8800);      /* orange */
    draw_rect(x + 3,  y + f + f/2,  4, 3,     COL_GAMEOVER);  /* red tip */
}

/* A floating power-up: a shield or magnet sprite, gently bobbing. Falls back to
 * a glowing diamond tagged S/M if the sprites failed to load. */
void draw_powerup(const PowerUp *pu) {
    int bob = (isin(M->anim * 4) * 3) / 1024;
    uint8_t    *pix = (pu->type == PU_SHIELD) ? shield_pix : magnet_pix;
    xpm_image_t img = (pu->type == PU_SHIELD) ? shield_img : magnet_img;
    if (pix) {
        draw_sprite(pix, img, pu->x, pu->y + bob, 1);
        return;
    }
    uint32_t col = (pu->type == PU_SHIELD) ? COL_SHIELD : COL_MAGNET;
    int cx = pu->x + PU_SIZE / 2;
    int cy = pu->y + PU_SIZE / 2 + bob;
    int r  = PU_SIZE / 2;
    for (int dy = -r; dy <= r; dy++) {              /* diamond */
        int hw = r - abs(dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, col);
    }
    draw_rect(cx - 2, cy - 2, 4, 4, COL_TEXT);      /* bright centre */
    draw_char((pu->type == PU_SHIELD) ? 'S' : 'M', cx - 2, cy - 3, 1, COL_SHADOW);
}

/* Pulsing ring drawn around the player while the shield is active. */
void draw_shield_aura(int px, int py) {
    int cx = px + PLAYER_W / 2, cy = py + PLAYER_H / 2;
    int r  = PLAYER_H / 2 + 6 + (isin(M->anim * 6) * 3) / 1024;
    for (int dy = -r; dy <= r; dy++) {
        int hw = isqrt_i(r * r - dy * dy);
        draw_rect(cx - hw, cy + dy, 1, 1, COL_SHIELD);      /* left arc  */
        draw_rect(cx + hw, cy + dy, 1, 1, COL_SHIELD);      /* right arc */
    }
}

/* Translucent-looking cyan silhouette of the 2-player opponent. */
void draw_ghost(int y) {
    for (int row = 0; row < PLAYER_H; row += 2)        /* scanline fill = "ghosted" */
        draw_rect(PLAYER_X, y + row, PLAYER_W, 1, COL_GHOST);
    draw_rect(PLAYER_X, y, PLAYER_W, 1, COL_TEXT);
    draw_rect(PLAYER_X, y + PLAYER_H - 1, PLAYER_W, 1, COL_TEXT);
    draw_rect(PLAYER_X, y, 1, PLAYER_H, COL_TEXT);
    draw_rect(PLAYER_X + PLAYER_W - 1, y, 1, PLAYER_H, COL_TEXT);
    draw_text("P2", PLAYER_X + 2, y - 12, 1, COL_GHOST);
}
