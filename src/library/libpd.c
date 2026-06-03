/**
 ******************************************************************************
 * @file    libpd.c
 * @author  Typheye
 * @brief   Libpd implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#define SYSLOGO_DEFINE
#include "include/libpd.h"



#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define PD_MAX_TEXT_CHARS 160

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif


static CCMRAM uint32_t pd_color = 0xFFFFFF;
static CCMRAM uint32_t pd_bg_color = 0x000000;
static CCMRAM bool pd_fill = false;

static pFONT *current_ascii_font = NULL;
static CCMRAM uint16_t pd_char_buffer[1024]; // 2KB

static char g_header_time[6] = "";


static uint16_t *g_fb = NULL;
static uint16_t g_fb_width = 0;
static uint16_t g_fb_height = 0;
static uint16_t g_tile_y = 0;
static uint16_t g_tile_h = 0;



static inline uint16_t *fb_get_ptr(int16_t x, int16_t y) {
  if (!g_fb || x < 0 || x >= (int)g_fb_width || y < (int)g_tile_y || y >= (int)(g_tile_y + g_tile_h))
    return NULL;
  return &g_fb[(y - g_tile_y) * g_fb_width + x];
}

static inline void fb_set_pixel(int16_t x, int16_t y, uint16_t color_565) {
  uint16_t *p = fb_get_ptr(x, y);
  if (p) *p = color_565;
}

static uint16_t color_to_565(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}


void PD_Init(void) {
  current_ascii_font = &ASCII_Font16;

  
  g_fb = LCD_GetFrameBuffer();
  g_fb_width = LCD_GetWidth();
  g_fb_height = TILE_HEIGHT;
  g_tile_y = LCD_GetTileY();
  g_tile_h = LCD_GetTileH();
}

void PD_SetTileWindow(uint16_t y, uint16_t h) {
  g_tile_y = y;
  g_tile_h = h;
}


void PD_SetColor(uint32_t color) { pd_color = color; }
void PD_SetBgColor(uint32_t color) { pd_bg_color = color; }
void PD_SetFill(bool fill) { pd_fill = fill; }



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
    for (uint32_t i = 0; i < g_fb_width * g_tile_h; i++) {
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


void PD_DrawChar(int16_t x, int16_t y, char ch) {
  if (current_ascii_font == NULL || *current_ascii_font == NULL)
    return;
  if (g_fb == NULL)
    return;
  if (ch < 32 || ch > 126)
    ch = '?';

  uint8_t c = ch - 32;
  uint16_t sizes = (*current_ascii_font)->Bytes;
  uint8_t width = (*current_ascii_font)->Width;
  uint8_t height = (*current_ascii_font)->Height;
  const uint8_t *font_data = (*current_ascii_font)->pTable;

  
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
        
        uint8_t col = i % width;
        uint8_t row = i / width;
        int16_t px = x + col;
        int16_t py = y + row;
        uint16_t *bg = fb_get_ptr(px, py);
        if (row < height && col < width && bg) {
          buffer[i] = *bg;
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

  
  for (uint16_t idx = 0; idx < width * height; idx++) {
    uint8_t col = idx % width;
    uint8_t row = idx / width;
    int16_t px = x + col;
    int16_t py = y + row;
    uint16_t *dst = fb_get_ptr(px, py);
    if (row < height && col < width && dst) {
      *dst = buffer[idx];
    }
  }
}

void PD_DrawString(int16_t x, int16_t y, const char *str) {
  if (current_ascii_font == NULL || *current_ascii_font == NULL)
    return;
  if (str == NULL)
    return;

  int16_t cursor_x = x;
  int16_t cursor_y = y;
  uint8_t space = 1;
  uint8_t width = (*current_ascii_font)->Width;
  uint8_t height = (*current_ascii_font)->Height;
  uint16_t guard = 0;

  while (*str && guard++ < PD_MAX_TEXT_CHARS) {
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
  if (str == NULL)
    return 0;
  uint16_t width = 0;
  uint8_t space = 1;
  uint8_t char_width = (*current_ascii_font)->Width;
  uint16_t guard = 0;
  while (*str && guard++ < PD_MAX_TEXT_CHARS) {
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

void PD_DrawStringCentered(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char *str) {
  if (current_ascii_font == NULL || *current_ascii_font == NULL)
    return;
  uint16_t sw = PD_GetStringWidth(str);
  uint16_t sh = (*current_ascii_font)->Height;
  int16_t cx = x + (w - sw) / 2;
  int16_t cy = y + (h - sh) / 2;
  if (cx < x)
    cx = x;
  if (cy < y)
    cy = y;
  PD_DrawString(cx, cy, str);
}

void PD_DrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) {
  if (r <= 0) {
    PD_DrawRect(x, y, w, h);
    return;
  }
  if (r > w / 2)
    r = w / 2;
  if (r > h / 2)
    r = h / 2;

  uint32_t color = pd_color;
  bool fill = pd_fill;

  if (fill) {
    
    PD_SetFill(true);
    PD_SetColor(color);
    PD_DrawRect(x + r, y, w - 2 * r, h);         
    PD_DrawRect(x, y + r, w, h - 2 * r);         

    
    for (int16_t dy = -r; dy <= r; dy++) {
      for (int16_t dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy <= r * r) {
          PD_DrawPixel(x + r + dx, y + r + dy);           
          PD_DrawPixel(x + w - r + dx, y + r + dy);       
          PD_DrawPixel(x + r + dx, y + h - r + dy);       
          PD_DrawPixel(x + w - r + dx, y + h - r + dy);   
        }
      }
    }
  } else {
    
    PD_SetFill(false);
    
    PD_DrawLine(x + r, y, x + w - r, y);             
    PD_DrawLine(x + r, y + h, x + w - r, y + h);     
    PD_DrawLine(x, y + r, x, y + h - r);             
    PD_DrawLine(x + w, y + r, x + w, y + h - r);     

    
    int16_t px = r, py = 0;
    int16_t err = 0;
    while (px >= py) {
      
      PD_DrawPixel(x + r + px, y + r + py);
      PD_DrawPixel(x + r + py, y + r + px);
      PD_DrawPixel(x + r - px, y + r + py);
      PD_DrawPixel(x + r - py, y + r + px);
      PD_DrawPixel(x + r - px, y + r - py);
      PD_DrawPixel(x + r - py, y + r - px);
      PD_DrawPixel(x + r + px, y + r - py);
      PD_DrawPixel(x + r + py, y + r - px);

      
      PD_DrawPixel(x + w - r + px, y + r + py);
      PD_DrawPixel(x + w - r + py, y + r + px);
      PD_DrawPixel(x + w - r - px, y + r + py);
      PD_DrawPixel(x + w - r - py, y + r + px);
      PD_DrawPixel(x + w - r - px, y + r - py);
      PD_DrawPixel(x + w - r - py, y + r - px);
      PD_DrawPixel(x + w - r + px, y + r - py);
      PD_DrawPixel(x + w - r + py, y + r - px);

      
      PD_DrawPixel(x + r + px, y + h - r + py);
      PD_DrawPixel(x + r + py, y + h - r + px);
      PD_DrawPixel(x + r - px, y + h - r + py);
      PD_DrawPixel(x + r - py, y + h - r + px);
      PD_DrawPixel(x + r - px, y + h - r - py);
      PD_DrawPixel(x + r - py, y + h - r - px);
      PD_DrawPixel(x + r + px, y + h - r - py);
      PD_DrawPixel(x + r + py, y + h - r - px);

      
      PD_DrawPixel(x + w - r + px, y + h - r + py);
      PD_DrawPixel(x + w - r + py, y + h - r + px);
      PD_DrawPixel(x + w - r - px, y + h - r + py);
      PD_DrawPixel(x + w - r - py, y + h - r + px);
      PD_DrawPixel(x + w - r - px, y + h - r - py);
      PD_DrawPixel(x + w - r - py, y + h - r - px);
      PD_DrawPixel(x + w - r + px, y + h - r - py);
      PD_DrawPixel(x + w - r + py, y + h - r - px);

      if (err <= 0) {
        py++;
        err += 2 * py + 1;
      }
      if (err > 0) {
        px--;
        err -= 2 * px + 1;
      }
    }
  }
  pd_color = color;
  pd_fill = fill;
}

void PD_DrawCard(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                 uint32_t bg_color, uint32_t border_color, int16_t border_w) {
  if (border_w < 1)
    border_w = 1;

  PD_SetColor(bg_color);
  PD_SetFill(true);
  PD_DrawRoundRect(x, y, w, h, r);
  PD_SetFill(false);

  if (border_color != bg_color && border_w > 0) {
    PD_SetColor(border_color);
    for (int16_t i = 0; i < border_w; i++) {
      PD_DrawRoundRect(x + i, y + i, w - i * 2, h - i * 2, r - i);
    }
  }
}

void PD_DrawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                        float percent, uint32_t bar_color, uint32_t bg_color) {
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;

  PD_SetColor(bg_color);
  PD_SetFill(true);
  PD_DrawRect(x, y, w, h);
  PD_SetFill(false);

  int16_t fill_w = (int16_t)(w * percent / 100.0f);
  if (fill_w > 0) {
    PD_SetColor(bar_color);
    PD_SetFill(true);
    PD_DrawRect(x + 1, y + 1, fill_w - 2, h - 2);
    PD_SetFill(false);
  }

  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(x, y, w, h);
}




typedef struct {
  int16_t y_min;
  int16_t y_max;
  float x;     
  float dx;    
} PolyEdge;

#define MAX_POLY_EDGES 16

void PD_DrawPolygon(const int16_t *points, uint16_t num_points,
                    uint32_t color) {
  if (num_points < 3 || points == NULL || g_fb == NULL)
    return;

  uint16_t color_565 = color_to_565(color);

  
  int16_t min_y = points[1], max_y = points[1];
  for (uint16_t i = 1; i < num_points; i++) {
    if (points[i * 2 + 1] < min_y)
      min_y = points[i * 2 + 1];
    if (points[i * 2 + 1] > max_y)
      max_y = points[i * 2 + 1];
  }
  if (min_y > max_y)
    return;

  
  PolyEdge edges[MAX_POLY_EDGES];
  uint16_t edge_count = 0;

  for (uint16_t i = 0; i < num_points && edge_count < MAX_POLY_EDGES; i++) {
    uint16_t j = (i + 1) % num_points;
    int16_t y0 = points[i * 2 + 1];
    int16_t y1 = points[j * 2 + 1];

    if (y0 == y1)
      continue; 

    int16_t x0 = points[i * 2];
    int16_t x1 = points[j * 2];
    int16_t ey_min, ey_max;
    float ex, edx;

    if (y0 < y1) {
      ey_min = y0;
      ey_max = y1;
      ex = (float)x0;
      edx = (float)(x1 - x0) / (float)(y1 - y0);
    } else {
      ey_min = y1;
      ey_max = y0;
      ex = (float)x1;
      edx = (float)(x0 - x1) / (float)(y0 - y1);
    }

    edges[edge_count].y_min = ey_min;
    edges[edge_count].y_max = ey_max;
    edges[edge_count].x = ex;
    edges[edge_count].dx = edx;
    edge_count++;
  }

  
  for (int16_t y = min_y; y <= max_y; y++) {
    
    int16_t intersections[MAX_POLY_EDGES];
    uint16_t int_count = 0;

    for (uint16_t e = 0; e < edge_count && int_count < MAX_POLY_EDGES; e++) {
      if (y >= edges[e].y_min && y < edges[e].y_max) {
        intersections[int_count++] = (int16_t)edges[e].x;
      }
    }

    
    for (uint16_t a = 0; a < int_count; a++) {
      for (uint16_t b = a + 1; b < int_count; b++) {
        if (intersections[a] > intersections[b]) {
          int16_t tmp = intersections[a];
          intersections[a] = intersections[b];
          intersections[b] = tmp;
        }
      }
    }

    
    if (y >= (int)g_tile_y && y < (int)(g_tile_y + g_tile_h)) {
      uint16_t *row_start = &g_fb[(y - g_tile_y) * g_fb_width];
      for (uint16_t k = 0; k + 1 < int_count; k += 2) {
      int16_t x0 = intersections[k];
      int16_t x1 = intersections[k + 1];
      if (x0 < 0)
        x0 = 0;
      if (x1 >= g_fb_width)
        x1 = g_fb_width - 1;
      if (x0 > x1)
        continue;
      for (int16_t x = x0; x <= x1; x++) {
        row_start[x] = color_565;
      }
      }
    }

    
    for (uint16_t e = 0; e < edge_count; e++) {
      if (y >= edges[e].y_min && y < edges[e].y_max) {
        edges[e].x += edges[e].dx;
      }
    }
  }
}

void PD_DrawPolygonOutline(const int16_t *points, uint16_t num_points) {
  if (num_points < 2 || points == NULL)
    return;
  for (uint16_t i = 0; i < num_points; i++) {
    uint16_t j = (i + 1) % num_points;
    PD_DrawLine(points[i * 2], points[i * 2 + 1], points[j * 2],
                points[j * 2 + 1]);
  }
}



void PD_DrawAngledCard(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint32_t color) {
  if (radius > w / 2)
    radius = w / 2;
  if (radius > h / 2)
    radius = h / 2;
  if (radius < 0)
    radius = 0;

  
  int16_t pts[12] = {
      x,                y,                  
      x + w - radius,   y,                  
      x + w,            y + radius,         
      x + w,            y + h,              
      x + radius,       y + h,              
      x,                y + h - radius,     
  };
  PD_DrawPolygon(pts, 6, color);
}



void PD_DrawWifiIcon(int16_t x, int16_t y, bool connected) {
  int16_t cx = x + 8;
  int16_t cy = y + 11;
  uint32_t on = connected ? TOS_ACCENT : TOS_GREY;
  #define WIFI_ARC_STEPS 16

  
  struct { int16_t ro; int16_t ri; } bars[3] = {
    {8, 5}, {4, 2}, {1, 0}
  };

  for (int b = 0; b < 3; b++) {
    int16_t pts[(WIFI_ARC_STEPS + 1) * 4];
    int n = 0;
    int16_t ro = bars[b].ro, ri = bars[b].ri;
    for (int i = 0; i <= WIFI_ARC_STEPS; i++) {
      float a = (210.0f + 120.0f * i / WIFI_ARC_STEPS) * 3.14159f / 180.0f;
      pts[n++] = cx + (int16_t)(ro * cosf(a));
      pts[n++] = cy - (int16_t)(ro * sinf(a));
    }
    for (int i = WIFI_ARC_STEPS; i >= 0; i--) {
      float a = (210.0f + 120.0f * i / WIFI_ARC_STEPS) * 3.14159f / 180.0f;
      pts[n++] = cx + (int16_t)(ri * cosf(a));
      pts[n++] = cy - (int16_t)(ri * sinf(a));
    }
    PD_DrawPolygon(pts, n / 2, on);
  }
}

void PD_DrawEthIcon(int16_t x, int16_t y, bool connected) {
  uint32_t color = connected ? TOS_ACCENT : TOS_GREY;
  int16_t bw = 5, bh = 4;

  
  PD_SetColor(color);
  PD_SetFill(true);
  PD_DrawRect(x + 7,  y + 1,  bw, bh);  
  PD_DrawRect(x + 1,  y + 11, bw, bh);  
  PD_DrawRect(x + 13, y + 11, bw, bh);  
  PD_SetFill(false);

  
  PD_DrawLine(x + 9,  y + 5,  x + 3,  y + 11);
  PD_DrawLine(x + 11, y + 5,  x + 16, y + 11);
}

void PD_DrawSignalIcon(int16_t x, int16_t y, int signal) {
  /* 2x scale of Python (10x8 → 20x16) */
  uint32_t c = signal > 0 ? TOS_ACCENT : TOS_CARD_BG;
  int16_t h = 16;

  /* Antenna triangle (left portion) */
  int16_t tx_w = 8;
  int16_t tx_h = h;
  int16_t tri[14] = {
    x,           y,
    x + tx_w,    y,
    x + tx_w/2,  y + tx_h/2,
    x,           y,
    x + tx_w/2,  y,
    x + tx_w/2,  y + tx_h,
    x + tx_w/2,  y,
  };
  PD_DrawPolygon(tri, 7, c);

  /* 5 signal bars (right side) — 2px wide, 1px gap */
  int bars_on = signal > 0 ? ((signal - 1) / 20 + 1) : 0;
  if (bars_on > 5) bars_on = 5;
  PD_SetFill(true);
  for (int f = 0; f < 5; f++) {
    int bh = 3 + f * 3;  /* 3,6,9,12,15 */
    int bx = x + tx_w + 2 + f * 3;
    int by = y + h - bh;
    PD_SetColor(f < bars_on ? c : TOS_CARD_BG);
    PD_FillRect(bx, by, 2, bh, f < bars_on ? c : TOS_CARD_BG);
  }
  PD_SetFill(false);
}


