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
/** @brief Integer sine of angle @p a (ANG_STEPS per turn). @return sin scaled by 1024. */
int isin(int a) { return sin1024[((a % ANG_STEPS) + ANG_STEPS) % ANG_STEPS]; }
/** @brief Integer cosine of angle @p a. @return cos scaled by 1024. */
int icos(int a) { return isin(a + ANG_VERT); }

/**
 * @brief Compute the two screen-space endpoints of a zapper beam from its centre,
 *        half-length and angle.
 * @param z      Zapper to evaluate.
 * @param ax,ay  Out: one endpoint.
 * @param bx,by  Out: the opposite endpoint.
 */
void zap_endpoints(const Zapper *z, int *ax, int *ay, int *bx, int *by) {
    int dx = (z->half * icos(z->ang)) / 1024;
    int dy = (z->half * isin(z->ang)) / 1024;
    *ax = z->cx - dx; *ay = z->cy - dy;
    *bx = z->cx + dx; *by = z->cy + dy;
}

/**
 * @brief Squared distance from point P to segment A-B, using integer math only.
 * @param px,py Point P.
 * @param ax,ay Segment start A.
 * @param bx,by Segment end B.
 * @return The squared distance (avoids a sqrt; compare against a squared threshold).
 */
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

/**
 * @brief Test whether the player overlaps a zapper beam.
 *
 * Samples three points down the player's body and checks each against the beam
 * segment, treating a hit as the squared distance falling under the combined
 * beam/player half-width.
 *
 * @param m Game model (for the player position).
 * @param z Zapper to test.
 * @return true if the player collides with the beam.
 */
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
