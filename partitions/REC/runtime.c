#include "rec.h"
#include "sbl_common.h"
#include "sbl_hw.h"
#include "stm32f4xx_hal.h"
#include "tos_partitions.h"
#include "usbd_conf.h"

extern uint32_t SystemCoreClock;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

/*
 * Keep the USB device electrically absent from the host for the whole REC
 * preparation phase.  Merely delaying USBD_Start() is not sufficient when
 * REC is entered from a stage that previously used OTG FS: the peripheral or
 * D+ pull-up may remain active long enough for Windows to create an instance
 * and begin its media probe.  Replacing that instance a moment later with MSC
 * triggers the host's long reset/retry path.
 *
 * This routine runs before interrupts and before any REC USB/HAL setup.  It
 * resets and gates OTG FS, then drives PA12 (D+) low.  PA12 remains low until
 * USBD_Init() configures the pin for AF10 and USBD_Start() deliberately
 * connects the fully prepared CDC/TDB device.
 */
static REC_CODE void rec_usb_hold_disconnected(void) {
  NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);

  RCC->AHB2RSTR |= RCC_AHB2RSTR_OTGFSRST;
  __DSB();
  RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;
  RCC->AHB2ENR &= ~RCC_AHB2ENR_OTGFSEN;

  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  (void)RCC->AHB1ENR;
  GPIOA->MODER &= ~(3UL << (12U * 2U));
  GPIOA->MODER |= (1UL << (12U * 2U));
  GPIOA->OTYPER &= ~(1UL << 12U);
  GPIOA->OSPEEDR |= (3UL << (12U * 2U));
  GPIOA->PUPDR &= ~(3UL << (12U * 2U));
  GPIOA->BSRR = (1UL << (12U + 16U));
  __DSB();
}

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
  /* USB CDC traffic must never starve the millisecond timebase used by HAL
   * SDIO timeouts.  TDB callbacks only move endpoint bytes, but keeping
   * SysTick one preemption level above OTG_FS makes this invariant explicit. */
  HAL_NVIC_SetPriority(SysTick_IRQn, 5U, 0U);
  return 1U;
}

void REC_RuntimeMain(void) {
  uint8_t ok;
  SCB->VTOR = TOS_PART_REC_ADDRESS;
  rec_usb_hold_disconnected();
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
  /* Full-speed hosts only need a brief D+ disconnect to force a clean new
   * enumeration.  Twenty milliseconds is comfortably above the USB minimum
   * while avoiding an unnecessary visible startup pause. */
  SBL_DelayMs(20U);
}
