/**
 * @file net.c
 * @brief 2-player netplay protocol over the serial port. Atomic transmit
 *        (a whole frame is enqueued only when it fits in the TX ring), CRC-8
 *        error detection, and a sliding-window parser that re-syncs byte by
 *        byte after corruption so a payload byte that happens to equal a sync
 *        byte cannot lock the parser onto a false frame.
 *
 * Frame layout: SYNC0(0xA5) SYNC1(0x5A) TYPE LEN B0..B(LEN-1) CRC8
 *   CRC8 = poly 0x07, init 0, covers TYPE, LEN, and the LEN payload bytes.
 */
#include <stddef.h>    /* NULL */
#include "game/net.h"
#include "devices/serial/serial.h"

/* ── Frame format constants ──────────────────────────────────────────────── */
#define SYNC0 0xA5
#define SYNC1 0x5A

#define NET_STATE  1
#define NET_DEAD   2
#define NET_HELLO  3
#define NET_READY  4
#define NET_BYE    5

#define HDR_LEN     2     /* SYNC0 SYNC1 */
#define META_LEN    2     /* TYPE LEN    */
#define CRC_LEN     1
#define FRAME_OVHD  (HDR_LEN + META_LEN + CRC_LEN)   /* 5 */

/* ── CRC-8 (poly 0x07, init 0) ───────────────────────────────────────────── */
static uint8_t crc8(const uint8_t *d, int n) {
    uint8_t c = 0;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++)
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
}

/* ── Transmit ────────────────────────────────────────────────────────────── */
/* Push the whole frame only if it all fits; otherwise drop the frame. The
 * caller never observes a torn frame on the wire. */
static void net_send_frame(uint8_t type, const uint8_t *payload, uint8_t len) {
    if (len > NET_MAX_PAYLOAD) return;

    int total = FRAME_OVHD + len;
    if (serial_tx_free() < total) {
        serial_tx_count_drop();
        return;
    }

    uint8_t frame[FRAME_OVHD + NET_MAX_PAYLOAD];
    frame[0] = SYNC0;
    frame[1] = SYNC1;
    frame[2] = type;
    frame[3] = len;
    for (uint8_t i = 0; i < len; i++) frame[4 + i] = payload[i];
    frame[4 + len] = crc8(&frame[2], META_LEN + len);

    for (int i = 0; i < total; i++) serial_putc(frame[i]);   /* guaranteed to fit */
}

static uint16_t clamp16(int v) {
    if (v < 0) return 0;
    if (v > 0xFFFF) return 0xFFFF;
    return (uint16_t) v;
}

static void net_send_state(int y, int score) {
    uint16_t ye = clamp16(y), sc = clamp16(score);
    uint8_t p[4] = { (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF),
                     (uint8_t)(sc >> 8), (uint8_t)(sc & 0xFF) };
    net_send_frame(NET_STATE, p, 4);
}

static void net_send_dead(int score) {
    uint16_t sc = clamp16(score);
    uint8_t p[2] = { (uint8_t)(sc >> 8), (uint8_t)(sc & 0xFF) };
    net_send_frame(NET_DEAD, p, 2);
}

static void net_send_hello(void) { net_send_frame(NET_HELLO, NULL, 0); }
static void net_send_ready(void) { net_send_frame(NET_READY, NULL, 0); }

void net_send_bye(void) { net_send_frame(NET_BYE, NULL, 0); }

/* ── Per-tick broadcaster ────────────────────────────────────────────────── */
void net_tick(GameModel *m) {
    if (!m->versus) return;

    static int hello_ctr = 0;
    static int state_ctr = 0;

    if (m->state == STATE_WAITING) {
        m->wait_ctr++;
        if (++hello_ctr >= NET_HELLO_INTERVAL) {
            hello_ctr = 0;
            net_send_hello();
            if (m->peer_seen) net_send_ready();
        }
        return;     /* no STATE, DEAD or peer-silence handling while waiting */
    }

    /* In PLAYING/GAMEOVER, a long silence on the wire is treated as the peer
     * having dropped out so the round can resolve. */
    if (m->opp_alive && m->state != STATE_MENU) {
        if (++m->net_silence > NET_PEER_TIMEOUT)
            m->opp_alive = false;
    }

    if (m->state == STATE_PLAYING) {
        if (++state_ctr >= NET_SEND_INTERVAL) {
            state_ctr = 0;
            net_send_state(m->player.y, m->score);
        }
    }

    if (m->state == STATE_GAMEOVER && !m->dead_sent) {
        net_send_dead(m->score);
        m->dead_sent = true;
    }
}

