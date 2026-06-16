#include "init.h"

#include <stdint.h>

#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "sbl_splash.h"
#include "sbl_ui.h"
#include "sbl_usb.h"
#include "trust.h"

extern uint32_t _estack;
extern void Reset_Handler(void);
extern int main(void);
extern void start_tos(void);
extern const uint8_t __trust_start__[];
extern const uint8_t __trust_end__[];

SBL_CODE uint8_t SBL_AppLooksValid(void) {
  const uint32_t flash_lo = 0x08040000UL;
  const uint32_t flash_hi = 0x080C0000UL;
  const uintptr_t symbols[3] = {
      ((uintptr_t)&Reset_Handler & ~1UL),
      ((uintptr_t)&main & ~1UL),
      ((uintptr_t)&start_tos & ~1UL),
  };
  (void)_estack;
  for (uint32_t i = 0U; i < 3U; ++i) {
    uintptr_t addr = symbols[i];
    uint16_t half0;
    uint16_t half1;
    if (addr < flash_lo || addr + 3U >= flash_hi) {
      return 0U;
    }
    half0 = *(const volatile uint16_t *)addr;
    half1 = *(const volatile uint16_t *)(addr + 2U);
    if ((half0 == 0xFFFFU && half1 == 0xFFFFU) ||
        (half0 == 0x0000U && half1 == 0x0000U)) {
      return 0U;
    }
  }
  return 1U;
}

SBL_CODE uint8_t SBL_TrustLooksValid(void) {
  const Trust_Block_t *block = (const Trust_Block_t *)__trust_start__;
  uintptr_t span = (uintptr_t)__trust_end__ - (uintptr_t)__trust_start__;
  if (span < sizeof(Trust_Block_t)) {
    return 0U;
  }
  if (block->magic != TRUST_MAGIC || block->version != TRUST_VERSION) {
    return 0U;
  }
  if (block->crc != Trust_CalcCrc(block)) {
    return 0U;
  }
  return 1U;
}

SBL_CODE void SBL_Run(void) {
  SBL_SplashRun();
  SBL_HwBootstrap();
  if (!SBL_IsFastbootRequested() && SBL_AppLooksValid() &&
      SBL_TrustLooksValid()) {
    return;
  }

  SBL_LedsOff();
  (void)SBL_USB_Init();
  if (!SBL_AppLooksValid() || !SBL_TrustLooksValid()) {
    SBL_UiRunSystemDamage();
  }
  SBL_UiRunFastboot();
}
