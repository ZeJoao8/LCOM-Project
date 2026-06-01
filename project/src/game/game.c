/*
 *  game.c — joguinho estilo Jetpack Joyride para LCOM
 *  Tudo num ficheiro só: jogador, obstáculos, desenho, colisões.
 *  Só usa vg_draw_rectangle — sem dependências XPM.
 */
#include <lcom/lcf.h>
#include <stdlib.h>
#include "game/game.h"
#include "devices/video_gr.h"

/* Scancodes */
#define W_MAKE    0x11
#define W_BREAK   0x91
#define SPACE_MAKE  0x39
#define SPACE_BREAK 0xB9
#define ESC_MAKE  0x01
#define ENTER_MAKE 0x1C

/* Constantes físicas */
#define GRAVITY       2     /* queda mais rápida */
#define THRUST       -5     /* mais impulso para cima */
#define MAX_VY       13     /* velocidade terminal a cair */
#define MIN_VY      -13     /* velocidade terminal a subir */

/* Constantes de layout */
/* O jogador é um sprite nativo 24x40 (sem upscaling). */
#define PLAYER_SCALE  1
#define PLAYER_W     24
#define PLAYER_H     40
#define PLAYER_X     90
#define FLOOR_H      40

/* Zappers — feixe elétrico entre duas orbes (estilo Jetpack Joyride) */
#define ZT_HORIZ   0    /* feixe horizontal parado */
#define ZT_VERT    1    /* feixe vertical parado */
#define ZT_DIAG    2    /* feixe diagonal parado */
#define ZT_SPIN    3    /* a rodar 360° à volta do centro */

#define MAX_ZAPPERS   12
#define ANG_STEPS      64          /* resolução angular (círculo completo) */
#define ANG_VERT       16          /* 90° em passos de ângulo */
#define ANG_DIAG        8          /* 45° */
#define BEAM_MIN_HALF  70          /* metade mínima -> feixes de 140..280px */
#define BEAM_MAX_HALF 140
#define BEAM_THICK     10          /* meia espessura do feixe para colisão */
#define ORB_R          11          /* raio das orbes nas pontas */
#define ZAP_SPIN_SPEED  1          /* passos de ângulo por frame a rodar */
#define ZAPPER_GAP    210          /* espaçamento horizontal entre spawns */
#define COIN_CLEAR     34          /* folga vertical mínima entre moedas e feixe */

/* Moedas */
/* Moeda é um sprite nativo 16x16 (sem upscaling). */
#define MAX_COINS    30
#define COIN_SCALE    1
#define COIN_SIZE    16
#define COIN_STEP    22                /* espaçamento horizontal entre moedas */
#define COIN_FRAMES   4                /* frames da animação de rotação */

/* Mísseis */
/* Sprite do míssil é 20x8 nativo, desenhado a MISSILE_SCALE -> 40x16 no ecrã. */
#define MAX_MISSILES  4
#define MISSILE_SCALE 2
#define MISSILE_H   (8 * MISSILE_SCALE)    /* 16 */
#define MISSILE_W   (20 * MISSILE_SCALE)   /* 40 */
#define MISSILE_WARN  50      /* frames de aviso antes do míssil aparecer */
#define MISSILE_SPEED 16

/* Cores (24-bit, modo 0x115 = 800x600x24) */
#define COL_BG        0x1A1A2E
#define COL_FLOOR     0x16213E
#define COL_PLAYER    0xE94560
#define COL_ZAP_BODY  0xFFCC00  /* corpo amarelo do zapper */
#define COL_ZAP_TIP   0x888888  /* ponta cinzenta do zapper */
#define COL_ZAP_GLOW  0xFFFF88  /* brilho amarelo vivo */
#define COL_MENU_BG   0x0F0F23
#define COL_TITLE     0xE94560
#define COL_TEXT      0xEEEEEE
#define COL_GAMEOVER  0xFF2222
#define COL_SCORE     0xFFCC00
#define COL_STAR      0xFFFFAA
#define COL_COIN      0xFFD700
#define COL_COIN_SHINE 0xFFFF88
#define COL_WARN      0xFF0000  /* vermelho de aviso para mísseis */

/* Estruturas de dados */
typedef struct {
    int y, vy;
    bool thrusting;
} Player;

typedef struct {
    bool active;
    int  type;                 /* ZT_HORIZ, ZT_VERT, ZT_DIAG, ZT_SPIN */
    int  cx, cy;               /* centro do feixe (cx anda para a esquerda) */
    int  half;                 /* metade do comprimento do feixe */
    int  ang;                  /* ângulo atual (0..ANG_STEPS-1) */
    int  spin;                 /* incremento de ângulo por frame (0 = parado) */
} Zapper;

typedef struct {
    int x, y;
    bool active;
} Coin;

typedef struct {
    int y;
    int warning;               /* frames que faltam para o míssil nascer */
    int active;
    int x;
    int speed;
} Missile;

/* Juice: partículas e popups de pontuação */
#define MAX_PARTS    48
#define MAX_POPUPS    6
typedef struct {
    int x, y, vx, vy, life, active;
    uint32_t col;
} Particle;
typedef struct {
    int x, y, life, active;
} Popup;

/* Estado do módulo */
static game_state_t state = STATE_MENU;
static Player player;
static Zapper zaps[MAX_ZAPPERS];
static Coin coins[MAX_COINS];
static Missile missiles[MAX_MISSILES];
static Particle parts[MAX_PARTS];
static Popup    popups[MAX_POPUPS];
static int      hit_flash = 0;

