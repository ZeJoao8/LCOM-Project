#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

extern struct packet mouse_packet;
extern uint8_t packet_byte_idx;
extern bool mouse_error;
extern uint8_t mouse_current_byte;

int (mouse_subscribe_int)(uint8_t *bit_no);
int (mouse_unsubscribe_int)();
void (mouse_ih)();
void (mouse_sync_packet)();
void (mouse_parse_packet)();
bool (mouse_process_interrupt)();
int (mouse_write_cmd)(uint8_t cmd);

#endif
