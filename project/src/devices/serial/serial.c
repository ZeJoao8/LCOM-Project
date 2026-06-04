/**
 * @file serial.c
 * @brief Interrupt-driven UART (16550) driver for COM1 with RX/TX ring buffers.
 */
#include <lcom/lcf.h>
#include "devices/serial/serial.h"

/* COM1 base port and register offsets. */
#define COM1_BASE 0x3F8
#define SER_RBR   0   /* DLAB=0: receiver buffer (read)        */
#define SER_THR   0   /* DLAB=0: transmitter holding (write)   */
#define SER_IER   1   /* DLAB=0: interrupt enable              */
#define SER_DLL   0   /* DLAB=1: divisor latch low             */
#define SER_DLM   1   /* DLAB=1: divisor latch high            */
#define SER_IIR   2   /* interrupt identification (read)       */
#define SER_FCR   2   /* FIFO control (write)                  */
#define SER_LCR   3   /* line control                          */
#define SER_MCR   4   /* modem control                         */
#define SER_LSR   5   /* line status                           */

/* LCR */
#define LCR_8N1   0x03   /* 8 bits, no parity, 1 stop bit */
#define LCR_DLAB  0x80
/* IER */
#define IER_RDA   0x01   /* received-data-available interrupt */
#define IER_THRE  0x02   /* transmitter-holding-empty interrupt */
#define IER_LS    0x04   /* receiver line-status interrupt */
/* IIR */
#define IIR_NO_INT 0x01
#define IIR_ID    0x0E
#define IIR_THRE  0x02
#define IIR_RDA   0x04
#define IIR_LS    0x06
#define IIR_TIMEOUT 0x0C
/* LSR */
#define LSR_RXRDY 0x01
#define LSR_OE    0x02   /* overrun error  */
#define LSR_PE    0x04   /* parity error   */
#define LSR_FE    0x08   /* framing error  */
#define LSR_BI    0x10   /* break interrupt*/
#define LSR_THRE  0x20
#define LSR_ERR   (LSR_OE | LSR_PE | LSR_FE | LSR_BI)
/* FCR */
#define FCR_ENABLE 0x01
#define FCR_CLR_RX 0x02
#define FCR_CLR_TX 0x04
#define FCR_TRIG14 0xC0
/* MCR */
#define MCR_DTR   0x01
#define MCR_RTS   0x02
#define MCR_OUT2  0x08   /* gates UART interrupts onto the PC IRQ line */

#define SER_IRQ   4
#define SER_HOOK_ID 5    /* keep distinct from timer/kbd/mouse/rtc hooks */
#define BAUD_DIV  1      /* 115200 baud (115200 / 1) */

#define RING 512
static volatile uint8_t rx_buf[RING];
static volatile uint8_t tx_buf[RING];
static volatile int rx_head = 0, rx_tail = 0;
static volatile int tx_head = 0, tx_tail = 0;

/* Diagnostics: counted in the ISR, never block the data path. */
static volatile int rx_dropped = 0;          /* bytes lost to a full RX ring   */
static volatile int line_errors = 0;         /* LSR overrun/framing/parity/break */
static volatile int tx_dropped_frames = 0;   /* frames rejected by the caller for atomicity */

static int serial_hook_id = SER_HOOK_ID;

/* ── Ring helpers ─────────────────────────────────────────────────────────── */
static bool rx_empty(void) { return rx_head == rx_tail; }
static bool tx_empty(void) { return tx_head == tx_tail; }

static void rx_push(uint8_t b) {
    int n = (rx_head + 1) % RING;
    if (n != rx_tail) { rx_buf[rx_head] = b; rx_head = n; }
    else rx_dropped++;                                         /* ring full: count the loss */
}
static int rx_pop(void) {
    if (rx_empty()) return -1;
    uint8_t b = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RING;
    return b;
}
static bool tx_push(uint8_t b) {
    int n = (tx_head + 1) % RING;
    if (n == tx_tail) return false;                           /* full */
    tx_buf[tx_head] = b; tx_head = n; return true;
}
static int tx_pop(void) {
    if (tx_empty()) return -1;
    uint8_t b = tx_buf[tx_tail];
    tx_tail = (tx_tail + 1) % RING;
    return b;
}