static int  scroll_speed = 3;
static int  score        = 0;
static int  coins_collected = 0;
static int  frame_count  = 0;
static int  spawn_timer  = 0;
static int  missile_timer = 0;
static int  difficulty_timer = 0;

static int  scr_w, scr_h;

/* Sprites (XPM, carregados uma vez) */
static bool        sprites_ok = false;
static uint8_t    *player_pix = NULL;
static xpm_image_t player_img;
static uint8_t    *coin_pix[COIN_FRAMES] = { NULL };
static xpm_image_t coin_img[COIN_FRAMES];
static uint8_t    *missile_pix = NULL;
static xpm_image_t missile_img;

static void reset_game(void);
static void spawn_zapper(void);
static void spawn_coins(int cx, int band_lo, int band_hi, int pattern);
static void load_sprites(void);

/* Trigonometria inteira (sin/cos escalados por 1024, ANG_STEPS por volta) */
static const int sin1024[ANG_STEPS] = {
        0,   100,   200,   297,   392,   483,   569,   650,
      724,   792,   851,   903,   946,   980,  1004,  1019,
     1024,  1019,  1004,   980,   946,   903,   851,   792,
      724,   650,   569,   483,   392,   297,   200,   100,
        0,  -100,  -200,  -297,  -392,  -483,  -569,  -650,
     -724,  -792,  -851,  -903,  -946,  -980, -1004, -1019,
    -1024, -1019, -1004,  -980,  -946,  -903,  -851,  -792,
     -724,  -650,  -569,  -483,  -392,  -297,  -200,  -100,
};
static int isin(int a) { return sin1024[((a % ANG_STEPS) + ANG_STEPS) % ANG_STEPS]; }
static int icos(int a) { return isin(a + ANG_VERT); }

/* Devolve as duas pontas de um feixe zapper em coordenadas do ecrã. */
static void zap_endpoints(const Zapper *z, int *ax, int *ay, int *bx, int *by) {
    int dx = (z->half * icos(z->ang)) / 1024;
    int dy = (z->half * isin(z->ang)) / 1024;
    *ax = z->cx - dx; *ay = z->cy - dy;
    *bx = z->cx + dx; *by = z->cy + dy;
}

/* Distância ao quadrado do ponto P ao segmento A-B (matemática inteira). */
static long pt_seg_d2(int px, int py, int ax, int ay, int bx, int by) {
    long abx = bx - ax, aby = by - ay;
    long apx = px - ax, apy = py - ay;
    long len2 = abx * abx + aby * aby;
    long t = (len2 > 0) ? (apx * abx + apy * aby) : 0;
    if (t < 0)        t = 0;
    else if (t > len2) t = len2;
    long qx = ax + (len2 > 0 ? (abx * t) / len2 : 0);
    long qy = ay + (len2 > 0 ? (aby * t) / len2 : 0);
    long dx = px - qx, dy = py - qy;
    return dx * dx + dy * dy;
}

/* Verdadeiro se o jogador colide com o feixe (amostrado ao longo do corpo). */
static bool zap_hits_player(const Zapper *z) {
    int ax, ay, bx, by;
    zap_endpoints(z, &ax, &ay, &bx, &by);
    int pcx = PLAYER_X + PLAYER_W / 2;
    long thr = BEAM_THICK + PLAYER_W / 2;
    long thr2 = thr * thr;
    for (int k = 1; k <= 3; k++) {
        int py = player.y + (PLAYER_H * k) / 4;
        if (pt_seg_d2(pcx, py, ax, ay, bx, by) < thr2)
            return true;
    }
    return false;
}

/* Partículas e popups (só lógica; o desenho está junto dos outros draws) */
static void spawn_particle(int x, int y, int vx, int vy, int life, uint32_t col) {
    for (int i = 0; i < MAX_PARTS; i++) {
        if (!parts[i].active) {
            parts[i].x = x; parts[i].y = y;
            parts[i].vx = vx; parts[i].vy = vy;
            parts[i].life = life; parts[i].active = 1; parts[i].col = col;
            return;
        }
    }
}
static void spawn_popup(int x, int y) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!popups[i].active) {
            popups[i].x = x; popups[i].y = y;
            popups[i].life = 36; popups[i].active = 1;
            return;
        }
    }
}
static void burst(int x, int y, int n, uint32_t col) {
    for (int i = 0; i < n; i++)
        spawn_particle(x, y, (rand() % 13) - 6, (rand() % 13) - 8,
                       14 + (rand() % 12), col);
}
static void update_particles(void) {
    for (int i = 0; i < MAX_PARTS; i++) {
        if (!parts[i].active) continue;
        parts[i].x += parts[i].vx;
        parts[i].y += parts[i].vy;
        parts[i].vy += 1;                      /* gravidade */
        if (--parts[i].life <= 0) parts[i].active = 0;
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!popups[i].active) continue;
        popups[i].y -= 1;
        if (--popups[i].life <= 0) popups[i].active = 0;
    }
    if (hit_flash > 0) hit_flash--;
}

/* Morte do jogador: explosão + flash vermelho, depois game over. */
static void player_explode(void) {
    int cx = PLAYER_X + PLAYER_W / 2, cy = player.y + PLAYER_H / 2;
    burst(cx, cy, 24, COL_GAMEOVER);
    burst(cx, cy, 12, COL_SCORE);
    burst(cx, cy,  8, 0xFF8800);
    hit_flash = 8;
    state = STATE_GAMEOVER;
}

/* API pública */

game_state_t game_get_state(void) { return state; }

void game_init(void) {
    scr_w = h_res;
    scr_h = v_res;
    state  = STATE_MENU;
    srand(42); /* semente fixa, dá para o jogo */
    load_sprites();
}

