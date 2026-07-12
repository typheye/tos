/**
 ******************************************************************************
 * @file    init.c
 * @author  Typheye
 * @brief   SBL boot initialization and routing implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
#include "init.h"

#include <stdint.h>

#include "common.h"
#include "hw.h"
#include "lcd.h"
#include "splash.h"
#include "state.h"
#include "ui.h"
#include "usb.h"
#include "stm32f4xx_hal.h"
#include "manifest.h"
#include "secure_boot.h"

extern uint32_t SystemCoreClock;
static volatile uint8_t sbl_usb_ready;

static SBL_CODE uint8_t SBL_ClockConfig(void) {
  uint32_t guard;
  uint32_t pll_m = 8UL;
  uint32_t pll_source = RCC_PLLCFGR_PLLSRC_HSE;

  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  (void)RCC->APB1ENR;
  PWR->CR |= PWR_CR_VOS;
  RCC->CR |= RCC_CR_HSEON;
  guard = 0x00200000UL;
  while ((RCC->CR & RCC_CR_HSERDY) == 0U) {
    if (--guard == 0U) {
      RCC->CR &= ~RCC_CR_HSEON;
      RCC->CR |= RCC_CR_HSION;
      guard = 0x00080000UL;
      while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {
        if (--guard == 0U) return 0U;
      }
      pll_m = 16UL;
      pll_source = 0UL;
      break;
    }
  }
  if ((RCC->CR & RCC_CR_PLLON) != 0U) {
    RCC->CR &= ~RCC_CR_PLLON;
    guard = 0x00200000UL;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0U) {
      if (--guard == 0U) return 0U;
    }
  }
  FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN |
               FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
  RCC->PLLCFGR = (pll_m << RCC_PLLCFGR_PLLM_Pos) |
                 (336UL << RCC_PLLCFGR_PLLN_Pos) | pll_source |
                 (7UL << RCC_PLLCFGR_PLLQ_Pos);
  RCC->CR |= RCC_CR_PLLON;
  guard = 0x00400000UL;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {
    if (--guard == 0U) return 0U;
  }
  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                             RCC_CFGR_PPRE2 | RCC_CFGR_SW)) |
              RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
              RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_SW_PLL;
  guard = 0x00200000UL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    if (--guard == 0U) return 0U;
  }
  SCB->VTOR = TOS_PART_SBL_ADDRESS;
  SystemCoreClock = 168000000UL;
  if (SysTick_Config(SystemCoreClock / 1000UL) != 0U) return 0U;
  HAL_NVIC_SetPriority(SysTick_IRQn, 0U, 0U);
  return 1U;
}

static SBL_CODE uint8_t image_vector_valid(uint32_t address, uint32_t size) {
  const uint32_t *v = (const uint32_t *)address;
  uint32_t sp;
  uint32_t pc;
  SBL_FlashFlushCaches();
  sp = v[0];
  pc = v[1];
  if (sp < 0x20000000UL || sp > 0x20020000UL || (sp & 7U) != 0U) return 0U;
  if ((pc & 1U) == 0U) return 0U;
  pc &= ~1UL;
  return pc >= address && pc < address + size ? 1U : 0U;
}

static SBL_CODE void jump_to_image(uint32_t address) {
  const uint32_t *v = (const uint32_t *)address;
  void (*entry)(void) = (void (*)(void))(uintptr_t)v[1];
  SBL_USB_DeInit();
  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
  for (uint32_t i = 0U; i < 8U; ++i) {
    NVIC->ICER[i] = 0xFFFFFFFFUL;
    NVIC->ICPR[i] = 0xFFFFFFFFUL;
  }
  SCB->VTOR = address;
  __set_MSP(v[0]);
  __set_CONTROL(0U);
  __DSB();
  __ISB();
  entry();
  while (1) __NOP();
}

SBL_CODE uint8_t SBL_AppLooksValid(void) {
  if (!image_vector_valid(TOS_PART_SYSTEM_ADDRESS, TOS_PART_SYSTEM_SIGNED_SIZE))
    return 0U;
  if (SBL_StateUnlocked())
    return 1U;
  return SecureBoot_Verify(TOS_PART_SYSTEM_ADDRESS, TOS_PART_SYSTEM_ADDRESS,
                           TOS_PART_SYSTEM_SIGNED_SIZE, TOS_IMAGE_TYPE_SYSTEM, 1U) ==
                 SECUREBOOT_VERIFY_OK
             ? 1U
             : 0U;
}

static SBL_CODE uint8_t SBL_RecLooksValid(void) {
  if (!image_vector_valid(TOS_PART_REC_ADDRESS, TOS_PART_REC_SIZE))
    return 0U;
  if (SBL_StateUnlocked())
    return 1U;
  return SecureBoot_Verify(TOS_PART_REC_ADDRESS,
                             TOS_PART_REC_ADDRESS,
                             TOS_PART_REC_SIGNED_SIZE,
                             TOS_IMAGE_TYPE_REC, 1U) == SECUREBOOT_VERIFY_OK
             ? 1U
             : 0U;
}

SBL_CODE void SBL_Run(void) {
  uint8_t fastboot_requested;
  uint8_t app_valid;
  uint32_t boot_target;

  SBL_SplashRun();
  boot_target = SBL_StatePeekBootTarget();
  if (boot_target == SBL_BOOT_TARGET_RECOVERY ||
      boot_target == SBL_BOOT_TARGET_RECOVERY_FORMAT ||
      boot_target == SBL_BOOT_TARGET_RECOVERY_UPGRADE ||
      boot_target == SBL_BOOT_TARGET_RECOVERY_INIT) {
    if (!SBL_RecLooksValid()) {
      /* Recovery boot targets are one-shot requests.  If REC itself has been
       * erased or damaged, clear the request before showing the exception page
       * so the 5-second reboot returns to the normal SYSTEM boot path instead
       * of looping forever on the stale REC flag. */
      (void)SBL_StateConsumeBootTarget();
      SBL_UiRunRecoveryException();
    }
    jump_to_image(TOS_PART_REC_ADDRESS);
  }

  if (boot_target == SBL_BOOT_TARGET_FASTBOOT) {
    (void)SBL_StateConsumeBootTarget();
  }
  fastboot_requested = boot_target == SBL_BOOT_TARGET_FASTBOOT ? 1U
                                                               : SBL_IsFastbootRequested();
  app_valid = SBL_AppLooksValid();

  /* A damaged or erased SYSTEM must remain visibly distinguishable from an
   * explicit FASTBOOT request.  The old code converted every invalid SYSTEM
   * into fastboot_requested=1, which made the SYSTEM DAMAGE page unreachable
   * after `erase system` + `reboot`.  Only the button or an explicit boot
   * target may enter FASTBOOT; otherwise stay on the non-rebooting damage
   * page so the user can decide when to recover the device. */
  if (!fastboot_requested) {
    if (app_valid) {
      jump_to_image(TOS_PART_SYSTEM_ADDRESS);
    }
    SBL_LedsOff();
    SBL_UiRunSystemDamage(SBL_StateUnlocked() ? SECUREBOOT_VERIFY_OK
        : SecureBoot_Verify(TOS_PART_SYSTEM_ADDRESS, TOS_PART_SYSTEM_ADDRESS,
                              TOS_PART_SYSTEM_SIGNED_SIZE, TOS_IMAGE_TYPE_SYSTEM, 1U));
  }

  SBL_LedsOff();
  if (fastboot_requested) {
    SBL_StatusLedOn();
    SBL_UiDrawFastboot();
    if (SBL_ClockConfig()) {
      SBL_UartInit();
      SBL_USB_DisconnectPulse();
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
}

void SBL_Main(void) {
  SCB->VTOR = TOS_PART_SBL_ADDRESS;
  __enable_irq();
  SBL_HwBootstrap();
  SBL_LedsOff();
  SystemCoreClock = 16000000UL;
  (void)SysTick_Config(SystemCoreClock / 1000UL);
  SBL_Run();
  while (1) __NOP();
}
