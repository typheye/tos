#ifndef __LCD_CONFIG_H
#define __LCD_CONFIG_H

#include "stm32f4xx_hal.h"

// ==================== LCD 尺寸 ====================
#define LCD_WIDTH 240
#define LCD_HEIGHT 240
#define TILE_HEIGHT 32

// ==================== 引脚定义 ====================
// SPI 引脚
#define LCD_SCK_PIN GPIO_PIN_3
#define LCD_SCK_PORT GPIOB
#define LCD_SDA_PIN GPIO_PIN_5
#define LCD_SDA_PORT GPIOB

// 控制引脚
#define LCD_CS_PIN GPIO_PIN_11
#define LCD_CS_PORT GPIOD
#define LCD_DC_PIN GPIO_PIN_12
#define LCD_DC_PORT GPIOD
#define LCD_BL_PIN GPIO_PIN_13
#define LCD_BL_PORT GPIOD
#define LCD_RST_PIN GPIO_PIN_14
#define LCD_RST_PORT GPIOD

// ==================== 控制宏 ====================
#define LCD_CS_L HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_H HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_DC_CMD HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_DATA HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)
#define LCD_BL_ON HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET)
#define LCD_BL_OFF HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_RESET)
#define LCD_RST_L HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define LCD_RST_H HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET)

// ==================== 常用颜色定义 ====================
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

// ==================== C 接口函数声明 ====================
#ifdef __cplusplus
extern "C" {
#endif

// 基础显示函数
void LCD_Init(void);
void LCD_FillScreen(uint32_t color);
void LCD_DrawPixel(int16_t x, int16_t y, uint32_t color);
void LCD_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void LCD_Flush(void);

// 帧缓冲区操作
uint16_t *LCD_GetFrameBuffer(void);
void LCD_ClearFrameBuffer(uint32_t color);

// 分块渲染接口
void LCD_BeginTileRender(uint16_t y, uint16_t h);
void LCD_EndTileRender(void);
void LCD_FlushTiled(void (*render_cb)(void));
/* Emergency/fatal-path drawing: blocking transfer, no DMA, no full LCD re-init. */
void LCD_EmergencyPrepare(void);
void LCD_FlushTiledBlocking(void (*render_cb)(void));
void LCD_FlushFull(const uint16_t *data);
uint16_t LCD_GetTileY(void);
uint16_t LCD_GetTileH(void);

// 工具函数
void LCD_UpdateAutoBrightness(void);
uint16_t LCD_RGB888ToRGB565(uint32_t rgb888);
uint16_t LCD_GetWidth(void);
uint16_t LCD_GetHeight(void);

#ifdef __cplusplus
}
#endif

#endif