void game_cleanup(void) {
    /* não há nada para libertar — é tudo estático */
}

/* Reset para uma nova ronda */
static void reset_game(void) {
    player.y  = scr_h / 2;
    player.vy = 0;
    player.thrusting = false;

    for (int i = 0; i < MAX_ZAPPERS; i++) zaps[i].active = false;
    for (int i = 0; i < MAX_COINS; i++) coins[i].active = false;
    for (int i = 0; i < MAX_MISSILES; i++) missiles[i].active = 0;
    for (int i = 0; i < MAX_PARTS; i++) parts[i].active = 0;
    for (int i = 0; i < MAX_POPUPS; i++) popups[i].active = 0;
    hit_flash = 0;

    scroll_speed = 4;
    score = 0;
    coins_collected = 0;
    frame_count = 0;
    spawn_timer = 0;
    missile_timer = 0;
    difficulty_timer = 0;
}

/* Gera um zapper de feixe único a partir do lado direito */
static void spawn_zapper(void) {
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (zaps[i].active) continue;

        Zapper *z = &zaps[i];
        int pf = scr_h - FLOOR_H;                 /* altura do campo de jogo */

        z->active = true;
        z->type   = rand() % 4;
        z->half   = BEAM_MIN_HALF + (rand() % (BEAM_MAX_HALF - BEAM_MIN_HALF + 1));

        switch (z->type) {
            case ZT_HORIZ: z->ang = 0;        z->spin = 0; break;
            case ZT_VERT:  z->ang = ANG_VERT; z->spin = 0; break;
            case ZT_DIAG:  z->ang = (rand() % 2) ? ANG_DIAG : (ANG_STEPS - ANG_DIAG);
                           z->spin = 0; break;
            default:       z->ang = rand() % ANG_STEPS; z->spin = ZAP_SPIN_SPEED;
                           z->half = 60 + (rand() % 50);   /* smaller sweep, fairer */
                           break;
        }

        /* Extensão vertical (pior caso) — para manter um corredor praticável. */
        int vh = (z->type == ZT_HORIZ) ? BEAM_THICK : z->half;
        int lo = vh + 20, hi = pf - vh - 20;
        if (hi < lo) hi = lo;
        z->cy = lo + (rand() % (hi - lo + 1));
        z->cx = scr_w + z->half + 20;             /* feixe todo começa fora do ecrã */

        /* Larga uma fila de moedas no corredor mais largo, longe do feixe. */
        int topgap = z->cy - vh;
        int botgap = pf - (z->cy + vh);
        int band_lo, band_hi;
        if (topgap >= botgap) { band_lo = 6;                  band_hi = z->cy - vh - COIN_CLEAR; }
        else                  { band_lo = z->cy + vh + COIN_CLEAR; band_hi = pf - 6; }
        spawn_coins(z->cx + z->half + 30, band_lo, band_hi, rand() % 4);
        return;
    }
}

/* Gera uma fila curta de moedas dentro de uma faixa vertical segura [lo, hi] */
static void spawn_coins(int cx, int band_lo, int band_hi, int pattern) {
    if (band_hi - band_lo < COIN_SIZE) return;        /* corredor muito apertado */

    int cy    = (band_lo + band_hi) / 2;
    int count = 3 + (rand() % 4);                      /* 3-6 moedas */
    int span  = (count - 1) * COIN_STEP;
    int placed = 0;
    for (int i = 0; i < MAX_COINS && placed < count; i++) {
        if (coins[i].active) continue;
        coins[i].active = true;

        int off = placed * COIN_STEP;
        int d   = off - span / 2;                 /* distância do centro da fila */
        int x   = cx + off;
        int y   = cy - COIN_SIZE / 2;             /* centra a moeda no cy */

        switch (pattern) {
            case 0: y -= 20 - abs(d) / 4; break;  /* arco para cima (∩) */
            case 1: y += 20 - abs(d) / 4; break;  /* arco para baixo (∪) */
            case 2: break;                        /* linha reta */
            case 3: y += (placed % 2) ? 12 : -12; break; /* ziguezague */
        }
        /* mantém a moeda toda dentro da faixa segura */
        if (y < band_lo)              y = band_lo;
        if (y > band_hi - COIN_SIZE)  y = band_hi - COIN_SIZE;

        coins[i].x = x;
        coins[i].y = y;
        placed++;
    }
}

/* UPDATE (chamado a cada tick do timer) */