void PD_DrawFrame(void) {
  LCD_UpdateAutoBrightness(); // global auto-brightness hook
  const int16_t line_w = 2;

  
  const int16_t outer[16] = {
      0,   0,   90,  0,   110, 20,  218, 20,
      238, 40,  238, 215, 20,  215, 0,   195,
  };
  PD_DrawPolygon(outer, 8, TOS_ACCENT);

  
  const int16_t inner[16] = {
      line_w,       line_w,       90 - line_w,  line_w,
      110 - line_w, 20 + line_w,  218 - line_w, 20 + line_w,
      238 - line_w, 40 + line_w,  238 - line_w, 215 - line_w,
      20 + line_w,  215 - line_w, line_w,       195 - line_w,
  };
  PD_DrawPolygon(inner, 8, TOS_BG);

  
  {
    const int16_t d1[8] = {83, 8, 88, 8, 98, 18, 93, 18};
    PD_DrawPolygon(d1, 4, TOS_ACCENT);
  }
  
  {
    const int16_t d2[8] = {73, 8, 78, 8, 88, 18, 83, 18};
    PD_DrawPolygon(d2, 4, TOS_ACCENT);
  }
  
  {
    const int16_t d3[6] = {223, 20, 238, 20, 238, 35};
    PD_DrawPolygon(d3, 3, TOS_ACCENT);
  }

  
  extern void draw_icon_ico(void);
  draw_icon_ico();

  // Header time (set by PD_SetHeaderTime)
  if (g_header_time[0] != '\0') {
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(195, 2, g_header_time);
  }

  /* Status icons (WiFi signal, WLAN, hotspot) */
  extern void status_icons_draw(bool wlan_on, bool wlan_connected, bool hotspot_on);
  extern bool esp_wlan_is_on(void);
  extern bool esp_wlan_is_connected(void);
  extern bool hotspot_is_active(void);
  status_icons_draw(esp_wlan_is_on(), esp_wlan_is_connected(), hotspot_is_active());
}

