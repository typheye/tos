#include "demo/include/3dox_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/lib3dox.h"
#include "include/libpd.h"
#include <stdio.h>
#include "syslog.h"

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

static int render_frame = 0;

// 像素回调 - 写入 tile buffer（tile-relative）
static void on_pixel(int x, int y, uint32_t color) {
  uint16_t *fb = LCD_GetFrameBuffer();
  if (fb && x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    fb[(y & (TILE_HEIGHT - 1)) * LCD_WIDTH + x] = rgb565;
  }
}

// UI 叠加层区域
#define UI_Y0 210
#define UI_H  25

// 在 3D 渲染好的 tile buffer 上叠加 UI（仅当 tile 与 UI 区域重叠时）
static void overlay_on_tile(uint16_t tile_y, int progress, int frame) {
  int tile_end = tile_y + TILE_HEIGHT;
  if (tile_end <= UI_Y0 || tile_y >= UI_Y0 + UI_H) return;  // 不重叠

  PD_Init();  // 读取当前 tile window、获取 g_fb
  PD_SetFill(true);

  // 深蓝背景（只画与当前 tile 相交的部分）
  int ry = (UI_Y0 > tile_y) ? UI_Y0 : tile_y;
  int rh = ((UI_Y0 + UI_H) < tile_end ? (UI_Y0 + UI_H) : tile_end) - ry;
  if (rh > 0) {
    PD_SetColor(LCD_COLOR_DARK_BLUE);
    PD_DrawRect(20, ry, 200, rh);
  }
  PD_SetFill(false);

  // 白边框（只画与当前 tile 相交的线段）
  PD_SetColor(LCD_COLOR_WHITE);
  auto hline = [&](int y) {
    if (y >= tile_y && y < tile_end)
      PD_DrawRect(20, y, 200, 1);
  };
  hline(UI_Y0);
  hline(UI_Y0 + UI_H - 1);

  // 文字
  int txt_y = UI_Y0 + 5;
  if (txt_y >= tile_y && txt_y < tile_end) {
    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(LCD_COLOR_WHITE);
    char dbg[32];
    sprintf(dbg, "Frame: %d", frame);
    PD_DrawString(25, txt_y, dbg);

    if (progress < 100) {
      PD_SetColor(LCD_COLOR_GREEN);
      sprintf(dbg, "Process: %d%%", progress);
      PD_DrawString(125, txt_y, dbg);
    } else {
      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawString(125, txt_y, "Process: OK");
    }
  }
}

void render_3dox_activity(void) {
  int total_pixels = LCD_WIDTH * LCD_HEIGHT;
  int current_frame = 0;
  const int TILE_PIXELS = LCD_WIDTH * TILE_HEIGHT;
  uint8_t last_enter = 0;

  LOG_I("3DOX", "3D Path Tracer (tiled+DMA)");
  LOG_I("3DOX", "ENTER to stop");

  render_init();
  render_frame = 0;

  while (1) {
    current_frame++;
    int rendered = 0;
    uint16_t cur_tile = 0;

    boardLCD.beginTileRender(0, TILE_HEIGHT);
    PD_SetTileWindow(0, TILE_HEIGHT);

    while (rendered < total_pixels) {
      // ---- 按键检测（每次循环都查，不依赖 delay）----
      keyManager.btn_enter.tick();
      uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
      if (ce && !last_enter) {
        LOG_I("3DOX", "Stopped at frame %d", current_frame);
        goto exit_render;
      }
      last_enter = ce;

      render_step(on_pixel);
      rendered++;

      // Tile 完成 → 叠加 UI 后 flush
      if ((rendered % TILE_PIXELS) == 0 && rendered < total_pixels) {
        int progress = (rendered * 100) / total_pixels;
        overlay_on_tile(cur_tile, progress, current_frame);
        boardLCD.endTileRender();

        cur_tile += TILE_HEIGHT;
        boardLCD.beginTileRender(cur_tile, TILE_HEIGHT);
        PD_SetTileWindow(cur_tile, TILE_HEIGHT);
      }

    }

    // 最后一个 tile
    overlay_on_tile(cur_tile, 100, current_frame);
    boardLCD.endTileRender();

    render_frame = current_frame;
    LOG_I("3DOX", "Frame %d done", render_frame);
  }

exit_render:
  // 停止画面：直接用 fillRect 覆盖底部区域
  boardLCD.fillRect(0, 0, 240, 25, LCD_COLOR_DARK_BLUE);
  {
    boardLCD.beginTileRender(0, TILE_HEIGHT);
    PD_SetTileWindow(0, TILE_HEIGHT);
    uint16_t *fb = LCD_GetFrameBuffer();
    uint16_t bg = LCD_RGB888ToRGB565(LCD_COLOR_DARK_BLUE);
    for (int i = 0; i < LCD_WIDTH * TILE_HEIGHT; i++) fb[i] = bg;
    PD_Init();
    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(LCD_COLOR_WHITE);
    PD_DrawString(10, 6, "Render Stopped");
    PD_SetColor(LCD_COLOR_CYAN);
    char dbg[32];
    sprintf(dbg, "Frames: %d", render_frame);
    PD_DrawString(120, 6, dbg);
    boardLCD.endTileRender();
  }
  HAL_Delay(2000);
  LOG_I("3DOX", "Total frames: %d", render_frame);
}

void render_3dox_activity_with_exit(void) { render_3dox_activity(); }
