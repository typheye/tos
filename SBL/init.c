#include "init.h"

#include <stdint.h>

#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "sbl_common.h"
#include "sbl_splash.h"
#include "sbl_ui.h"
#include "sbl_usb.h"
#include "stm32f4xx_hal.h"

extern uint32_t _estack;
extern uint32_t SystemCoreClock;

#define SBL_SYSTEM_START 0x08040000UL
#define SBL_SYSTEM_END   0x080C0000UL

static volatile uint8_t sbl_usb_ready;

static SBL_CODE void SBL_UsbDisconnectPulse(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  (void)RCC->AHB1ENR;

  GPIOA->MODER &= ~(3UL << (12U * 2U));
  GPIOA->MODER |=  (1UL << (12U * 2U));
  GPIOA->OTYPER &= ~(1UL << 12U);
  GPIOA->PUPDR &= ~(3UL << (12U * 2U));
  GPIOA->BSRR = (1UL << (12U + 16U));
  SBL_DelayMs(80U);
}

static SBL_CODE uint8_t SBL_ClockConfig(void) {
  uint32_t guard;

  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  (void)RCC->APB1ENR;
  PWR->CR |= PWR_CR_VOS;

  RCC->CR |= RCC_CR_HSEON;
  guard = 0x00200000UL;
  while ((RCC->CR & RCC_CR_HSERDY) == 0U) {
    if (--guard == 0U) {
      return 0U;
    }
  }

  if ((RCC->CR & RCC_CR_PLLON) != 0U) {
    RCC->CR &= ~RCC_CR_PLLON;
    guard = 0x00200000UL;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0U) {
      if (--guard == 0U) {
        return 0U;
      }
    }
  }

  FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN |
               FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
  RCC->PLLCFGR = (8UL << RCC_PLLCFGR_PLLM_Pos) |
                 (336UL << RCC_PLLCFGR_PLLN_Pos) |
                 RCC_PLLCFGR_PLLSRC_HSE |
                 (7UL << RCC_PLLCFGR_PLLQ_Pos);

  RCC->CR |= RCC_CR_PLLON;
  guard = 0x00400000UL;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {
    if (--guard == 0U) {
      return 0U;
    }
  }

  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                             RCC_CFGR_PPRE2 | RCC_CFGR_SW)) |
              RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
              RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_SW_PLL;
  guard = 0x00200000UL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    if (--guard == 0U) {
      return 0U;
    }
  }

  RCC->CSR |= RCC_CSR_LSION;
  guard = 0x00080000UL;
  while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U) {
    if (--guard == 0U) {
      break;
    }
  }

  SCB->VTOR = 0x08000000UL;
  SystemCoreClock = 168000000UL;
  if (SysTick_Config(SystemCoreClock / 1000UL) != 0U) {
    return 0U;
  }
  HAL_NVIC_SetPriority(SysTick_IRQn, 0U, 0U);
  return 1U;
}

static SBL_CODE uint8_t SBL_UsbClockStart(void) {
  return SBL_ClockConfig();
}

SBL_CODE uint8_t SBL_AppLooksValid(void) {
  const uint32_t *words = (const uint32_t *)SBL_SYSTEM_START;
  uint32_t non_blank = 0U;
  uint32_t changed = 0U;
  uint32_t prev;

  (void)_estack;

  SBL_FlashFlushCaches();
  prev = words[0];
  for (uint32_t i = 0U; i < 64U; ++i) {
    uint32_t word = words[i];
    if (word != 0xFFFFFFFFUL && word != 0x00000000UL) {
      non_blank++;
    }
    if (word != prev) {
      changed++;
      prev = word;
    }
  }

  if (non_blank < 8U || changed < 4U) {
    return 0U;
  }

  return 1U;
}

SBL_CODE void SBL_Run(void) {
  uint8_t fastboot_requested;
  uint8_t app_valid;

  SBL_SplashRun();
  fastboot_requested = SBL_IsFastbootRequested();
  app_valid = SBL_AppLooksValid();

  if (!fastboot_requested && app_valid) {
    return;
  }

  SBL_LedsOff();
  if (fastboot_requested) {
    SBL_UiDrawFastboot();
    if (SBL_UsbClockStart()) {
      SBL_UsbDisconnectPulse();
      if (SBL_USB_Init()) {
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 6U, 0U);
        sbl_usb_ready = 1U;
      } else {
        SBL_USB_DeInit();
        sbl_usb_ready = 0U;
      }
    }
    SBL_UiRunFastboot();
  }
  if (!app_valid) {
    SBL_UiRunSystemDamage();
  }
  SBL_UiRunFastboot();
}

SBL_CODE void SBL_PreMain(void) {
  SBL_HwBootstrap();
  SBL_LedsOff();
  SystemCoreClock = 16000000UL;
  (void)SysTick_Config(SystemCoreClock / 1000UL);
  SBL_Run();
}