void PD_SetHeaderTime(const char *time_str) {
  if (time_str) {
    strncpy(g_header_time, time_str, 5);
    g_header_time[5] = '\0';
  } else {
    g_header_time[0] = '\0';
  }
}

void PD_DrawHeader(const char *title) {
  PD_DrawFrame();

  
  if (title) {
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, title);
  }
}

void PD_DrawHeaderWithTime(const char *title, const char *time_str) {
  PD_DrawFrame();

  if (title) {
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, title);
  }

  if (time_str) {
    PD_SetFont(FONT_ASCII_24);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(193, 1, time_str);
  }
}

void PD_DrawFooter(const char *left_text, const char *right_text) {
  PD_DrawFooterCenter(left_text, NULL, right_text);
}

void PD_DrawFooterCenter(const char *left_text, const char *center_text,
                         const char *right_text) {
  int16_t footer_y = 219;

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(TOS_TEXT);

  if (left_text) {
    PD_DrawString(4, footer_y, left_text);
  }
  if (right_text) {
    uint16_t rw = PD_GetStringWidth(right_text);
    PD_DrawString(236 - rw, footer_y, right_text);
  }
  if (center_text) {
    uint16_t cw = PD_GetStringWidth(center_text);
    PD_DrawString(120 - cw / 2, footer_y, center_text);
  }
}


