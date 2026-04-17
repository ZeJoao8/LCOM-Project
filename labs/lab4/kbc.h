#ifndef _KBC_H_
#define _KBC_H_

#include <stdint.h>

int kbc_read_data(uint8_t *data);
int kbc_write_data(uint8_t data);
int kbc_issue_cmd(uint8_t cmd);
int kbc_read_cmd_byte(uint8_t *cmd);
int kbc_write_cmd_byte(uint8_t cmd);

#endif
