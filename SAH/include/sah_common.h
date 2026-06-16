#ifndef SAH_COMMON_H
#define SAH_COMMON_H

#include "stm32f407xx.h"
#include <stdint.h>

#define SAH_CODE  __attribute__((section(".sbl.text"), noinline, used))
#define SAH_CONST __attribute__((section(".sah.rodata"), used))

#define SAH_LCD_W 240U
#define SAH_LCD_H 240U

SAH_CODE void SAH_Run(void);
SAH_CODE void SAH_DelayMs(uint32_t ms);

#endif /* SAH_COMMON_H */