static volatile uint8_t splash_fade_out_requested = 0;
static volatile uint8_t splash_in_progress = 0;
static uint32_t splash_total_pixels = 0;
static uint16_t *splash_logo_data = NULL;


static uint16_t blend_rgb565(uint16_t color1, uint16_t color2, float ratio) {
  uint8_t r1 = (color1 >> 11) & 0x1F;
  uint8_t g1 = (color1 >> 5) & 0x3F;
  uint8_t b1 = color1 & 0x1F;

  uint8_t r2 = (color2 >> 11) & 0x1F;
  uint8_t g2 = (color2 >> 5) & 0x3F;
  uint8_t b2 = color2 & 0x1F;

  uint8_t r = (uint8_t)(r1 * (1.0f - ratio) + r2 * ratio);
  uint8_t g = (uint8_t)(g1 * (1.0f - ratio) + g2 * ratio);
  uint8_t b = (uint8_t)(b1 * (1.0f - ratio) + b2 * ratio);

  return (r << 11) | (g << 5) | b;
}



void PD_ShowSplashFadeStart(uint32_t fade_in_ms) {
  PD_Init();

  if (g_fb == NULL) {
    LOG_E("PD", "Fade start failed: framebuffer is NULL");
    return;
  }

  if (splash_in_progress) {
    LOG_W("PD", "Fade already in progress");
    return;
  }

  splash_fade_out_requested = 0;
  splash_in_progress = 1;
  splash_total_pixels = LOGO_WIDTH * LOGO_HEIGHT;
  splash_logo_data = (uint16_t *)logo_data;

  LOG_I("PD", "Starting fade in animation (%lums)", (unsigned long)fade_in_ms);

  uint32_t steps = 30;
  uint32_t step_delay = fade_in_ms / steps;

  
  for (uint32_t s = 0; s <= steps; s++) {
    float t = (float)s / steps;
    float ratio = 0.5f - 0.5f * cosf(3.14159f * t);

    for (uint16_t ty = 0; ty < LOGO_HEIGHT; ty += TILE_HEIGHT) {
      uint16_t h = (ty + TILE_HEIGHT <= LOGO_HEIGHT) ? TILE_HEIGHT : LOGO_HEIGHT - ty;
      LCD_BeginTileRender(ty, h);
      g_fb = LCD_GetFrameBuffer();
      g_tile_y = ty;
      g_tile_h = h;

      for (uint32_t i = 0; i < (uint32_t)LOGO_WIDTH * h; i++) {
        uint32_t src_idx = (uint32_t)ty * LOGO_WIDTH + i;
        g_fb[i] = blend_rgb565(0x0000, logo_data[src_idx], ratio);
      }

      LCD_EndTileRender();
    }

    
    if (splash_fade_out_requested) {
      LOG_W("PD", "Fade in interrupted by fade out request");
      break;
    }

    if (step_delay > 0) {
      HAL_Delay(step_delay);
    }
  }

  
  if (!splash_fade_out_requested) {
    for (uint16_t ty = 0; ty < LOGO_HEIGHT; ty += TILE_HEIGHT) {
      uint16_t h = (ty + TILE_HEIGHT <= LOGO_HEIGHT) ? TILE_HEIGHT : LOGO_HEIGHT - ty;
      LCD_BeginTileRender(ty, h);
      g_fb = LCD_GetFrameBuffer();
      g_tile_y = ty;
      g_tile_h = h;
      memcpy(g_fb, &logo_data[ty * LOGO_WIDTH], LOGO_WIDTH * h * sizeof(uint16_t));
      LCD_EndTileRender();
    }
  }

  LOG_I("PD", "Fade in complete, waiting for finish signal");
}



