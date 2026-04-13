#include <lcom/lcf.h>
#include <stdint.h>

int(util_sys_inb)(int port, uint8_t *value) {
  uint32_t val32;
  if (sys_inb(port, &val32) != 0) {
    return 1;
  }
  *value = val32 & 0xFF;
  return 0;
}




