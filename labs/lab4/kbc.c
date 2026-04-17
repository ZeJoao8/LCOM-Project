#include "kbc.h"
#include "i8042.h"
#include "utils.h"
#include <lcom/lcf.h>

int kbc_read_data(uint8_t *data) {
  uint8_t status;
  int retries = 20;

  while (retries > 0) {
    if (util_sys_inb(KBC_ST_REG, &status) == 0) {
      if (status & KBC_OBF) {
        if (util_sys_inb(KBC_OUT_BUF, data) == 0) {
          if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) == 0) {
            return 0;
          }
        }
      }
    }
    tickdelay(micros_to_ticks(DELAY_US));
    retries--;
  }

  return 1;
}

int kbc_issue_cmd(uint8_t cmd) {
  uint8_t status;
  int retries = 20;

  while (retries > 0) {
    if (util_sys_inb(KBC_ST_REG, &status) == 0) {
      if ((status & KBC_IBF) == 0) {
        if (sys_outb(KBC_CMD_REG, cmd) == 0) {
          return 0;
        }
      }
    }
    tickdelay(micros_to_ticks(DELAY_US));
    retries--;
  }
  return 1;
}

int kbc_read_cmd_byte(uint8_t *cmd) {
  if (kbc_issue_cmd(KBC_READ_CMD) != 0)
    return 1;
  if (kbc_read_data(cmd) != 0)
    return 1;
  return 0;
}

int kbc_write_cmd_byte(uint8_t cmd) {
  if (kbc_issue_cmd(KBC_WRITE_CMD) != 0)
    return 1;

  uint8_t status;
  int retries = 20;

  while (retries > 0) {
    if (util_sys_inb(KBC_ST_REG, &status) == 0) {
      if ((status & KBC_IBF) == 0) {
        if (sys_outb(KBC_OUT_BUF, cmd) == 0) {
          return 0;
        }
      }
    }
    tickdelay(micros_to_ticks(DELAY_US));
    retries--;
  }

  return 1;
}
