#ifndef _RTC_H_
#define _RTC_H_

/**
 * @file rtc.h
 * @brief Real-Time Clock (MC146818) driver. Reads the current date/time and
 *        drives a once-per-second update-ended interrupt so the on-screen clock
 *        ticks live. Adapted from lab1/rtc.c.
 */

#include <stdint.h>

/** Current date and time read from the RTC. */
typedef struct {
    uint8_t sec, min, hour;
    uint8_t day, month, year;
} rtc_datetime;

/** Subscribe the RTC interrupt (IRQ 8). Returns 0 on success. */
int rtc_subscribe_int(uint8_t *bit_no);

/** Unsubscribe the RTC interrupt. Returns 0 on success. */
int rtc_unsubscribe_int(void);

/** Interrupt handler: acknowledges the interrupt by reading register C. */
void rtc_ih(void);

/** Enable the update-ended interrupt (fires once per second). */
int rtc_enable_update_int(void);

/** Disable the update-ended interrupt. */
int rtc_disable_update_int(void);

/** Read the current date/time (handles BCD and 12/24h). Returns 0 on success. */
int rtc_read_datetime(rtc_datetime *dt);

/** A seed for srand() derived from the current time (falls back to a constant). */
unsigned rtc_seed(void);

#endif /* _RTC_H_ */