void PD_SplashFinish(uint32_t fade_out_ms) {
  if (!splash_in_progress) {
    LOG_W("PD", "No splash animation in progress");
    return;
  }

  LOG_I("PD", "Finish requested, starting fade out (%lums)", (unsigned long)fade_out_ms);

  splash_fade_out_requested = 1;

  uint32_t steps = 10;
  uint32_t step_delay = fade_out_ms / steps;

  
  for (uint32_t s = 0; s <= steps; s++) {
    float t = (float)s / steps;
    float ratio = 0.5f - 0.5f * cosf(3.14159f * t);

    for (uint16_t ty = 0; ty < LOGO_HEIGHT; ty += TILE_HEIGHT) {
      uint16_t h = (ty + TILE_HEIGHT <= LOGO_HEIGHT) ? TILE_HEIGHT : LOGO_HEIGHT - ty;
      LCD_BeginTileRender(ty, h);
      g_fb = LCD_GetFrameBuffer();
      g_tile_y = ty;
      g_tile_h = h;

      for (uint32_t i = 0; i < (uint32_t)LOGO_WIDTH * h; i++) {
        uint32_t src_idx = (uint32_t)ty * LOGO_WIDTH + i;
        g_fb[i] = blend_rgb565(logo_data[src_idx], 0x0000, ratio);
      }

      LCD_EndTileRender();
    }

    if (step_delay > 0) {
      HAL_Delay(step_delay);
    }
  }

  
  for (uint16_t ty = 0; ty < LCD_HEIGHT; ty += TILE_HEIGHT) {
    uint16_t h = (ty + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - ty;
    LCD_BeginTileRender(ty, h);
    g_fb = LCD_GetFrameBuffer();
    g_tile_y = ty;
    g_tile_h = h;
    uint16_t black = color_to_565(LCD_COLOR_BLACK);
    for (uint32_t i = 0; i < (uint32_t)LCD_WIDTH * h; i++) {
      g_fb[i] = black;
    }
    LCD_EndTileRender();
  }

  splash_in_progress = 0;
  LOG_I("PD", "Fade out complete, splash finished");
}



uint8_t PD_IsSplashActive(void) { return splash_in_progress; }