void game_update(void) {
    update_particles();                 /* manter efeitos vivos mesmo com game over */
    if (state != STATE_PLAYING) return;

    frame_count++;

    /* Física do jogador */
    if (player.thrusting) player.vy += THRUST;
    else player.vy += GRAVITY;

    if (player.vy > MAX_VY)  player.vy = MAX_VY;
    if (player.vy < MIN_VY)  player.vy = MIN_VY;
    player.y += player.vy;

    if (player.y < 0) { player.y = 0; player.vy = 0; }
    int floor_top = scr_h - FLOOR_H - PLAYER_H;
    if (player.y > floor_top) { player.y = floor_top; player.vy = 0; }

    /* Partículas de escape do jetpack enquanto acelera */
    if (player.thrusting) {
        spawn_particle(PLAYER_X + 2, player.y + 34, -(2 + rand() % 3),
                       1 + rand() % 2, 8 + rand() % 6,
                       (rand() % 2) ? COL_SCORE : 0xFF8800);
    }

    /* Mover e rodar zappers */
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!zaps[i].active) continue;
        zaps[i].cx -= scroll_speed;
        if (zaps[i].spin)
            zaps[i].ang = (zaps[i].ang + zaps[i].spin) % ANG_STEPS;
        if (zaps[i].cx + zaps[i].half < -20) zaps[i].active = false;
    }

    /* Mover moedas; eliminar as que chegarem muito perto de um feixe */
    long coin_safe2 = (long)(BEAM_THICK + COIN_SIZE / 2 + 18) *
                      (BEAM_THICK + COIN_SIZE / 2 + 18);
    for (int i = 0; i < MAX_COINS; i++) {
        if (!coins[i].active) continue;
        coins[i].x -= scroll_speed;
        if (coins[i].x + COIN_SIZE < 0) { coins[i].active = false; continue; }

        int ccx = coins[i].x + COIN_SIZE / 2;
        int ccy = coins[i].y + COIN_SIZE / 2;
        for (int j = 0; j < MAX_ZAPPERS; j++) {
            if (!zaps[j].active) continue;
            int ax, ay, bx, by;
            zap_endpoints(&zaps[j], &ax, &ay, &bx, &by);
            if (pt_seg_d2(ccx, ccy, ax, ay, bx, by) < coin_safe2) {
                coins[i].active = false;
                break;
            }
        }
    }

    /* Mísseis */
    for (int i = 0; i < MAX_MISSILES; i++) {
        Missile *m = &missiles[i];
        if (m->warning > 0) {
            m->warning--;
            if (m->warning == 0) {
                m->active = 1;
                m->x = scr_w;
                m->y = 30 + (rand() % (scr_h - FLOOR_H - 60));
                m->speed = MISSILE_SPEED + (scroll_speed / 2);
            }
        }
        if (m->active) {
            m->x -= m->speed;
            if (m->x + MISSILE_W < 0) m->active = 0;
        }
    }

    missile_timer++;
    if (missile_timer >= MISSILE_WARN * 2) {
        missile_timer = 0;
        for (int i = 0; i < MAX_MISSILES; i++) {
            if (missiles[i].warning == 0 && !missiles[i].active) {
                missiles[i].warning = MISSILE_WARN;
                missiles[i].y = 30 + (rand() % (scr_h - FLOOR_H - 60));
                break;
            }
        }
    }

    /* Lógica de spawn */
    spawn_timer++;
    int spawn_interval = ZAPPER_GAP / scroll_speed;
    if (spawn_interval < 20) spawn_interval = 20;
    if (spawn_timer >= spawn_interval) {
        spawn_zapper();
        spawn_timer = 0;
    }

    /* Recolha de moedas */
    for (int i = 0; i < MAX_COINS; i++) {
        if (!coins[i].active) continue;
        if (PLAYER_X + PLAYER_W > coins[i].x &&
            PLAYER_X < coins[i].x + COIN_SIZE &&
            player.y + PLAYER_H > coins[i].y &&
            player.y < coins[i].y + COIN_SIZE) {
            coins[i].active = false;
            coins_collected++;
            /* brilho + popup "+10" */
            int ccx = coins[i].x + COIN_SIZE / 2, ccy = coins[i].y + COIN_SIZE / 2;
            for (int p = 0; p < 5; p++)
                spawn_particle(ccx, ccy, (rand() % 9) - 4, (rand() % 7) - 5,
                               10 + rand() % 8, COL_COIN_SHINE);
            spawn_popup(coins[i].x, coins[i].y - 6);
        }
    }

    /* Pontuação */
    score = (frame_count / 6) + coins_collected * 10;

    /* Aumento de dificuldade */
    difficulty_timer++;
    if (difficulty_timer >= 220 && scroll_speed < 14) {
        scroll_speed++;
        difficulty_timer = 0;
    }

    /* Colisão: zappers (distância ponto-feixe) */
    for (int i = 0; i < MAX_ZAPPERS; i++) {
        if (!zaps[i].active) continue;
        if (zap_hits_player(&zaps[i])) { player_explode(); return; }
    }

    /* Colisão: mísseis */
    for (int i = 0; i < MAX_MISSILES; i++) {
        if (!missiles[i].active) continue;
        Missile *m = &missiles[i];
        if (PLAYER_X + PLAYER_W > m->x &&
            PLAYER_X < m->x + MISSILE_W &&
            player.y + PLAYER_H > m->y &&
            player.y < m->y + MISSILE_H)
            { player_explode(); return; }
    }
}

/* DRAW (chamado a cada tick do timer) */

/* Desenha um retângulo preenchido, limitado ao ecrã */
static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0)      { w += x; x = 0; }
    if (y < 0)      { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    vg_draw_rectangle((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, color);
}

/* Desenha letras em bloco simples — cada carácter é 5x7 pixéis, escalado por s */
static void draw_char(char c, int x, int y, int s, uint32_t color);
static void draw_text(const char *str, int x, int y, int s, uint32_t color) {
    while (*str) {
        draw_char(*str, x, y, s, color);
        x += 6 * s; /* 5 de largura + 1 de espaço */
        str++;
    }
}

/* Fonte pequena 5x7 — cada linha é um padrão de 5 bits */
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
};

static void draw_char(char c, int x, int y, int s, uint32_t color) {
    if (c < 0 || (unsigned char)c >= sizeof(font_5x7)/sizeof(font_5x7[0]))
        return;
    const uint8_t *glyph = font_5x7[(unsigned char)c];
    if (glyph[0] == 0 && glyph[1] == 0 && glyph[6] == 0 && c != ' ')
        return; /* sem glifo para este carácter */
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (glyph[row] & (0x10 >> col)) {
                draw_rect(x + col*s, y + row*s, s, s, color);
            }
        }
    }
}