/* Read a UART register; returns 0xFF on failure. */
static uint8_t ser_in(int off) {
    uint32_t v;
    if (sys_inb(COM1_BASE + off, &v) != OK) return 0xFF;
    return (uint8_t) v;
}
static void ser_out(int off, uint8_t v) {
    sys_outb(COM1_BASE + off, v);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int serial_subscribe_int(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;
    *bit_no = serial_hook_id;
    if (sys_irqsetpolicy(SER_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &serial_hook_id) != OK)
        return 1;
    return 0;
}

int serial_unsubscribe_int(void) {
    ser_out(SER_IER, 0x00);          /* disable all UART interrupts */
    if (sys_irqrmpolicy(&serial_hook_id) != OK) return 1;
    return 0;
}

int serial_config(void) {
    /* Set divisor for the baud rate (requires DLAB=1). */
    ser_out(SER_LCR, LCR_DLAB | LCR_8N1);
    ser_out(SER_DLL, BAUD_DIV & 0xFF);
    ser_out(SER_DLM, (BAUD_DIV >> 8) & 0xFF);
    ser_out(SER_LCR, LCR_8N1);                       /* DLAB=0, 8N1 */

    /* Enable and clear the FIFOs. */
    ser_out(SER_FCR, FCR_ENABLE | FCR_CLR_RX | FCR_CLR_TX | FCR_TRIG14);

    /* Route interrupts to the PC IRQ line. */
    ser_out(SER_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);

    rx_head = rx_tail = tx_head = tx_tail = 0;
    rx_dropped = line_errors = tx_dropped_frames = 0;

    /* Enable received-data-available and line-status (error) interrupts. */
    ser_out(SER_IER, IER_RDA | IER_LS);
    return 0;
}

void serial_ih(void) {
    uint8_t iir;
    while (!((iir = ser_in(SER_IIR)) & IIR_NO_INT)) {   /* while an interrupt is pending */
        uint8_t id = iir & IIR_ID;

        if (id == IIR_RDA || id == IIR_TIMEOUT) {       /* data to read */
            uint8_t lsr;
            while ((lsr = ser_in(SER_LSR)) & LSR_RXRDY) {
                uint8_t byte = ser_in(SER_RBR);         /* reading RBR clears the error */
                if (lsr & LSR_ERR) line_errors++;       /* corrupt byte: drop it */
                else rx_push(byte);
            }
        }
        else if (id == IIR_THRE) {                      /* room to write */
            while (!tx_empty() && (ser_in(SER_LSR) & LSR_THRE))
                ser_out(SER_THR, (uint8_t) tx_pop());
            if (tx_empty())
                ser_out(SER_IER, IER_RDA | IER_LS);     /* stop asking for THRE */
        }
        else if (id == IIR_LS) {
            if (ser_in(SER_LSR) & LSR_ERR) line_errors++;   /* read clears the status */
        }
        else {
            (void) ser_in(SER_RBR);                     /* clear anything else */
        }
    }
}

int serial_putc(uint8_t byte) {
    if (!tx_push(byte)) return 1;
    ser_out(SER_IER, IER_RDA | IER_LS | IER_THRE);   /* tell me when THR empties */
    return 0;
}

int serial_getc(void) {
    return rx_pop();
}

bool serial_available(void) {
    return !rx_empty();
}

int serial_rx_dropped(void) { return rx_dropped; }
int serial_line_errors(void) { return line_errors; }

int serial_tx_free(void) {
    int h = tx_head, t = tx_tail;
    int used = (h - t + RING) % RING;
    return RING - 1 - used;            /* one slot is reserved to distinguish full from empty */
}

void serial_rx_drain(void) {
    rx_head = rx_tail = 0;             /* atomic enough at our cadence; ISR refills as needed */
}

int  serial_tx_dropped_frames(void) { return tx_dropped_frames; }
void serial_tx_count_drop(void)     { tx_dropped_frames++; }
