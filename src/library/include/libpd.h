/**
 ******************************************************************************
 * @file    libpd.h
 * @author  Typheye
 * @brief   Libpd interface.
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

#ifndef __LIBPD_H
#define __LIBPD_H

#include "include/lcd.h"
#include "sysfonts.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== TOS 风格颜色调色板 ====================
#define TOS_BG         0x000000  // 主背景纯黑
#define TOS_CARD_BG    0x111111  // 卡片背景深灰
#define TOS_ACCENT     0x1f4648  // 主色调暗青/墨绿 (边框、标题)
#define TOS_ACCENT2    0x0D1F20  // 次色调 (比 accent 更暗)
#define TOS_TEXT       0xFFFFFF  // 主文字白
#define TOS_TEXT_SEC   0xCDCDCD  // 次文字灰白
#define TOS_GREY       0x555555  // 灰色
#define TOS_GREEN      0x00FF00  // 成功绿
#define TOS_RED        0xF00000  // 错误红
#define TOS_YELLOW     0xFFFF00  // 警告黄

// 保留旧的 LV_ 宏以兼容现有代码
#define LV_BG_DARK     TOS_BG
#define LV_BG_CARD     TOS_CARD_BG
#define LV_BG_CARD2    0x1A1A1A
#define LV_BORDER      0x1f4648
#define LV_PRIMARY     TOS_ACCENT
#define LV_PRIMARY_DARK 0x0D1F20
#define LV_ACCENT      TOS_ACCENT
#define LV_SUCCESS     TOS_GREEN
#define LV_WARNING     TOS_YELLOW
#define LV_ERROR       TOS_RED
#define LV_TEXT_PRIMARY TOS_TEXT
#define LV_TEXT_SECONDARY TOS_TEXT_SEC
#define LV_TEXT_HINT   0x6A6A6A

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
void PD_SetTileWindow(uint16_t y, uint16_t h);

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
void PD_DrawStringCentered(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char *str);
uint16_t PD_GetStringWidth(const char *str);
uint16_t PD_GetCharWidth(void);
uint16_t PD_GetCharHeight(void);

// 高级图形
void PD_DrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r);
void PD_DrawCard(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                 uint32_t bg_color, uint32_t border_color, int16_t border_w);
void PD_DrawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                        float percent, uint32_t bar_color, uint32_t bg_color);

// ========== 新增：多边形绘图 ==========
// 填充多边形 (x0,y0, x1,y1, ...) 每组2个int16_t, N个顶点
void PD_DrawPolygon(const int16_t *points, uint16_t num_points,
                    uint32_t color);
// 多边形描边
void PD_DrawPolygonOutline(const int16_t *points, uint16_t num_points);

// ========== 新增：TOS 风格 GUI 元素 ==========
// 全屏装饰边框 (斜角科技风)
void PD_DrawFrame(void);
void PD_SetHeaderTime(const char *time_str);
// 页面头部 (标题 + 时间 + 装饰)
void PD_DrawHeader(const char *title);
void PD_DrawHeaderWithTime(const char *title, const char *time_str);
// 页面底部操作栏
void PD_DrawFooter(const char *left_text, const char *right_text);
void PD_DrawFooterCenter(const char *left_text, const char *center_text,
                         const char *right_text);
// 斜角卡片 (6边形近似圆角矩形, 参考项目风格)
void PD_DrawAngledCard(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint32_t color);

// ========== 新增：状态图标 ==========
void PD_DrawWifiIcon(int16_t x, int16_t y, bool connected);
void PD_DrawEthIcon(int16_t x, int16_t y, bool connected);
void PD_DrawSignalIcon(int16_t x, int16_t y, int signal);

// ========== 新增：可控淡入淡出 ==========
void PD_ShowSplashFadeStart(uint32_t fade_in_ms);
void PD_SplashFinish(uint32_t fade_out_ms);
uint8_t PD_IsSplashActive(void);

#ifdef __cplusplus
}
#endif

#endif
