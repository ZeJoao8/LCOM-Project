#ifndef _LCOM_MOUSE_H_
#define _LCOM_MOUSE_H_

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>
#include "i8042.h" // Hardware macros included here

extern uint8_t mouse_byte;  
extern bool mouse_error;    

int mouse_subscribe_int(uint8_t *bit_no);
int mouse_unsubscribe_int();
void (mouse_ih)();
void mouse_parse_packet(uint8_t bytes[3], struct packet *pp);
int mouse_write_cmd(uint8_t cmd);

#endif /* _LCOM_MOUSE_H_ */