/* Converte um inteiro para uma string */
static void int_to_str(int val, char *buf, int bufsz) {
    if (bufsz < 2) return;
    if (val < 0) { buf[0] = '-'; buf++; bufsz--; val = -val; }
    int len = 0;
    char tmp[16];
    do { tmp[len++] = '0' + (val % 10); val /= 10; } while (val > 0 && len < 15);
    for (int i = len - 1; i >= 0 && bufsz > 1; i--) { *buf++ = tmp[i]; bufsz--; }
    *buf = '\0';
}

/* SPRITES (XPM) */
/* XPMs de 24-bit. '.' = #FF00FF é a cor de transparência tratada pela vg_draw_pixmap. */

/* Jogador 24x40 e moedas 16x16 — sprites nativos (sem upscaling).
 * '.' = #FF00FF é a cor de transparência tratada pela vg_draw_pixmap. */
static const char *player_xpm[] = {
    "24 40 13 1",
    ". c #FF00FF",
    "o c #15151F",
    "h c #E8ECF2",
    "H c #AEB8C6",
    "v c #3CC8FF",
    "V c #A6EBFF",
    "s c #FFCC99",
    "r c #E94560",
    "k c #A8243E",
    "g c #9AA6B2",
    "d c #5A6470",
    "y c #FFCC00",
    "b c #2B2B3A",
    "........................",
    "........................",
    ".........ooooooo........",
    ".......ooohhhhhooo......",
    "......oohhhhhhhHHoo.....",
    "......ohhhhhhhhHHHo.....",
    ".....oohhhhhhhhHHHoo....",
    ".....ohhhhhhhhhHHHHo....",
    ".....ohhhhVVvvvvHHHo....",
    ".....ohhhvvvvvvvvHHo....",
    ".....ohhhvvvvvvvvHHo....",
    ".....ohhhvvvvvvvvHHo....",
    ".....oohhhvvvvvvHHoo....",
    "......ohhhhhhhhHHHo.....",
    ".ooooooohhhhhhhHHooo....",
    ".oddgggrrrhhhhhrrrro....",
    ".oddgggrrrrrrrrrkkko....",
    ".odyyyyrrrrrrrrrkkko....",
    ".odyyyyrrrrrrrrrkkkoo...",
    ".odyyyyrrrrrrrrrkrrro...",
    ".oddgggrrrrrrrrrkrrro...",
    ".oddgggrrrrrrrrrkrrro...",
    ".oddgggrrrrrrrrrkrrro...",
    ".oddgggrrrrrrrrrkrrro...",
    ".oddgggrrrrrrrrrkssso...",
    ".oddgggrrrrrrrrrkssso...",
    ".oddgggrrrrrrrrrkkkoo...",
    ".oddgggrrrrrrrrrkkko....",
    ".oddgggyyyyyyyyyyyyo....",
    ".oddgggyyyyyyyyyyyyo....",
    ".oddgggrrrrrrrrrrrro....",
    ".oddgggorrrroorrrroo....",
    ".odddddorrrroorrrro.....",
    ".odddddorrrroorrrro.....",
    ".odddddorrrroorrrro.....",
    ".ooooooorrrroorrrro.....",
    ".......orrrroorrrro.....",
    ".......obbbboobbbbo.....",
    ".......obbbboobbbbo.....",
    ".......obbbboobbbbo.....",
};

static const char *coin_xpm0[] = {
    "16 16 5 1",
    ". c #FF00FF",
    "O c #B8860B",
    "Y c #FFD700",
    "W c #FFF6A8",
    "D c #D4A017",
    "................",
    ".....OOOOOO.....",
    "....OOOOOOOO....",
    "...OOYYYYYYOO...",
    "..OOYWWYYYYYOO..",
    ".OOYWWWWYYYYYOO.",
    ".OOYWWWWYYYYYOO.",
    ".OOYYWWYYYYYYOO.",
    ".OOYYYYYYYYYYOO.",
    ".OOYYYYYYDDDYOO.",
    ".OOYYYYYDDDDYOO.",
    "..OOYYYYYDDDOO..",
    "...OOYYYYYYOO...",
    "....OOOOOOOO....",
    ".....OOOOOO.....",
    "................",
};

static const char *coin_xpm1[] = {
    "16 16 5 1",
    ". c #FF00FF",
    "O c #B8860B",
    "Y c #FFD700",
    "W c #FFF6A8",
    "D c #D4A017",
    "................",
    "......OOOO......",
    ".....OOOOOO.....",
    ".....OYYYYO.....",
    "....OOWYYYOO....",
    "....OWWWYYYO....",
    "....OWWWYYYO....",
    "....OWWYYYYO....",
    "....OYYYYYYO....",
    "....OYYYYDDO....",
    "....OYYYDDDO....",
    "....OOYYYDOO....",
    ".....OYYYYO.....",
    ".....OOOOOO.....",
    "......OOOO......",
    "................",
};

static const char *coin_xpm2[] = {
    "16 16 3 1",
    ". c #FF00FF",
    "O c #B8860B",
    "Y c #FFD700",
    "................",
    ".......OO.......",
    ".......OO.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......YY.......",
    ".......OO.......",
    ".......OO.......",
    "................",
};

