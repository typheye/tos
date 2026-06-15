#include "sbl_hw.h"

#include "gpio.h"
#include "spi.h"

#define SBL_LCD_SCK_PIN  3U
#define SBL_LCD_MOSI_PIN 5U
#define SBL_LCD_CS_PIN   11U
#define SBL_LCD_DC_PIN   12U
#define SBL_LCD_BL_PIN   13U
#define SBL_LCD_RST_PIN  14U

SBL_CODE void SBL_HwBootstrap(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                  RCC_AHB1ENR_GPIODEN;
  (void)RCC->AHB1ENR;

  SBL_BTN_PORT->MODER &= ~(3UL << (SBL_BTN_PIN * 2U));
  SBL_BTN_PORT->PUPDR &= ~(3UL << (SBL_BTN_PIN * 2U));
  SBL_BTN_PORT->PUPDR |=  (1UL << (SBL_BTN_PIN * 2U));
}

SBL_CODE uint8_t SBL_IsFastbootRequested(void) {
  SBL_DelayMs(700U);
  uint8_t pressed = 0U;
  for (uint8_t i = 0; i < 12U; ++i) {
    if ((SBL_BTN_PORT->IDR & (1UL << SBL_BTN_PIN)) == 0U) {
      pressed++;
    }
    SBL_DelayMs(20U);
  }
  return pressed >= 9U;
}

SBL_CODE uint8_t SBL_IsButtonDown(void) {
  return HAL_GPIO_ReadPin(SBL_BTN_PORT, (uint16_t)(1UL << SBL_BTN_PIN)) ==
         GPIO_PIN_RESET;
}

SBL_CODE void SBL_WaitButtonRelease(uint32_t settle_ms) {
  while (SBL_IsButtonDown()) {
    SBL_DelayMs(10U);
  }
  if (settle_ms > 0U) {
    SBL_DelayMs(settle_ms);
  }
}

SBL_CODE void SBL_LedsOff(void) {
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, GPIO_PIN_RESET);
}

SBL_CODE void SBL_SystemReboot(void) {
  __disable_irq();
  NVIC_SystemReset();
  while (1) {
    __NOP();
  }
}

SBL_CODE void SBL_Spi1InitForLcd(void) {
  SBL_GpioSet(GPIOD, SBL_LCD_CS_PIN);
  SBL_GpioReset(GPIOD, SBL_LCD_DC_PIN);
  SBL_GpioSet(GPIOD, SBL_LCD_RST_PIN);
}

SBL_CODE void SBL_Spi1Write(uint8_t v) {
  (void)HAL_SPI_Transmit(&hspi1, &v, 1U, 100U);
}

SBL_CODE void SBL_Spi1WriteBytes(const uint8_t *data, uint32_t len) {
  while (len > 0U) {
    uint16_t chunk = len > 0xFFFFU ? 0xFFFFU : (uint16_t)len;
    (void)HAL_SPI_Transmit(&hspi1, (uint8_t *)data, chunk, 1000U);
    data += chunk;
    len -= chunk;
  }
}
