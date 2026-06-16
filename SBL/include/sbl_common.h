#ifndef SBL_COMMON_H
#define SBL_COMMON_H

#include "stm32f407xx.h"
#include <stdint.h>

#define SBL_CODE  __attribute__((section(".sbl.text"), noinline, used))
#define SBL_CONST __attribute__((section(".sbl.rodata"), used))

#define SBL_LCD_W 240U
#define SBL_LCD_H 240U

#define SBL_RGB565(r, g, b) \
  (uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | ((b) >> 3))

#define SBL_BLACK SBL_RGB565(0x00, 0x00, 0x00)
#define SBL_WHITE SBL_RGB565(0xFF, 0xFF, 0xFF)
#define SBL_GREY  SBL_RGB565(0x70, 0x70, 0x70)
#define SBL_GREEN SBL_RGB565(0x4C, 0xFF, 0x4C)
#define SBL_BLUE  SBL_RGB565(0x1A, 0x4D, 0xFF)
#define SBL_RED   SBL_RGB565(0xFF, 0x30, 0x30)

SBL_CODE void SBL_Delay(volatile uint32_t loops);
SBL_CODE void SBL_DelayMs(uint32_t ms);
SBL_CODE void SBL_GpioSet(GPIO_TypeDef *port, uint32_t pin);
SBL_CODE void SBL_GpioReset(GPIO_TypeDef *port, uint32_t pin);
SBL_CODE uint8_t SBL_FlashUnlock(void);
SBL_CODE void SBL_FlashLock(void);
SBL_CODE void SBL_FlashClearStatus(void);
SBL_CODE uint8_t SBL_FlashProgramWord(uint32_t addr, uint32_t word);
SBL_CODE uint8_t SBL_FlashEraseSectorIndex(uint32_t sector_index);

#endif /* SBL_COMMON_H */