static const char *coin_xpm3[] = {
    "16 16 5 1",
    ". c #FF00FF",
    "O c #B8860B",
    "Y c #FFD700",
    "W c #FFF6A8",
    "D c #D4A017",
    "................",
    "......OOOO......",
    ".....OOOOOO.....",
    ".....OYYYYO.....",
    "....OOYYYWOO....",
    "....OYYYWWWO....",
    "....OYYYWWWO....",
    "....OYYYYWWO....",
    "....OYYYYYYO....",
    "....OYYYYDDO....",
    "....OYYYDDDO....",
    "....OOYYYDOO....",
    ".....OYYYYO.....",
    ".....OOOOOO.....",
    "......OOOO......",
    "................",
};

/* Míssil — foguete vermelho a apontar para a esquerda, rasto para a direita. 20x8 nativo. */
static const char *missile_xpm[] = {
    "20 8 6 1",
    ". c #FF00FF",
    "o c #551111",
    "r c #E03030",
    "w c #FFFFFF",
    "y c #FFE000",
    "a c #FF7711",
    "..............oo....",
    "..........rrrrro....",
    "..orrrrrrrrrrrwoa...",
    "orrrrrrrrrrrrrwoayay",
    "orrrrrrrrrrrrrwoayay",
    "..orrrrrrrrrrrwoa...",
    "..........rrrrro....",
    "..............oo....",
};

static void load_sprites(void) {
    player_pix  = xpm_load((xpm_map_t) player_xpm,  XPM_8_8_8, &player_img);
    coin_pix[0] = xpm_load((xpm_map_t) coin_xpm0,   XPM_8_8_8, &coin_img[0]);
    coin_pix[1] = xpm_load((xpm_map_t) coin_xpm1,   XPM_8_8_8, &coin_img[1]);
    coin_pix[2] = xpm_load((xpm_map_t) coin_xpm2,   XPM_8_8_8, &coin_img[2]);
    coin_pix[3] = xpm_load((xpm_map_t) coin_xpm3,   XPM_8_8_8, &coin_img[3]);
    missile_pix = xpm_load((xpm_map_t) missile_xpm, XPM_8_8_8, &missile_img);

    sprites_ok = (player_pix && missile_pix &&
                  coin_pix[0] && coin_pix[1] && coin_pix[2] && coin_pix[3]);
}

/* Helpers para desenhar sprites */

static uint8_t xpm_bpp(xpm_image_t img) {
    switch (img.type) {
        case XPM_INDEXED:               return 1;
        case XPM_1_5_5_5:
        case XPM_5_6_5:                 return 2;
        case XPM_8_8_8:                 return 3;
        case XPM_8_8_8_8:               return 4;
        default:                        return 3;
    }
}

/* Desenha um XPM carregado com escala inteira s. s==1 usa vg_draw_pixmap;
 * escalas maiores expandem cada pixel fonte num bloco s*s (draw_rect faz clip). */
static void draw_sprite(uint8_t *pix, xpm_image_t img, int x, int y, int s) {
    if (pix == NULL) return;
    if (s <= 1) {
        vg_draw_pixmap(pix, img, (uint16_t) x, (uint16_t) y);
        return;
    }
    uint8_t bpp = xpm_bpp(img);
    for (int row = 0; row < (int) img.height; row++) {
        for (int col = 0; col < (int) img.width; col++) {
            const uint8_t *p = pix + ((uint32_t) row * img.width + col) * bpp;
            uint32_t color = 0;
            for (uint8_t b = 0; b < bpp; b++)
                color |= (uint32_t) p[b] << (8 * b);
            if (color == 0xFF00FFu) continue;          /* cor transparente */
            draw_rect(x + col * s, y + row * s, s, s, color);
        }
    }
}

/* Desenha um sprite nativo (escala 1) com cisalhamento horizontal por linha,
 * simulando a inclinação do jogador a subir ou descer. slope está em 1/256 px-por-linha. */
static void draw_sprite_tilt(uint8_t *pix, xpm_image_t img, int x, int y, int slope) {
    if (pix == NULL) return;
    uint8_t bpp = xpm_bpp(img);
    int cy = img.height / 2;
    for (int row = 0; row < (int) img.height; row++) {
        int xo = (slope * (row - cy)) >> 8;
        for (int col = 0; col < (int) img.width; col++) {
            const uint8_t *p = pix + ((uint32_t) row * img.width + col) * bpp;
            uint32_t color = 0;
            for (uint8_t b = 0; b < bpp; b++)
                color |= (uint32_t) p[b] << (8 * b);
            if (color == 0xFF00FFu) continue;
            draw_rect(x + col + xo, y + row, 1, 1, color);
        }
    }
}

/* Desenha todas as partículas vivas e popups de pontuação. */
static void draw_particles(void) {
    for (int i = 0; i < MAX_PARTS; i++)
        if (parts[i].active)
            draw_rect(parts[i].x, parts[i].y, 3, 3, parts[i].col);
    for (int i = 0; i < MAX_POPUPS; i++)
        if (popups[i].active)
            draw_text("+10", popups[i].x, popups[i].y, 1, COL_SCORE);
}

/* Escolhe o frame atual da animação da moeda a partir do contador global. */
static int coin_frame(void) {
    static const int seq[6] = { 0, 1, 2, 3, 2, 1 };
    return seq[(frame_count / 4) % 6];
}

