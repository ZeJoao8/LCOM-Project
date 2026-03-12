#include "rtc.h"

#define TODO return -1

#define RTC_ADDR_REG 0x70
#define RTC_DATA_REG 0x71
#define RTC_REG_A 0x0A
#define RTC_REG_B 0x0B
#define RTC_REG_DAY 0x07
#define RTC_REG_MONTH 0x08
#define RTC_REG_YEAR 0x09
#define RTC_UIP_MSK (1 << 7)
#define RTC_DM_MSK (1 << 2)

static int bcd_to_bin(uint8_t bcd) { 
    return ((bcd>>4)*10) + (bcd & 0x0F);
 }

static uint8_t rtc_read_reg(uint8_t reg){
  uint32_t val;

  if (sys_outb(RTC_ADDR_REG, reg)!=OK) return -1;
  if (sys_inb(RTC_DATA_REG, &val)!=OK) return -1;

  return val;
}


int rtc_read_date(rtc_date *date) { 
  if (!date) return -1;

  while (rtc_read_reg(RTC_REG_A) & RTC_UIP_MSK){
    tickdelay(micros_to_ticks(244));
  }

  uint8_t day = rtc_read_reg(RTC_REG_DAY);
  uint8_t month = rtc_read_reg(RTC_REG_MONTH);
  uint8_t year = rtc_read_reg(RTC_REG_YEAR);

  uint8_t reg_b = rtc_read_reg(RTC_REG_B);
  bool bcd_mode = !(reg_b & RTC_DM_MSK);

  if (bcd_mode){
    day = bcd_to_bin(day);
    month = bcd_to_bin(month);
    year = bcd_to_bin(year);
  }

  date->day=day;
  date->month=month;
  date->year=year;

  return 0;
}
