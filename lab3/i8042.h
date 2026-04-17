#ifndef _LCOM_I8042_H_
#define _LCOM_I8042_H_

#include <lcom/lcf.h>

#define KBD_IRQ 1

#define KBC_OUT_BUF 0x60
#define KBC_ST_REG 0x64
#define KBC_CMD_REG 0x64

/* KBC STATUS REGISTER BITS */
#define KBC_OBF BIT(0)
#define KBC_IBF BIT(1)
#define KBC_SYS BIT(2)
#define KBC_A2 BIT(3)
#define KBC_INH BIT(4)
#define KBC_AUX BIT(5)
#define KBC_TIMEOUT_ERR BIT(6)
#define KBC_PARITY_ERR BIT(7)

/* KEYBOARD MACROS */
#define ESC_BREAKCODE 0x81
#define TWO_BYTE_SCANCODE 0xE0
#define BREAK_CODE_BIT BIT(7)

/* POLLING & KBC COMMANDS */
#define DELAY_US 20000
#define KBC_READ_CMD 0x20
#define KBC_WRITE_CMD 0x60
#define KBC_INT_KBD BIT(0)

#endif

