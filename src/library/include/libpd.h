#ifndef __LIBPD_H
#define __LIBPD_H

#include "include/lcd.h"
#include "include/sysfonts.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 字体大小枚举
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

// 初始化
void PD_Init(void);

// 绘图设置
void PD_SetColor(uint32_t color);
void PD_SetBgColor(uint32_t color);
void PD_SetFill(bool fill);

// 基本图形（绘制到帧缓冲区）
void PD_DrawPixel(int16_t x, int16_t y);
void PD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void PD_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h);
void PD_DrawCircle(int16_t x0, int16_t y0, int16_t radius);
void PD_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void PD_FillScreen(uint32_t color);

// 文字绘制
void PD_SetFont(FontSize_t font);
void PD_DrawChar(int16_t x, int16_t y, char ch);
void PD_DrawString(int16_t x, int16_t y, const char *str);
uint16_t PD_GetStringWidth(const char *str);
uint16_t PD_GetCharWidth(void);
uint16_t PD_GetCharHeight(void);

#ifdef __cplusplus
}
#endif

#endif