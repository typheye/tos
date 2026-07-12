/**
 ******************************************************************************
 * @file    splash.c
 * @author  Typheye
 * @brief   SBL splash screen implementation.
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

#include "splash.h"

#if LCD_ENABLED

#include "hw.h"
#include "logo_data.h"

#define SPLASH_LCD_CS_PIN   11U
#define SPLASH_LCD_DC_PIN   12U
#define SPLASH_LCD_RST_PIN  14U
#define SPLASH_LCD_BL_PIN   13U

#define SPLASH_BOARD_LED_PIN 13U
#define SPLASH_WARN_LED_PIN   8U
#define SPLASH_ERROR_LED_PIN  9U
#define SPLASH_F10_PIN       10U

#define SPLASH_UNLOCK_ICON_W      20U
#define SPLASH_UNLOCK_ICON_H      20U
#define SPLASH_UNLOCK_ICON_STRIDE  3U
#define SPLASH_UNLOCK_ICON_Y      23U
#define SPLASH_UNLOCK_ICON_COLOR 0x4208U

static const uint8_t splash_unlock_20x20[60] SBL_CONST = {
    0x00, 0x00, 0x00,  0x00, 0x7C, 0x00,  0x01, 0x86, 0x00,
    0x03, 0x03, 0x00,  0x03, 0x01, 0x80,  0x03, 0x01, 0x80,
    0x03, 0x00, 0x00,  0x03, 0x00, 0x00,  0x03, 0x00, 0x00,
    0x07, 0xFE, 0x00,  0x0F, 0xFF, 0x00,  0x0F, 0xFF, 0x00,
    0x0F, 0x9F, 0x00,  0x0F, 0x0F, 0x00,  0x0F, 0x9F, 0x00,
    0x0F, 0x9F, 0x00,  0x0F, 0xFF, 0x00,  0x07, 0xFE, 0x00,
    0x00, 0x00, 0x00,  0x00, 0x00, 0x00,
};

static SBL_CODE void splash_gpio_set(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << pin);
}

static SBL_CODE void splash_gpio_reset(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << (pin + 16U));
}

static SBL_CODE void splash_outputs_off(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN |
                  RCC_AHB1ENR_GPIOFEN;
  (void)RCC->AHB1ENR;

  GPIOC->MODER &= ~(3UL << (SPLASH_BOARD_LED_PIN * 2U));
  GPIOC->MODER |= (1UL << (SPLASH_BOARD_LED_PIN * 2U));
  GPIOD->MODER &= ~((3UL << (SPLASH_WARN_LED_PIN * 2U)) |
                    (3UL << (SPLASH_ERROR_LED_PIN * 2U)) |
                    (3UL << (SPLASH_LCD_BL_PIN * 2U)));
  GPIOD->MODER |= ((1UL << (SPLASH_WARN_LED_PIN * 2U)) |
                   (1UL << (SPLASH_ERROR_LED_PIN * 2U)) |
                   (1UL << (SPLASH_LCD_BL_PIN * 2U)));
  GPIOF->MODER &= ~(3UL << (SPLASH_F10_PIN * 2U));
  GPIOF->MODER |= (1UL << (SPLASH_F10_PIN * 2U));

  splash_gpio_set(GPIOC, SPLASH_BOARD_LED_PIN);
  splash_gpio_reset(GPIOD, SPLASH_WARN_LED_PIN);
  splash_gpio_reset(GPIOD, SPLASH_ERROR_LED_PIN);
  splash_gpio_reset(GPIOD, SPLASH_LCD_BL_PIN);
  splash_gpio_reset(GPIOF, SPLASH_F10_PIN);
}

static SBL_CODE void splash_backlight_full(void) {
  splash_gpio_set(GPIOD, SPLASH_LCD_BL_PIN);
}

static SBL_CODE void splash_lcd_cmd(uint8_t cmd) {
  splash_gpio_reset(GPIOD, SPLASH_LCD_DC_PIN);
  splash_gpio_reset(GPIOD, SPLASH_LCD_CS_PIN);
  SBL_Spi1Write(cmd);
  splash_gpio_set(GPIOD, SPLASH_LCD_CS_PIN);
}

static SBL_CODE void splash_lcd_data_bytes(const uint8_t *data, uint32_t len) {
  splash_gpio_set(GPIOD, SPLASH_LCD_DC_PIN);
  splash_gpio_reset(GPIOD, SPLASH_LCD_CS_PIN);
  SBL_Spi1WriteBytes(data, len);
  splash_gpio_set(GPIOD, SPLASH_LCD_CS_PIN);
}

static SBL_CODE void splash_lcd_data(uint8_t data) {
  splash_lcd_data_bytes(&data, 1U);
}

static SBL_CODE void splash_lcd_data16(uint16_t data) {
  uint8_t bytes[2] = {(uint8_t)(data >> 8), (uint8_t)data};
  splash_lcd_data_bytes(bytes, sizeof(bytes));
}

static SBL_CODE void splash_lcd_addr(uint16_t x0, uint16_t y0,
                                     uint16_t x1, uint16_t y1) {
  splash_lcd_cmd(0x2AU);
  splash_lcd_data16(x0);
  splash_lcd_data16(x1);
  splash_lcd_cmd(0x2BU);
  splash_lcd_data16(y0);
  splash_lcd_data16(y1);
  splash_lcd_cmd(0x2CU);
}

static SBL_CODE void splash_lcd_fill(uint16_t color) {
  splash_lcd_addr(0U, 0U, SBL_LCD_W - 1U, SBL_LCD_H - 1U);
  splash_gpio_set(GPIOD, SPLASH_LCD_DC_PIN);
  splash_gpio_reset(GPIOD, SPLASH_LCD_CS_PIN);
  for (uint32_t i = 0U; i < (uint32_t)SBL_LCD_W * SBL_LCD_H; ++i) {
    SBL_Spi1Write((uint8_t)(color >> 8));
    SBL_Spi1Write((uint8_t)color);
  }
  splash_gpio_set(GPIOD, SPLASH_LCD_CS_PIN);
}

static SBL_CODE void splash_lcd_pixel(uint16_t x, uint16_t y, uint16_t color) {
  splash_lcd_addr(x, y, x, y);
  splash_lcd_data16(color);
}

static SBL_CODE void splash_draw_unlock_icon(void) {
  const uint16_t x0 = (uint16_t)((SBL_LCD_W - SPLASH_UNLOCK_ICON_W) / 2U);
  const uint16_t y0 = SPLASH_UNLOCK_ICON_Y;

  for (uint16_t y = 0U; y < SPLASH_UNLOCK_ICON_H; ++y) {
    const uint8_t *row =
        &splash_unlock_20x20[(uint32_t)y * SPLASH_UNLOCK_ICON_STRIDE];
    for (uint16_t x = 0U; x < SPLASH_UNLOCK_ICON_W; ++x) {
      uint8_t bit = (uint8_t)(0x80U >> (x & 7U));
      if ((row[x >> 3U] & bit) != 0U) {
        splash_lcd_pixel((uint16_t)(x0 + x), (uint16_t)(y0 + y),
                         SPLASH_UNLOCK_ICON_COLOR);
      }
    }
  }
}

static SBL_CODE void splash_lcd_init(void) {
  SBL_Spi1InitForLcd();
  splash_gpio_set(GPIOD, SPLASH_LCD_CS_PIN);
  splash_gpio_reset(GPIOD, SPLASH_LCD_DC_PIN);
  splash_gpio_set(GPIOD, SPLASH_LCD_RST_PIN);
  SBL_DelayMs(10U);
  splash_gpio_reset(GPIOD, SPLASH_LCD_RST_PIN);
  SBL_DelayMs(10U);
  splash_gpio_set(GPIOD, SPLASH_LCD_RST_PIN);
  SBL_DelayMs(120U);

  splash_lcd_cmd(0x01U); SBL_DelayMs(150U);
  splash_lcd_cmd(0x11U); SBL_DelayMs(120U);
  splash_lcd_cmd(0x3AU); splash_lcd_data(0x55U);
  splash_lcd_cmd(0x36U); splash_lcd_data(0x10U);
  splash_lcd_cmd(0xB2U);
  { uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; splash_lcd_data_bytes(d, sizeof(d)); }
  splash_lcd_cmd(0xB7U); splash_lcd_data(0x35U);
  splash_lcd_cmd(0xBBU); splash_lcd_data(0x19U);
  splash_lcd_cmd(0xC0U); splash_lcd_data(0x2CU);
  splash_lcd_cmd(0xC2U); splash_lcd_data(0x01U);
  splash_lcd_cmd(0xC3U); splash_lcd_data(0x12U);
  splash_lcd_cmd(0xC4U); splash_lcd_data(0x20U);
  splash_lcd_cmd(0xC6U); splash_lcd_data(0x0FU);
  splash_lcd_cmd(0xD0U);
  { uint8_t d[] = {0xA4, 0xA1}; splash_lcd_data_bytes(d, sizeof(d)); }
  splash_lcd_cmd(0x21U);
  splash_lcd_cmd(0x29U);
  SBL_DelayMs(100U);
  splash_lcd_fill(SBL_BLACK);
  SBL_DelayMs(100U);
}

static SBL_CODE void splash_draw_logo(void) {
  for (uint32_t i = 0U; i < LOGO_PIXEL_COUNT; ++i) {
    splash_lcd_pixel(logo_pixels[i].x, logo_pixels[i].y, logo_pixels[i].color);
  }
}

SBL_CODE void SBL_SplashRun(void) {
  splash_outputs_off();
  splash_lcd_init();
  splash_lcd_fill(SBL_BLACK);
  splash_draw_logo();
  splash_backlight_full();
  SBL_DelayMs(1600U);
}

#endif /* LCD_ENABLED */