static int isqrt_i(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

/* Orbe brilhante na ponta de um zapper. */
static void draw_orb(int cx, int cy) {
    for (int dy = -ORB_R; dy <= ORB_R; dy++) {
        int hw = isqrt_i(ORB_R * ORB_R - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, COL_ZAP_TIP);     /* invólucro */
    }
    int ir = ORB_R - 4;
    for (int dy = -ir; dy <= ir; dy++) {
        int hw = isqrt_i(ir * ir - dy * dy);
        draw_rect(cx - hw, cy + dy, 2 * hw, 1, COL_ZAP_GLOW);    /* brilho */
    }
    draw_rect(cx - 2, cy - 2, 4, 4, COL_TEXT);                   /* ponto central */
}

/* Desenha um zapper como um feixe elétrico animado entre duas orbes brilhantes. */
static void draw_beam(const Zapper *z) {
    int ax, ay, bx, by;
    zap_endpoints(z, &ax, &ay, &bx, &by);

    int steps = z->half / 3;
    if (steps < 2) steps = 2;

    /* Perpendicular unitária (*1024) para a oscilação elétrica lateral. */
    int perpx = icos(z->ang + ANG_VERT);
    int perpy = isin(z->ang + ANG_VERT);

    for (int s = 0; s <= steps; s++) {
        int bxp = ax + (bx - ax) * s / steps;
        int byp = ay + (by - ay) * s / steps;
        int w   = (isin(z->ang + frame_count * 3 + s * 6) * 5) / 1024;  /* ±5px de oscilação */
        int wx  = bxp + (perpx * w) / 1024;
        int wy  = byp + (perpy * w) / 1024;
        draw_rect(wx - 3, wy - 3, 6, 6, COL_ZAP_BODY);   /* brilho âmbar */
        draw_rect(wx - 2, wy - 2, 4, 4, COL_ZAP_GLOW);   /* brilho forte */
        draw_rect(wx - 1, wy - 1, 2, 2, COL_TEXT);       /* núcleo branco */
    }

    draw_orb(ax, ay);
    draw_orb(bx, by);
}

/* Chama do jetpack em camadas, ancorada nas costas do jogador. */
static void draw_flame(int x, int y) {
    int f = 8 + (frame_count % 8);
    draw_rect(x,      y,         10, f,     COL_SCORE);      /* núcleo amarelo */
    draw_rect(x + 2,  y + f,      6, f / 2, 0xFF8800);       /* laranja */
    draw_rect(x + 3,  y + f + f/2, 4, 3,    COL_GAMEOVER);   /* ponta vermelha */
}

void game_draw(void) {
    if (state == STATE_MENU) {
        /* ECRÃ DE MENU */
        clear_buffer(COL_MENU_BG);

        /* Estrelas decorativas */
        for (int i = 0; i < 50; i++) {
            int sx = (i * 97 + frame_count * (i % 3 + 1)) % scr_w;
            int sy = (i * 73) % (scr_h - 100);
            draw_rect(sx, sy, 2, 2, COL_STAR);
        }

        /* Título */
        draw_text("JETPACK JOYRIDE", scr_w/2 - 15*6*3/2, scr_h/4, 3, COL_TITLE);

        /* Subtítulo */
        draw_text("LCOM EDITION", scr_w/2 - 12*6*2/2, scr_h/4 + 40, 2, COL_TEXT);

        /* Instruções */
        draw_text("PRESS W OR SPACE TO START", scr_w/2 - 25*6*1/2, scr_h/2 + 40, 1, COL_SCORE);
        draw_text("W TO FLY  ESC TO QUIT", scr_w/2 - 21*6*1/2, scr_h/2 + 70, 1, COL_TEXT);

        /* Pré-visualização animada do jogador (com chama do jetpack parado) */
        int preview_y = scr_h/2 - 30 + ((frame_count / 8) % 2 == 0 ? -8 : 8);
        draw_flame(scr_w/2 - PLAYER_W/2 + 1, preview_y + 33);
        if (sprites_ok)
            draw_sprite(player_pix, player_img,
                        scr_w/2 - PLAYER_W/2, preview_y, PLAYER_SCALE);
        else
            draw_rect(scr_w/2 - PLAYER_W/2, preview_y, PLAYER_W, PLAYER_H, COL_PLAYER);

        frame_count++;
    }
    else if (state == STATE_PLAYING) {
        /* ECRÃ DE JOGO */
        clear_buffer(COL_BG);

        /* Chão */
        draw_rect(0, scr_h - FLOOR_H, scr_w, FLOOR_H, COL_FLOOR);

        /* Linha do chão */
        draw_rect(0, scr_h - FLOOR_H, scr_w, 2, COL_ZAP_TIP);

        /* Partículas de fundo (pontos a scrollar tipo parallax) */
        for (int i = 0; i < 30; i++) {
            int px = (i * 137 - frame_count * 1) % scr_w;
            if (px < 0) px += scr_w;
            int py = (i * 53) % (scr_h - FLOOR_H);
            draw_rect(px, py, 1, 1, 0x333355);
        }

        /* Zappers (feixes elétricos) */
        for (int i = 0; i < MAX_ZAPPERS; i++) {
            if (zaps[i].active) draw_beam(&zaps[i]);
        }

        /* Moedas (animação de rotação) */
        int cf = coin_frame();
        for (int i = 0; i < MAX_COINS; i++) {
            if (!coins[i].active) continue;
            if (sprites_ok) {
                draw_sprite(coin_pix[cf], coin_img[cf],
                            coins[i].x, coins[i].y, COIN_SCALE);
            } else {
                /* fallback: forma de diamante */
                int cx = coins[i].x + COIN_SIZE/2;
                int cy = coins[i].y + COIN_SIZE/2;
                int r = COIN_SIZE/2;
                for (int dy = -r; dy <= r; dy++) {
                    int half_w = r - abs(dy);
                    draw_rect(cx - half_w, cy + dy, half_w * 2, 1, COL_COIN);
                    if (dy > -r && dy < r)
                        draw_rect(cx - half_w + 1, cy + dy, half_w * 2 - 2, 1, COL_COIN_SHINE);
                }
            }
        }

        /* Mísseis + avisos */
        for (int i = 0; i < MAX_MISSILES; i++) {
            Missile *m = &missiles[i];
            if (m->warning > 0) {
                /* Seta a piscar no lado direito, a apontar para dentro */
                int blink = (m->warning / 5) % 2;
                if (blink) {
                    int ay = m->y + MISSILE_H / 2;
                    for (int t = 0; t < 10; t++)               /* seta ◄ */
                        draw_rect(scr_w - 16 + t, ay - t, 2, 2 * t + 1, COL_WARN);
                    draw_text("!", scr_w - 34, m->y, 2, COL_WARN);
                }
            }
            if (m->active) {
                if (sprites_ok)
                    draw_sprite(missile_pix, missile_img, m->x, m->y, MISSILE_SCALE);
                else {
                    draw_rect(m->x, m->y, MISSILE_W, MISSILE_H, COL_GAMEOVER);
                    draw_rect(m->x + MISSILE_W - 6, m->y - 2, 6, MISSILE_H + 4, COL_SCORE);
                }
            }
        }

        /* Chama do jetpack (desenhada atrás do jogador, a sair do motor) */
        if (player.thrusting)
            draw_flame(PLAYER_X + 1, player.y + 33);

        /* Jogador — inclina para cima/baixo conforme a velocidade vertical */
        if (sprites_ok)
            draw_sprite_tilt(player_pix, player_img, PLAYER_X, player.y,
                             player.vy * 6);
        else
            draw_rect(PLAYER_X, player.y, PLAYER_W, PLAYER_H, COL_PLAYER);

        /* Partículas e popups de pontuação (brilhos, rasto do jetpack) */
        draw_particles();

        /* Mostrar pontuação */
        char sbuf[16];
        int_to_str(score, sbuf, sizeof(sbuf));
        draw_text("SCORE:", 10, 10, 2, COL_TEXT);
        draw_text(sbuf, 10 + 6*6*2, 10, 2, COL_SCORE);

        /* Mostrar moedas */
        char cbuf[16];
        int_to_str(coins_collected, cbuf, sizeof(cbuf));
        draw_text("x", 34, 40, 2, COL_COIN);
        draw_text(cbuf, 50, 40, 2, COL_COIN);
        if (sprites_ok) {
            int hf = coin_frame();
            draw_sprite(coin_pix[hf], coin_img[hf], 10, 36, COIN_SCALE);
        } else {
            int ci = COIN_SIZE;
            for (int dy = -ci/2; dy <= ci/2; dy++) {
                int hw = ci/2 - abs(dy);
                draw_rect(10 + ci/2 - hw, 36 + ci/2 + dy, hw*2, 1, COL_COIN);
            }
        }

        /* Indicador de velocidade */
        char spdbuf[16];
        int_to_str(scroll_speed, spdbuf, sizeof(spdbuf));
        draw_text("SPD:", scr_w - 120, 10, 1, COL_TEXT);
        draw_text(spdbuf, scr_w - 80, 10, 1, COL_SCORE);
    }
    else if (state == STATE_GAMEOVER) {
        /* ECRÃ DE GAME OVER */
        clear_buffer(hit_flash > 0 ? 0x661111 : COL_MENU_BG);   /* flash vermelho */

        /* Deixa a explosão da morte reproduzir-se no ecrã */
        draw_particles();

        draw_text("GAME OVER", scr_w/2 - 9*6*4/2, scr_h/4, 4, COL_GAMEOVER);

        char sbuf[16];
        int_to_str(score, sbuf, sizeof(sbuf));
        draw_text("SCORE:", scr_w/2 - 10*6*2/2, scr_h/2 - 30, 2, COL_TEXT);
        draw_text(sbuf, scr_w/2 + 2*6*2, scr_h/2 - 30, 2, COL_SCORE);

        char cbuf[16];
        int_to_str(coins_collected, cbuf, sizeof(cbuf));
        draw_text("COINS:", scr_w/2 - 10*6*1/2, scr_h/2 - 2, 1, COL_TEXT);
        draw_text(cbuf, scr_w/2 + 2*6*1, scr_h/2 - 2, 1, COL_COIN);

        draw_text("PRESS W TO RESTART", scr_w/2 - 18*6*1/2, scr_h/2 + 40, 1, COL_SCORE);
        draw_text("ESC TO QUIT", scr_w/2 - 11*6*1/2, scr_h/2 + 60, 1, COL_TEXT);
    }

    /* Copia sempre para a VRAM */
    swap_buffers();
}

/* INPUT DO TECLADO */

void game_handle_kbd(uint8_t scancode) {
    /* ESC sai sempre */
    if (scancode == ESC_MAKE || scancode == 0x81) {
        state = STATE_EXIT;
        return;
    }

    switch (state) {
        case STATE_MENU:
            if (scancode == W_MAKE || scancode == SPACE_MAKE || scancode == ENTER_MAKE) {
                reset_game();
                state = STATE_PLAYING;
            }
            break;

        case STATE_PLAYING:
            if (scancode == W_MAKE || scancode == SPACE_MAKE)
                player.thrusting = true;
            else if (scancode == W_BREAK || scancode == SPACE_BREAK)
                player.thrusting = false;
            break;

        case STATE_GAMEOVER:
            if (scancode == W_MAKE || scancode == SPACE_MAKE || scancode == ENTER_MAKE) {
                reset_game();
                state = STATE_PLAYING;
            }
            break;

        default:
            break;
    }
}
