/**
 * @file net.c
 * @brief 2-player netplay protocol over the serial port. Parses incoming frames
 *        into the opponent's fields and periodically broadcasts our own state.
 */
#include "game/net.h"
#include "devices/serial/serial.h"

#define NET_SYNC   0xA5
#define NET_STATE  1
#define NET_DEAD   2

/* ── Transmit ─────────────────────────────────────────────────────────────── */
static void net_send(uint8_t type, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    uint8_t chk = type ^ b0 ^ b1 ^ b2 ^ b3;
    serial_putc(NET_SYNC);
    serial_putc(type);
    serial_putc(b0);
    serial_putc(b1);
    serial_putc(b2);
    serial_putc(b3);
    serial_putc(chk);
}

static uint16_t clamp16(int v) {
    if (v < 0) return 0;
    if (v > 0xFFFF) return 0xFFFF;
    return (uint16_t) v;
}

void net_tick(GameModel *m) {
    if (!m->versus) return;

    static int send_ctr = 0;
    if (m->state == STATE_PLAYING) {
        if (++send_ctr >= NET_SEND_INTERVAL) {
            send_ctr = 0;
            uint16_t y  = clamp16(m->player.y);
            uint16_t sc = clamp16(m->score);
            net_send(NET_STATE, y >> 8, y & 0xFF, sc >> 8, sc & 0xFF);
        }
    }

    if (m->state == STATE_GAMEOVER && !m->dead_sent) {
        uint16_t sc = clamp16(m->score);
        net_send(NET_DEAD, sc >> 8, sc & 0xFF, 0, 0);
        m->dead_sent = true;
    }
}

/* ── Receive (frame parser) ───────────────────────────────────────────────── */
static void net_apply(GameModel *m, uint8_t type, const uint8_t *p) {
    if (type == NET_STATE) {
        m->opp_y     = (p[0] << 8) | p[1];
        m->opp_score = (p[2] << 8) | p[3];
        m->opp_alive = true;
    } else if (type == NET_DEAD) {
        m->opp_score = (p[0] << 8) | p[1];
        m->opp_alive = false;
    }
}

void net_poll(GameModel *m) {
    if (!m->versus) return;

    /* Frame parser state (persists across calls). */
    static int     st = 0;      /* 0=sync,1=type,2..5=payload,6=checksum */
    static uint8_t type = 0;
    static uint8_t pay[4];
    static int     idx = 0;

    int c;
    while ((c = serial_getc()) >= 0) {
        uint8_t b = (uint8_t) c;
        switch (st) {
            case 0: if (b == NET_SYNC) st = 1;            break;
            case 1: type = b; idx = 0; st = 2;            break;
            case 2: case 3: case 4: case 5:
                pay[idx++] = b;
                st = (idx >= 4) ? 6 : st + 1;
                break;
            case 6: {
                uint8_t chk = type ^ pay[0] ^ pay[1] ^ pay[2] ^ pay[3];
                if (chk == b) net_apply(m, type, pay);    /* else: drop frame */
                st = 0;
                break;
            }
            default: st = 0; break;
        }
    }
}
