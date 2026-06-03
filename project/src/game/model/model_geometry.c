/**
 * @file model_geometry.c
 * @brief Integer trigonometry (no FPU) and point/segment geometry used for the
 *        zapper beams and their collision tests.
 */
#include "game/model.h"
#include "game/model_internal.h"

/* ── Integer trigonometry (sin/cos scaled by 1024, ANG_STEPS per turn) ────── */
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
int isin(int a) { return sin1024[((a % ANG_STEPS) + ANG_STEPS) % ANG_STEPS]; }
int icos(int a) { return isin(a + ANG_VERT); }

void zap_endpoints(const Zapper *z, int *ax, int *ay, int *bx, int *by) {
    int dx = (z->half * icos(z->ang)) / 1024;
    int dy = (z->half * isin(z->ang)) / 1024;
    *ax = z->cx - dx; *ay = z->cy - dy;
    *bx = z->cx + dx; *by = z->cy + dy;
}

/* Squared distance from point P to segment A-B (integer math). */
long pt_seg_d2(int px, int py, int ax, int ay, int bx, int by) {
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

/* True if the player collides with the beam (sampled along the body). */
bool zap_hits_player(const GameModel *m, const Zapper *z) {
    int ax, ay, bx, by;
    zap_endpoints(z, &ax, &ay, &bx, &by);
    int pcx = PLAYER_X + PLAYER_W / 2;
    long thr = BEAM_THICK + PLAYER_W / 2;
    long thr2 = thr * thr;
    for (int k = 1; k <= 3; k++) {
        int py = m->player.y + (PLAYER_H * k) / 4;
        if (pt_seg_d2(pcx, py, ax, ay, bx, by) < thr2)
            return true;
    }
    return false;
}
