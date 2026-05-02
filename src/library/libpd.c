#include "include/libpd.h"
#include "include/sysfonts.h"
#include <math.h> // 添加：sin, cos, sqrt 等（如果需要）
#include <stdio.h>
#include <stdlib.h> // 添加：abs() 函数
#include <string.h> // 添加：memset 等


#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

// ==================== 全局变量 ====================
static CCMRAM uint32_t pd_color = 0xFFFFFF;
static CCMRAM uint32_t pd_bg_color = 0x000000;
static CCMRAM bool pd_fill = false;

static pFONT *current_ascii_font = NULL;
static CCMRAM uint16_t pd_char_buffer[1024]; // 2KB

// 帧缓冲区（直接使用 LCD 的缓冲区）
static uint16_t *g_fb = NULL;
static uint16_t g_fb_width = 0;
static uint16_t g_fb_height = 0;

// ==================== 内部函数 ====================

static inline void fb_set_pixel(int16_t x, int16_t y, uint16_t color_565) {
  if (g_fb && x >= 0 && x < g_fb_width && y >= 0 && y < g_fb_height) {
    g_fb[y * g_fb_width + x] = color_565;
  }
}

static uint16_t color_to_565(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// ==================== 初始化 ====================
void PD_Init(void) {
  current_ascii_font = &ASCII_Font16;

  // 获取 LCD 帧缓冲区
  g_fb = LCD_GetFrameBuffer();
  g_fb_width = LCD_GetWidth();
  g_fb_height = LCD_GetHeight();
}

// ==================== 设置函数 ====================
void PD_SetColor(uint32_t color) { pd_color = color; }
void PD_SetBgColor(uint32_t color) { pd_bg_color = color; }
void PD_SetFill(bool fill) { pd_fill = fill; }

// ==================== 基本图形 ====================

void PD_DrawPixel(int16_t x, int16_t y) {
  fb_set_pixel(x, y, color_to_565(pd_color));
}

void PD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t dx = abs(x1 - x0);
  int16_t dy = -abs(y1 - y0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  int16_t e2;

  while (1) {
    PD_DrawPixel(x0, y0);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void PD_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
  uint16_t color_565 = color_to_565(color);
  for (int16_t i = 0; i < w; i++) {
    for (int16_t j = 0; j < h; j++) {
      fb_set_pixel(x + i, y + j, color_565);
    }
  }
}

void PD_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (pd_fill) {
    PD_FillRect(x, y, w, h, pd_color);
  } else {
    PD_DrawLine(x, y, x + w, y);
    PD_DrawLine(x + w, y, x + w, y + h);
    PD_DrawLine(x + w, y + h, x, y + h);
    PD_DrawLine(x, y + h, x, y);
  }
}

void PD_FillScreen(uint32_t color) {
  if (g_fb) {
    uint16_t color_565 = color_to_565(color);
    for (uint32_t i = 0; i < g_fb_width * g_fb_height; i++) {
      g_fb[i] = color_565;
    }
  }
}

void PD_DrawCircle(int16_t x0, int16_t y0, int16_t radius) {
  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;

  while (x >= y) {
    PD_DrawPixel(x0 + x, y0 + y);
    PD_DrawPixel(x0 + y, y0 + x);
    PD_DrawPixel(x0 - y, y0 + x);
    PD_DrawPixel(x0 - x, y0 + y);
    PD_DrawPixel(x0 - x, y0 - y);
    PD_DrawPixel(x0 - y, y0 - x);
    PD_DrawPixel(x0 + y, y0 - x);
    PD_DrawPixel(x0 + x, y0 - y);

    if (err <= 0) {
      y++;
      err += 2 * y + 1;
    }
    if (err > 0) {
      x--;
      err -= 2 * x + 1;
    }
  }
}

// ==================== 文字绘制 ====================

void PD_SetFont(FontSize_t font) {
  switch (font) {
  case FONT_ASCII_12:
    current_ascii_font = &ASCII_Font12;
    break;
  case FONT_ASCII_16:
    current_ascii_font = &ASCII_Font16;
    break;
  case FONT_ASCII_20:
    current_ascii_font = &ASCII_Font20;
    break;
  case FONT_ASCII_24:
    current_ascii_font = &ASCII_Font24;
    break;
  case FONT_ASCII_32:
    current_ascii_font = &ASCII_Font32;
    break;
  default:
    break;
  }
}

// 使用当前字体绘制字符
void PD_DrawChar(int16_t x, int16_t y, char ch) {
  if (current_ascii_font == NULL || *current_ascii_font == NULL)
    return;
  if (g_fb == NULL)
    return;

  uint8_t c = ch - 32;
  uint16_t sizes = (*current_ascii_font)->Bytes;
  uint8_t width = (*current_ascii_font)->Width;
  uint8_t height = (*current_ascii_font)->Height;
  const uint8_t *font_data = (*current_ascii_font)->pTable;

  // 静态缓冲区
  uint16_t *buffer = pd_char_buffer;
  uint16_t i = 0;
  uint8_t w = 0;
  uint16_t char_offset = c * sizes;

  for (uint16_t index = 0; index < sizes; index++) {
    uint8_t disChar = font_data[char_offset + index];
    for (uint8_t counter = 0; counter < 8; counter++) {
      if (disChar & 0x01) {
        buffer[i] = color_to_565(pd_color);
      } else {
        // 读取原帧缓冲区的内容，保持背景不变
        uint8_t col = i % width;
        uint8_t row = i / width;
        if (row < height && col < width) {
          buffer[i] = g_fb[(y + row) * LCD_WIDTH + (x + col)];
        } else {
          buffer[i] = 0x0000;
        }
      }
      i++;
      w++;
      if (w == width) {
        w = 0;
        break;
      }
      disChar >>= 1;
    }
  }

  // 写入帧缓冲区
  for (uint16_t idx = 0; idx < width * height; idx++) {
    uint8_t col = idx % width;
    uint8_t row = idx / width;
    if (row < height && col < width) {
      g_fb[(y + row) * LCD_WIDTH + (x + col)] = buffer[idx];
    }
  }
}

void PD_DrawString(int16_t x, int16_t y, const char *str) {
  if (current_ascii_font == NULL || *current_ascii_font == NULL)
    return;

  int16_t cursor_x = x;
  int16_t cursor_y = y;
  uint8_t space = 1;
  uint8_t width = (*current_ascii_font)->Width;
  uint8_t height = (*current_ascii_font)->Height;

  while (*str) {
    if (*str == '\n') {
      cursor_x = x;
      cursor_y += height + 1;
    } else if (*str == '\r') {
      cursor_x = x;
    } else {
      PD_DrawChar(cursor_x, cursor_y, *str);
      cursor_x += width + space;
    }
    str++;
  }
}

uint16_t PD_GetStringWidth(const char *str) {
  if (current_ascii_font == NULL || *current_ascii_font == NULL)
    return 0;
  uint16_t width = 0;
  uint8_t space = 1;
  uint8_t char_width = (*current_ascii_font)->Width;
  while (*str) {
    if (*str != '\n' && *str != '\r') {
      width += char_width + space;
    }
    str++;
  }
  return width > 0 ? width - space : 0;
}

uint16_t PD_GetCharWidth(void) {
  return (current_ascii_font && *current_ascii_font)
             ? (*current_ascii_font)->Width
             : 0;
}

uint16_t PD_GetCharHeight(void) {
  return (current_ascii_font && *current_ascii_font)
             ? (*current_ascii_font)->Height
             : 0;
}