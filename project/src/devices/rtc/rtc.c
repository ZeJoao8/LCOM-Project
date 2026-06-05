/**
 * @file rtc.c
 * @brief Real-Time Clock (MC146818) driver. Adapted and extended from
 *        lab1/rtc.c: full date+time read plus an update-ended interrupt.
 */
#include <lcom/lcf.h>
#include "devices/rtc/rtc.h"

/* Address / data ports */
#define RTC_ADDR_REG 0x70
#define RTC_DATA_REG 0x71

/* Time / date registers */
#define RTC_SEC      0x00
#define RTC_MIN      0x02
#define RTC_HOUR     0x04
#define RTC_DAY      0x07
#define RTC_MONTH    0x08
#define RTC_YEAR     0x09

/* Control registers */
#define RTC_REG_A    0x0A
#define RTC_REG_B    0x0B
#define RTC_REG_C    0x0C

#define RTC_UIP   (1 << 7)   /* reg A: update in progress              */
#define RTC_24H   (1 << 1)   /* reg B: 1 = 24h mode                    */
#define RTC_DM    (1 << 2)   /* reg B: 1 = binary, 0 = BCD             */
#define RTC_UIE   (1 << 4)   /* reg B: update-ended interrupt enable   */
#define RTC_UF    (1 << 4)   /* reg C: update-ended interrupt flag     */

#define RTC_IRQ      8
#define RTC_HOOK_ID  3       /* keep distinct from timer/kbd/mouse/serial hooks */

static int rtc_hook_id = RTC_HOOK_ID;

/** @brief Convert a packed BCD byte to its binary value. */
static int bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/** @brief Read one RTC register: select it via the address port, read the data port. @return register value (0 on I/O error). */
static uint8_t rtc_read_reg(uint8_t reg) {
    uint32_t val;
    if (sys_outb(RTC_ADDR_REG, reg) != OK) return 0;
    if (sys_inb(RTC_DATA_REG, &val) != OK) return 0;
    return (uint8_t) val;
}

/** @brief Write @p val to RTC register @p reg. @return 0 on success, 1 on I/O error. */
static int rtc_write_reg(uint8_t reg, uint8_t val) {
    if (sys_outb(RTC_ADDR_REG, reg) != OK) return 1;
    if (sys_outb(RTC_DATA_REG, val) != OK) return 1;
    return 0;
}

/** @brief Subscribe the RTC interrupt (IRQ 8) in reenable mode. @param bit_no Out: mask bit. @return 0 on success, 1 on failure. */
int rtc_subscribe_int(uint8_t *bit_no) {
    if (bit_no == NULL) return 1;
    *bit_no = rtc_hook_id;
    if (sys_irqsetpolicy(RTC_IRQ, IRQ_REENABLE, &rtc_hook_id) != OK)
        return 1;
    return 0;
}

/** @brief Remove the RTC interrupt policy. @return 0 on success, 1 on failure. */
int rtc_unsubscribe_int(void) {
    if (sys_irqrmpolicy(&rtc_hook_id) != OK) return 1;
    return 0;
}

/** @brief RTC interrupt handler: reads register C to acknowledge and clear the interrupt flags. */
void rtc_ih(void) {
    /* Reading register C acknowledges the interrupt and clears its flags. */
    (void) rtc_read_reg(RTC_REG_C);
}

/** @brief Enable the update-ended interrupt (reg B, UIE bit). @return 0 on success, 1 on failure. */
int rtc_enable_update_int(void) {
    uint8_t b = rtc_read_reg(RTC_REG_B);
    return rtc_write_reg(RTC_REG_B, b | RTC_UIE);
}

/** @brief Disable the update-ended interrupt (reg B, UIE bit). @return 0 on success, 1 on failure. */
int rtc_disable_update_int(void) {
    uint8_t b = rtc_read_reg(RTC_REG_B);
    return rtc_write_reg(RTC_REG_B, b & ~RTC_UIE);
}

/**
 * @brief Read the full current date and time into @p dt.
 *
 * Waits for any in-progress update to finish, reads all six time/date
 * registers, then normalises them to binary 24-hour form based on the BCD and
 * 12/24-hour flags in register B.
 *
 * @param dt Out: receives the decoded date/time.
 * @return 0 on success, 1 if @p dt is NULL.
 */
int rtc_read_datetime(rtc_datetime *dt) {
    if (dt == NULL) return 1;

    /* Wait until no update is in progress to read consistent values. */
    while (rtc_read_reg(RTC_REG_A) & RTC_UIP)
        tickdelay(micros_to_ticks(244));

    uint8_t sec   = rtc_read_reg(RTC_SEC);
    uint8_t min   = rtc_read_reg(RTC_MIN);
    uint8_t hour  = rtc_read_reg(RTC_HOUR);
    uint8_t day   = rtc_read_reg(RTC_DAY);
    uint8_t month = rtc_read_reg(RTC_MONTH);
    uint8_t year  = rtc_read_reg(RTC_YEAR);

    uint8_t b = rtc_read_reg(RTC_REG_B);
    bool bcd = !(b & RTC_DM);
    bool h24 = (b & RTC_24H);

    bool pm = false;
    if (!h24) { pm = (hour & 0x80) != 0; hour &= 0x7F; }

    if (bcd) {
        sec   = bcd_to_bin(sec);
        min   = bcd_to_bin(min);
        hour  = bcd_to_bin(hour);
        day   = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year  = bcd_to_bin(year);
    }

    if (!h24) {                          /* convert 12h -> 24h */
        if (pm && hour != 12) hour += 12;
        else if (!pm && hour == 12) hour = 0;
    }

    dt->sec = sec; dt->min = min; dt->hour = hour;
    dt->day = day; dt->month = month; dt->year = year;
    return 0;
}

/**
 * @brief Derive a pseudo-random seed from the current time (seconds-of-day).
 * @return A seed value; falls back to 42 if the clock cannot be read.
 */
unsigned rtc_seed(void) {
    rtc_datetime dt;
    if (rtc_read_datetime(&dt) != 0) return 42u;
    return (unsigned)(dt.sec + dt.min * 60 + dt.hour * 3600 + dt.day * 86400);
}
