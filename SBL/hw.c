#include "sbl_hw.h"

#define SBL_LCD_SCK_PIN  3U
#define SBL_LCD_MISO_PIN 4U
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
  return (SBL_BTN_PORT->IDR & (1UL << SBL_BTN_PIN)) == 0U;
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
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                  RCC_AHB1ENR_GPIOFEN;
  (void)RCC->AHB1ENR;

  GPIOC->MODER &= ~(3UL << (13U * 2U));
  GPIOC->MODER |=  (1UL << (13U * 2U));
  GPIOD->MODER &= ~((3UL << (8U * 2U)) | (3UL << (9U * 2U)));
  GPIOD->MODER |=  ((1UL << (8U * 2U)) | (1UL << (9U * 2U)));
  GPIOF->MODER &= ~(3UL << (10U * 2U));
  GPIOF->MODER |=  (1UL << (10U * 2U));

  SBL_GpioSet(GPIOC, 13U);
  SBL_GpioReset(GPIOD, 8U);
  SBL_GpioReset(GPIOD, 9U);
  SBL_GpioReset(GPIOF, 10U);
}

SBL_CODE void SBL_SystemReboot(void) {
  __disable_irq();
  NVIC_SystemReset();
  while (1) {
    __NOP();
  }
}

SBL_CODE void SBL_Spi1InitForLcd(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
  (void)RCC->AHB1ENR;
  (void)RCC->APB2ENR;

  GPIOB->MODER &= ~((3UL << (SBL_LCD_SCK_PIN * 2U)) |
                    (3UL << (SBL_LCD_MISO_PIN * 2U)) |
                    (3UL << (SBL_LCD_MOSI_PIN * 2U)));
  GPIOB->MODER |=  ((2UL << (SBL_LCD_SCK_PIN * 2U)) |
                    (2UL << (SBL_LCD_MISO_PIN * 2U)) |
                    (2UL << (SBL_LCD_MOSI_PIN * 2U)));
  GPIOB->OSPEEDR |= ((3UL << (SBL_LCD_SCK_PIN * 2U)) |
                     (3UL << (SBL_LCD_MISO_PIN * 2U)) |
                     (3UL << (SBL_LCD_MOSI_PIN * 2U)));
  GPIOB->PUPDR &= ~((3UL << (SBL_LCD_SCK_PIN * 2U)) |
                    (3UL << (SBL_LCD_MISO_PIN * 2U)) |
                    (3UL << (SBL_LCD_MOSI_PIN * 2U)));
  GPIOB->AFR[0] &= ~((0xFUL << (SBL_LCD_SCK_PIN * 4U)) |
                     (0xFUL << (SBL_LCD_MISO_PIN * 4U)) |
                     (0xFUL << (SBL_LCD_MOSI_PIN * 4U)));
  GPIOB->AFR[0] |=  ((5UL << (SBL_LCD_SCK_PIN * 4U)) |
                     (5UL << (SBL_LCD_MISO_PIN * 4U)) |
                     (5UL << (SBL_LCD_MOSI_PIN * 4U)));

  GPIOD->MODER &= ~((3UL << (SBL_LCD_CS_PIN * 2U)) |
                    (3UL << (SBL_LCD_DC_PIN * 2U)) |
                    (3UL << (SBL_LCD_BL_PIN * 2U)) |
                    (3UL << (SBL_LCD_RST_PIN * 2U)));
  GPIOD->MODER |=  ((1UL << (SBL_LCD_CS_PIN * 2U)) |
                    (1UL << (SBL_LCD_DC_PIN * 2U)) |
                    (1UL << (SBL_LCD_BL_PIN * 2U)) |
                    (1UL << (SBL_LCD_RST_PIN * 2U)));
  GPIOD->OTYPER &= ~((1UL << SBL_LCD_CS_PIN) |
                     (1UL << SBL_LCD_DC_PIN) |
                     (1UL << SBL_LCD_BL_PIN) |
                     (1UL << SBL_LCD_RST_PIN));
  GPIOD->OSPEEDR |= ((3UL << (SBL_LCD_CS_PIN * 2U)) |
                     (3UL << (SBL_LCD_DC_PIN * 2U)) |
                     (3UL << (SBL_LCD_BL_PIN * 2U)) |
                     (3UL << (SBL_LCD_RST_PIN * 2U)));
  GPIOD->PUPDR &= ~((3UL << (SBL_LCD_CS_PIN * 2U)) |
                    (3UL << (SBL_LCD_DC_PIN * 2U)) |
                    (3UL << (SBL_LCD_BL_PIN * 2U)) |
                    (3UL << (SBL_LCD_RST_PIN * 2U)));

  SPI1->CR1 = 0U;
  SPI1->CR2 = 0U;
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;
  SPI1->CR1 |= SPI_CR1_SPE;

  SBL_GpioSet(GPIOD, SBL_LCD_CS_PIN);
  SBL_GpioReset(GPIOD, SBL_LCD_DC_PIN);
  SBL_GpioSet(GPIOD, SBL_LCD_RST_PIN);
}

SBL_CODE void SBL_Spi1Write(uint8_t v) {
  while ((SPI1->SR & SPI_SR_TXE) == 0U) {
  }
  *(__IO uint8_t *)&SPI1->DR = v;
  while ((SPI1->SR & SPI_SR_TXE) == 0U) {
  }
  while (SPI1->SR & SPI_SR_BSY) {
  }
}

SBL_CODE void SBL_Spi1WriteBytes(const uint8_t *data, uint32_t len) {
  while (len > 0U) {
    SBL_Spi1Write(*data++);
    len--;
  }
}
