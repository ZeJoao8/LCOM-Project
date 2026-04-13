#ifndef _KBC_H_
#define _KBC_H_


#include <stdint.h>
#include <stdbool.h>


extern uint8_t scancode;
extern bool kbc_error;

int kbd_subscribe_int(uint8_t *bit_no);
int kbd_unsubscribe_int();
void (kbc_ih)();

int kbc_read_data(uint8_t *data);
int kbc_issue_cmd(uint8_t cmd);
int kbc_read_cmd_byte(uint8_t *cmd);
int kbc_write_cmd_byte(uint8_t cmd);



#endif



