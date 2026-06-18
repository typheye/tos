#include "rec.h"
#include "sbl_common.h"
#include "sbl_hw.h"
#include "stm32f4xx_hal.h"
#include "tos_partitions.h"
#include "usbd_conf.h"

extern uint32_t SystemCoreClock;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

static uint8_t rec_clock_config(void) {
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
      while ((RCC->CR & RCC_CR_HSIRDY) == 0U) if (--guard == 0U) return 0U;
      pll_m = 16UL;
      pll_source = 0UL;
      break;
    }
  }
  RCC->CR &= ~RCC_CR_PLLON;
  guard = 0x00200000UL;
  while ((RCC->CR & RCC_CR_PLLRDY) != 0U) if (--guard == 0U) return 0U;
  FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN |
               FLASH_ACR_LATENCY_5WS;
  RCC->PLLCFGR = (pll_m << RCC_PLLCFGR_PLLM_Pos) |
                 (336UL << RCC_PLLCFGR_PLLN_Pos) | pll_source |
                 (7UL << RCC_PLLCFGR_PLLQ_Pos);
  RCC->CR |= RCC_CR_PLLON;
  guard = 0x00400000UL;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) if (--guard == 0U) return 0U;
  RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                             RCC_CFGR_PPRE2 | RCC_CFGR_SW)) |
              RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
              RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_SW_PLL;
  guard = 0x00200000UL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) if (--guard == 0U) return 0U;
  SCB->VTOR = TOS_PART_REC_ADDRESS;
  SystemCoreClock = 168000000UL;
  if (SysTick_Config(SystemCoreClock / 1000UL) != 0U) {
    return 0U;
  }
  /* USB bulk traffic must never starve the millisecond timebase used by HAL
   * SDIO timeouts.  USB callbacks are short after the MSC rework, but keeping
   * SysTick one preemption level above OTG_FS makes this invariant explicit. */
  HAL_NVIC_SetPriority(SysTick_IRQn, 5U, 0U);
  return 1U;
}

void REC_RuntimeMain(void) {
  uint8_t ok;
  SCB->VTOR = TOS_PART_REC_ADDRESS;
  __enable_irq();
  SBL_HwBootstrap();
  SBL_LedsOff();
  SystemCoreClock = 16000000UL;
  (void)SysTick_Config(SystemCoreClock / 1000UL);
  HAL_NVIC_SetPriority(SysTick_IRQn, 5U, 0U);
  ok = rec_clock_config();
  REC_Main(ok);
}

void SysTick_Handler(void) { HAL_IncTick(); }
void OTG_FS_IRQHandler(void) { HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); }
void Error_Handler(void) { __disable_irq(); while (1) __NOP(); }

SBL_CODE void SBL_USB_DisconnectPulse(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  (void)RCC->AHB1ENR;
  GPIOA->MODER &= ~(3UL << (12U * 2U));
  GPIOA->MODER |= (1UL << (12U * 2U));
  GPIOA->OTYPER &= ~(1UL << 12U);
  GPIOA->PUPDR &= ~(3UL << (12U * 2U));
  GPIOA->BSRR = (1UL << (12U + 16U));
  SBL_DelayMs(80U);
}
