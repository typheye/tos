/**
 ******************************************************************************
 * @file    libpd.h
 * @author  Typheye
 * @brief   Libpd interface.
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

#ifndef LIBPD_H
#define LIBPD_H

#include "core/sys/include/sysfonts.h"
#include "core/sys/include/syslog.h"
#include "include/lcd.h"
#include "sysfonts.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOS_BG 0x000000
#define TOS_CARD_BG 0x111111
#define TOS_ACCENT 0x1f4648
#define TOS_ACCENT2 0x0D1F20
#define TOS_TEXT 0xFFFFFF
#define TOS_TEXT_SEC 0xCDCDCD
#define TOS_GREY 0x555555
#define TOS_GREEN 0x00FF00
#define TOS_RED 0xF00000
#define TOS_YELLOW 0xFFFF00

#define LV_BG_DARK TOS_BG
#define LV_BG_CARD TOS_CARD_BG
#define LV_BG_CARD2 0x1A1A1A
#define LV_BORDER 0x1f4648
#define LV_PRIMARY TOS_ACCENT
#define LV_PRIMARY_DARK 0x0D1F20
#define LV_ACCENT TOS_ACCENT
#define LV_SUCCESS TOS_GREEN
#define LV_WARNING TOS_YELLOW
#define LV_ERROR TOS_RED
#define LV_TEXT_PRIMARY TOS_TEXT
#define LV_TEXT_SECONDARY TOS_TEXT_SEC
#define LV_TEXT_HINT 0x6A6A6A

typedef enum {
  FONT_ASCII_12 = 0,
  FONT_ASCII_16 = 1,
  FONT_ASCII_20 = 2,
  FONT_ASCII_24 = 3,
  FONT_ASCII_32 = 4,
  FONT_CH_12 = 5,
  FONT_CH_16 = 6,
  FONT_CH_20 = 7,
  FONT_CH_24 = 8,
  FONT_CH_32 = 9
} FontSize_t;

void PD_Init(void);
void PD_SetTileWindow(uint16_t y, uint16_t h);

void PD_SetColor(uint32_t color);
void PD_SetBgColor(uint32_t color);
void PD_SetFill(bool fill);

void PD_DrawPixel(int16_t x, int16_t y);
void PD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void PD_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h);
void PD_DrawCircle(int16_t x0, int16_t y0, int16_t radius);
void PD_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void PD_FillScreen(uint32_t color);

void PD_SetFont(FontSize_t font);
void PD_DrawChar(int16_t x, int16_t y, char ch);
void PD_DrawString(int16_t x, int16_t y, const char *str);
void PD_DrawStringCentered(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char *str);
uint16_t PD_GetStringWidth(const char *str);
uint16_t PD_GetCharWidth(void);
uint16_t PD_GetCharHeight(void);

void PD_DrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r);
void PD_DrawCard(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                 uint32_t bg_color, uint32_t border_color, int16_t border_w);
void PD_DrawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                        float percent, uint32_t bar_color, uint32_t bg_color);

void PD_DrawPolygon(const int16_t *points, uint16_t num_points, uint32_t color);

void PD_DrawPolygonOutline(const int16_t *points, uint16_t num_points);

void PD_DrawFrame(void);
void PD_SetHeaderTime(const char *time_str);

void PD_DrawHeader(const char *title);
void PD_DrawHeaderWithTime(const char *title, const char *time_str);

void PD_DrawFooter(const char *left_text, const char *right_text);
void PD_DrawFooterCenter(const char *left_text, const char *center_text,
                         const char *right_text);

void PD_DrawAngledCard(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint32_t color);

void PD_DrawWifiIcon(int16_t x, int16_t y, bool connected);
void PD_DrawEthIcon(int16_t x, int16_t y, bool connected);
void PD_DrawSignalIcon(int16_t x, int16_t y, int signal);

void PD_ShowSplashFadeStart(uint32_t fade_in_ms);
void PD_SplashTick(void);
void PD_SplashFinish(uint32_t fade_out_ms);
uint8_t PD_IsSplashActive(void);

#ifdef __cplusplus
}
#endif

#endif
