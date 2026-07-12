/**
 ******************************************************************************
 * @file    hw.c
 * @author  Typheye
 * @brief   SBL hardware abstraction (GPIO, UART, LED) implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "hw.h"

#include "state.h"

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
  uint8_t pressed = 0U;
  for (uint8_t i = 0; i < 12U; ++i) {
    if ((SBL_BTN_PORT->IDR & (1UL << SBL_BTN_PIN)) == 0U) {
      pressed++;
    }
    SBL_DelayMs(10U);
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

#define SBL_STATUS_LED_PORT GPIOC
#define SBL_STATUS_LED_PIN  13U

SBL_CODE void SBL_StatusLedOn(void) {
  SBL_GpioReset(SBL_STATUS_LED_PORT, SBL_STATUS_LED_PIN);
}

SBL_CODE void SBL_StatusLedOff(void) {
  SBL_GpioSet(SBL_STATUS_LED_PORT, SBL_STATUS_LED_PIN);
}

SBL_CODE void SBL_SystemReboot(void) {
  (void)SBL_StateSetRestart(SBL_BOOT_TARGET_NONE);
  __disable_irq();
  NVIC_SystemReset();
  while (1) { __NOP(); }
}

SBL_CODE void SBL_SystemRebootTo(uint32_t boot_target) {
  (void)SBL_StateSetRestart(boot_target);
  __disable_irq();
  NVIC_SystemReset();
  while (1) { __NOP(); }
}

/* ── USART1 serial console (PA9 TX, PA10 RX, 115200-8N1) ────────── */
#if SBL_UART_ENABLED

#define SBL_UART USART1
#define SBL_UART_AF 7U

SBL_CODE void SBL_UartInit(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  (void)RCC->AHB1ENR;
  (void)RCC->APB2ENR;

  /* PA9 = TX (AF7).  Board routes USART1_RX to PB7 -- we only need TX. */
  GPIOA->MODER &= ~(3UL << (9U * 2U));
  GPIOA->MODER |=  (2UL << (9U * 2U));
  GPIOA->OSPEEDR |= (3UL << (9U * 2U));
  GPIOA->PUPDR &= ~(3UL << (9U * 2U));
  GPIOA->AFR[1] &= ~(0xFUL << ((9U - 8U) * 4U));
  GPIOA->AFR[1] |=  (SBL_UART_AF << ((9U - 8U) * 4U));

  /* 115200-8N1, 16x oversampling.  APB2 = 84 MHz:
   *   USARTDIV = f_CK / (16 * BaudRate)
   *            = 84 000 000 / (16 * 115 200)
   *            = 84 000 000 / 1 843 200
   *            = 45.5729... -> mantissa 45, fraction 9/16 */
  SBL_UART->CR1 = 0U;
  SBL_UART->BRR = (45UL << 4U) | 9UL;
  SBL_UART->CR1 = USART_CR1_TE | USART_CR1_UE;
}

SBL_CODE void SBL_UartWrite(uint8_t byte) {
  while ((SBL_UART->SR & USART_SR_TXE) == 0U) {}
  SBL_UART->DR = byte;
}

SBL_CODE void SBL_UartWriteText(const char *text) {
  if (!text) return;
  while (*text) {
    SBL_UartWrite((uint8_t)*text++);
  }
}

SBL_CODE void SBL_UartWriteLine(const char *text) {
  SBL_UartWriteText(text);
  SBL_UartWrite((uint8_t)'\r');
  SBL_UartWrite((uint8_t)'\n');
}

#endif /* SBL_UART_ENABLED */

#if LCD_ENABLED

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

#endif /* LCD_ENABLED */
