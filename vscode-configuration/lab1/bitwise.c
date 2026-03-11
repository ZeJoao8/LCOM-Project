#include "bitwise.h"

#define TODO return 255

uint8_t clear(uint8_t msk, int pos) { 
   return msk & ~(1<<pos);
 }

uint8_t set(uint8_t msk, int pos) { 
  return msk | (1<<pos);
 }

bool is_set(uint8_t msk, int pos) { 
  return (msk & (1<<pos))!=0;
 }


uint8_t lsb(uint16_t wide_msk) { 
  return wide_msk & 0xFF; 
 }

uint8_t msb(uint16_t wide_msk) { 
  return wide_msk  >> 8;
 }


#include <stdint.h>
#include <stdarg.h>
uint8_t mask(int pos, ...) { 
  uint8_t msk=0;
  va_list args;
  va_start(args,pos);

  while(pos!=MSK_END){
    msk |= (1<<pos);
    pos=va_arg(args,int);

  }
  va_end(args);

  return msk;
 }