/* ── Receive (sliding-window parser) ─────────────────────────────────────── */
#define NET_FBUF 16
static uint8_t fbuf[NET_FBUF];
static int     flen = 0;

static void shift_one(void) {
    for (int i = 1; i < flen; i++) fbuf[i - 1] = fbuf[i];
    flen--;
}

static void shift_n(int n) {
    if (n <= 0) return;
    if (n >= flen) { flen = 0; return; }
    for (int i = n; i < flen; i++) fbuf[i - n] = fbuf[i];
    flen -= n;
}

void net_reset_parser(void) {
    flen = 0;
}

/* Apply a validated frame's effect on the model. NET_STATE and NET_DEAD are
 * only ever sent by a peer that has left WAITING, so receiving one while we
 * are still in WAITING is conclusive proof that the peer is ready to play —
 * we promote both handshake flags so the game.c transition fires this tick.
 * Likewise NET_READY implies NET_HELLO. */
static void net_apply(GameModel *m, uint8_t type, const uint8_t *p, uint8_t len) {
    m->net_silence = 0;
    switch (type) {
        case NET_STATE:
            if (len >= 4) {
                m->opp_y     = (p[0] << 8) | p[1];
                m->opp_score = (p[2] << 8) | p[3];
                m->opp_alive = true;
            }
            if (m->state == STATE_WAITING) {
                m->peer_seen = true; m->peer_ready = true;
            }
            break;
        case NET_DEAD:
            if (len >= 2) {
                m->opp_score = (p[0] << 8) | p[1];
                m->opp_alive = false;
            }
            if (m->state == STATE_WAITING) {
                m->peer_seen = true; m->peer_ready = true;
            }
            break;
        case NET_HELLO:
            if (m->state == STATE_WAITING) m->peer_seen = true;
            break;
        case NET_READY:
            if (m->state == STATE_WAITING) {
                m->peer_seen = true; m->peer_ready = true;
            }
            break;
        case NET_BYE:
            if (m->state == STATE_WAITING) m->state = STATE_MENU;
            else                            m->opp_alive = false;
            break;
        default:
            break;     /* unknown TYPE: silently ignored */
    }
}

void net_poll(GameModel *m) {
    if (!m->versus) return;

    /* Drain the RX ring into fbuf, dropping the oldest byte when full. */
    int c;
    while ((c = serial_getc()) >= 0) {
        if (flen >= NET_FBUF) shift_one();
        fbuf[flen++] = (uint8_t) c;
    }

    /* Try to parse one frame at a time from fbuf[0]. On any mismatch slide
     * forward by exactly one byte so a real SYNC0 SYNC1 lurking inside what
     * looked like payload still gets a chance. */
    while (flen > 0) {
        if (fbuf[0] != SYNC0) { shift_one(); continue; }
        if (flen < 2)         break;
        if (fbuf[1] != SYNC1) { shift_one(); continue; }
        if (flen < 4)         break;                  /* need TYPE + LEN */

        uint8_t type = fbuf[2];
        uint8_t len  = fbuf[3];
        if (len > NET_MAX_PAYLOAD) { shift_one(); continue; }

        int total = FRAME_OVHD + len;
        if (flen < total) break;                      /* incomplete: wait for more */

        uint8_t want = crc8(&fbuf[2], META_LEN + len);
        if (want != fbuf[4 + len]) { shift_one(); continue; }

        net_apply(m, type, &fbuf[4], len);
        shift_n(total);
    }
}
