/**
 ******************************************************************************
 * @file    lcd.h
 * @author  Typheye
 * @brief   Lcd interface.
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

#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#include "stm32f4xx_hal.h"
#ifdef __cplusplus
#include "hardware/include/tcs3472.hpp"
#endif
#include "include/libpd.h"
#include "include/libemo.h"
#include "tim.h"
#include "core/sys/include/syslog.h"
#include <stdio.h>


#define LCD_WIDTH 240
#define LCD_HEIGHT 240
#define TILE_HEIGHT 60



#define LCD_SCK_PIN GPIO_PIN_3
#define LCD_SCK_PORT GPIOB
#define LCD_SDA_PIN GPIO_PIN_5
#define LCD_SDA_PORT GPIOB


#define LCD_CS_PIN GPIO_PIN_11
#define LCD_CS_PORT GPIOD
#define LCD_DC_PIN GPIO_PIN_12
#define LCD_DC_PORT GPIOD
#define LCD_BL_PIN GPIO_PIN_13
#define LCD_BL_PORT GPIOD
#define LCD_RST_PIN GPIO_PIN_14
#define LCD_RST_PORT GPIOD


#define LCD_CS_L HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_H HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_DC_CMD HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_DATA HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)
#define LCD_BL_ON HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET)
#define LCD_BL_OFF HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_RESET)
#define LCD_RST_L HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_H HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)


#define LCD_COLOR_BLACK 0x000000
#define LCD_COLOR_WHITE 0xFFFFFF
#define LCD_COLOR_RED 0xFF0000
#define LCD_COLOR_GREEN 0x00FF00
#define LCD_COLOR_BLUE 0x0000FF
#define LCD_COLOR_YELLOW 0xFFFF00
#define LCD_COLOR_CYAN 0x00FFFF
#define LCD_COLOR_MAGENTA 0xFF00FF
#define LCD_COLOR_GRAY 0x808080
#define LCD_COLOR_ORANGE 0xFFA500
#define LCD_COLOR_PINK 0xFFC0CB
#define LCD_COLOR_BROWN 0x8B4513
#define LCD_COLOR_PURPLE 0x800080
#define LCD_COLOR_DARK_BLUE 0x000080


#ifdef __cplusplus
extern "C" {
#endif


void LCD_Init(void);
void LCD_SetSahSplashPreserve(uint8_t preserve);
void LCD_FillScreen(uint32_t color);
void LCD_DrawPixel(int16_t x, int16_t y, uint32_t color);
void LCD_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void LCD_Flush(void);


uint16_t *LCD_GetFrameBuffer(void);
void LCD_ClearFrameBuffer(uint32_t color);


void LCD_BeginTileRender(uint16_t y, uint16_t h);
void LCD_EndTileRender(void);
void LCD_FlushTiled(void (*render_cb)(void));
void LCD_SetDebugOverlaySuppressed(uint8_t suppressed);
/* Emergency/fatal-path drawing: blocking transfer, no DMA, no full LCD re-init. */
void LCD_EmergencyPrepare(void);
void LCD_FlushTiledBlocking(void (*render_cb)(void));
void LCD_FlushFull(const uint16_t *data);
uint16_t LCD_GetTileY(void);
uint16_t LCD_GetTileH(void);


void LCD_UpdateAutoBrightness(void);
uint16_t LCD_RGB888ToRGB565(uint32_t rgb888);
uint16_t LCD_GetWidth(void);
uint16_t LCD_GetHeight(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_CONFIG_H */
