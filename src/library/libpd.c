#include "include/libpd.h"
#include "include/sysfonts.h"
#include "include/syslogo.h"
#include <math.h> // 添加：sin, cos, sqrt 等（如果需要）
#include <stdio.h>
#include <stdlib.h> // 添加：abs() 函数
#include <string.h> // 添加：memset 等
#include "syslog.h"


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

static char g_header_time[6] = "";

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
    // 填充圆角矩形: 中心矩形 + 4个边条 + 4个1/4圆
    PD_SetFill(true);
    PD_SetColor(color);
    PD_DrawRect(x + r, y, w - 2 * r, h);         // 中心竖条
    PD_DrawRect(x, y + r, w, h - 2 * r);         // 中心横条

    // 四角扇形填充 (用像素近似)
    for (int16_t dy = -r; dy <= r; dy++) {
      for (int16_t dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy <= r * r) {
          PD_DrawPixel(x + r + dx, y + r + dy);           // 左上
          PD_DrawPixel(x + w - r + dx, y + r + dy);       // 右上
          PD_DrawPixel(x + r + dx, y + h - r + dy);       // 左下
          PD_DrawPixel(x + w - r + dx, y + h - r + dy);   // 右下
        }
      }
    }
  } else {
    // 描边圆角矩形
    PD_SetFill(false);
    // 四条直边
    PD_DrawLine(x + r, y, x + w - r, y);             // 上
    PD_DrawLine(x + r, y + h, x + w - r, y + h);     // 下
    PD_DrawLine(x, y + r, x, y + h - r);             // 左
    PD_DrawLine(x + w, y + r, x + w, y + h - r);     // 右

    // 四角圆弧
    int16_t px = r, py = 0;
    int16_t err = 0;
    while (px >= py) {
      // 八分圆对称描点
      PD_DrawPixel(x + r + px, y + r + py);
      PD_DrawPixel(x + r + py, y + r + px);
      PD_DrawPixel(x + r - px, y + r + py);
      PD_DrawPixel(x + r - py, y + r + px);
      PD_DrawPixel(x + r - px, y + r - py);
      PD_DrawPixel(x + r - py, y + r - px);
      PD_DrawPixel(x + r + px, y + r - py);
      PD_DrawPixel(x + r + py, y + r - px);

      // 右上角
      PD_DrawPixel(x + w - r + px, y + r + py);
      PD_DrawPixel(x + w - r + py, y + r + px);
      PD_DrawPixel(x + w - r - px, y + r + py);
      PD_DrawPixel(x + w - r - py, y + r + px);
      PD_DrawPixel(x + w - r - px, y + r - py);
      PD_DrawPixel(x + w - r - py, y + r - px);
      PD_DrawPixel(x + w - r + px, y + r - py);
      PD_DrawPixel(x + w - r + py, y + r - px);

      // 左下角
      PD_DrawPixel(x + r + px, y + h - r + py);
      PD_DrawPixel(x + r + py, y + h - r + px);
      PD_DrawPixel(x + r - px, y + h - r + py);
      PD_DrawPixel(x + r - py, y + h - r + px);
      PD_DrawPixel(x + r - px, y + h - r - py);
      PD_DrawPixel(x + r - py, y + h - r - px);
      PD_DrawPixel(x + r + px, y + h - r - py);
      PD_DrawPixel(x + r + py, y + h - r - px);

      // 右下角
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

// ==================== 多边形绘制 ====================

// 边结构体 (用于扫描线填充)
typedef struct {
  int16_t y_min;
  int16_t y_max;
  float x;     // 当前 x 交点
  float dx;    // 斜率倒数 (Δx/Δy)
} PolyEdge;

#define MAX_POLY_EDGES 16

void PD_DrawPolygon(const int16_t *points, uint16_t num_points,
                    uint32_t color) {
  if (num_points < 3 || points == NULL)
    return;

  uint16_t color_565 = color_to_565(color);

  // 找到多边形包围盒
  int16_t min_y = points[1], max_y = points[1];
  for (uint16_t i = 1; i < num_points; i++) {
    if (points[i * 2 + 1] < min_y)
      min_y = points[i * 2 + 1];
    if (points[i * 2 + 1] > max_y)
      max_y = points[i * 2 + 1];
  }
  if (min_y > max_y)
    return;

  // 为每条扫描线构建边表
  PolyEdge edges[MAX_POLY_EDGES];
  uint16_t edge_count = 0;

  for (uint16_t i = 0; i < num_points && edge_count < MAX_POLY_EDGES; i++) {
    uint16_t j = (i + 1) % num_points;
    int16_t y0 = points[i * 2 + 1];
    int16_t y1 = points[j * 2 + 1];

    if (y0 == y1)
      continue; // 水平边跳过

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

  // 扫描线填充
  for (int16_t y = min_y; y <= max_y; y++) {
    // 收集当前扫描线的所有交点
    int16_t intersections[MAX_POLY_EDGES];
    uint16_t int_count = 0;

    for (uint16_t e = 0; e < edge_count && int_count < MAX_POLY_EDGES; e++) {
      if (y >= edges[e].y_min && y < edges[e].y_max) {
        intersections[int_count++] = (int16_t)edges[e].x;
      }
    }

    // 按 x 排序 (简单冒泡)
    for (uint16_t a = 0; a < int_count; a++) {
      for (uint16_t b = a + 1; b < int_count; b++) {
        if (intersections[a] > intersections[b]) {
          int16_t tmp = intersections[a];
          intersections[a] = intersections[b];
          intersections[b] = tmp;
        }
      }
    }

    // 配对填充
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
        g_fb[y * g_fb_width + x] = color_565;
      }
    }

    // 更新每条边的 x 交点
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

// ==================== TOS 风格 GUI 元素 ====================

void PD_DrawAngledCard(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint32_t color) {
  if (radius > w / 2)
    radius = w / 2;
  if (radius > h / 2)
    radius = h / 2;
  if (radius < 0)
    radius = 0;

  // 6边形顶点: 左上→右上(缩进)→右上角→右下角→右下(缩进)→左下
  int16_t pts[12] = {
      x,                y,                  // 左上
      x + w - radius,   y,                  // 上边右
      x + w,            y + radius,         // 右上角
      x + w,            y + h,              // 右下角
      x + radius,       y + h,              // 下边左
      x,                y + h - radius,     // 左下角
  };
  PD_DrawPolygon(pts, 6, color);
}

// ==================== TOS 状态图标 ====================

void PD_DrawWifiIcon(int16_t x, int16_t y, bool connected) {
  int16_t cx = x + 8;
  int16_t cy = y + 11;
  uint32_t on = connected ? TOS_ACCENT : TOS_GREY;
  #define WIFI_ARC_STEPS 16

  // 3层弧形条 (从外到内)
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

  // 3个填充小方块
  PD_SetColor(color);
  PD_SetFill(true);
  PD_DrawRect(x + 7,  y + 1,  bw, bh);  // 顶部居中
  PD_DrawRect(x + 1,  y + 11, bw, bh);  // 左下
  PD_DrawRect(x + 13, y + 11, bw, bh);  // 右下
  PD_SetFill(false);

  // 连接线 — 顶部方块分别连到两个底部方块
  PD_DrawLine(x + 9,  y + 5,  x + 3,  y + 11);
  PD_DrawLine(x + 11, y + 5,  x + 16, y + 11);
}

// 全屏装饰边框 (斜角科技风)
void PD_DrawFrame(void) {
  LCD_UpdateAutoBrightness(); // global auto-brightness hook
  const int16_t line_w = 2;

  // 外框 (填充为主色调)
  const int16_t outer[16] = {
      0,   0,   90,  0,   110, 20,  218, 20,
      238, 40,  238, 215, 20,  215, 0,   195,
  };
  PD_DrawPolygon(outer, 8, TOS_ACCENT);

  // 内框 (挖空为背景色)
  const int16_t inner[16] = {
      line_w,       line_w,       90 - line_w,  line_w,
      110 - line_w, 20 + line_w,  218 - line_w, 20 + line_w,
      238 - line_w, 40 + line_w,  238 - line_w, 215 - line_w,
      20 + line_w,  215 - line_w, line_w,       195 - line_w,
  };
  PD_DrawPolygon(inner, 8, TOS_BG);

  // 上方菱形装饰1
  {
    const int16_t d1[8] = {83, 8, 88, 8, 98, 18, 93, 18};
    PD_DrawPolygon(d1, 4, TOS_ACCENT);
  }
  // 上方菱形装饰2
  {
    const int16_t d2[8] = {73, 8, 78, 8, 88, 18, 83, 18};
    PD_DrawPolygon(d2, 4, TOS_ACCENT);
  }
  // 右上三角装饰
  {
    const int16_t d3[6] = {223, 20, 238, 20, 238, 35};
    PD_DrawPolygon(d3, 3, TOS_ACCENT);
  }

  // 标题栏图标 (左侧小方块 + 内部折线)
  PD_SetColor(TOS_ACCENT);
  PD_SetFill(true);
  PD_DrawRect(6, 8, 13, 15);
  PD_SetFill(false);

  // 图标内部折线 (用背景色画)
  {
    const int16_t icon[10] = {8, 10, 16, 10, 16, 20, 8, 20, 8, 18};
    PD_DrawPolygonOutline(icon, 5);

    PD_SetColor(TOS_BG);
    PD_DrawLine(14, 16, 14, 14);
    PD_DrawLine(8, 12, 14, 12);
    PD_DrawLine(14, 12, 14, 10);
  }

  // Header time (set by PD_SetHeaderTime)
  if (g_header_time[0] != '\0') {
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(195, 2, g_header_time);
  }
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

  // 标题文字 (青色, 放在边框左上区域)
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

// ==================== 全局变量（文件开头添加）====================
static volatile uint8_t splash_fade_out_requested = 0;
static volatile uint8_t splash_in_progress = 0;
static uint32_t splash_total_pixels = 0;
static uint16_t *splash_logo_data = NULL;

// ==================== 颜色混合函数 ====================
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

// ==================== 淡入动画（不等待，立即返回）====================
/**
 * @brief 显示启动 Logo 淡入动画（非阻塞）
 * @param fade_in_ms 淡入时长(毫秒)
 * @note 调用后立即返回，动画在后台进行
 */
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

  // 淡入动画
  for (uint32_t s = 0; s <= steps; s++) {
    float t = (float)s / steps;
    float ratio = 0.5f - 0.5f * cosf(3.14159f * t);

    for (uint32_t i = 0; i < splash_total_pixels; i++) {
      g_fb[i] = blend_rgb565(0x0000, logo_data[i], ratio);
    }

    LCD_Flush();

    // 检查是否被中断
    if (splash_fade_out_requested) {
      LOG_W("PD", "Fade in interrupted by fade out request");
      break;
    }

    if (step_delay > 0) {
      HAL_Delay(step_delay);
    }
  }

  // 确保完全显示
  if (!splash_fade_out_requested) {
    memcpy(g_fb, logo_data, splash_total_pixels * sizeof(uint16_t));
    LCD_Flush();
  }

  LOG_I("PD", "Fade in complete, waiting for finish signal");
}

// ==================== 主动结束并淡出 ====================
/**
 * @brief 结束启动画面，执行淡出动画
 * @param fade_out_ms 淡出时长(毫秒)
 */
void PD_SplashFinish(uint32_t fade_out_ms) {
  if (!splash_in_progress) {
    LOG_W("PD", "No splash animation in progress");
    return;
  }

  LOG_I("PD", "Finish requested, starting fade out (%lums)", (unsigned long)fade_out_ms);

  splash_fade_out_requested = 1;

  uint32_t steps = 10;
  uint32_t step_delay = fade_out_ms / steps;

  // 淡出动画
  for (uint32_t s = 0; s <= steps; s++) {
    float t = (float)s / steps;
    float ratio = 0.5f - 0.5f * cosf(3.14159f * t);

    for (uint32_t i = 0; i < splash_total_pixels; i++) {
      g_fb[i] = blend_rgb565(logo_data[i], 0x0000, ratio);
    }

    LCD_Flush();

    if (step_delay > 0) {
      HAL_Delay(step_delay);
    }
  }

  // 清屏
  PD_FillScreen(LCD_COLOR_BLACK);
  LCD_Flush();

  splash_in_progress = 0;
  LOG_I("PD", "Fade out complete, splash finished");
}

// ==================== 检查是否正在显示 ====================
/**
 * @brief 检查启动画面是否正在显示
 * @return 1 正在显示，0 未显示
 */
uint8_t PD_IsSplashActive(void) { return splash_in_progress; }