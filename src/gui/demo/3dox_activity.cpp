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

void render_3dox_activity(void) {
  int total_pixels = LCD_WIDTH * LCD_HEIGHT;
  int current_frame = 0;
  const int TILE_PIXELS = LCD_WIDTH * TILE_HEIGHT;
  uint8_t last_enter = 0;

  LOG_I("3DOX", "3D Path Tracer (tiled+DMA)");
  LOG_I("3DOX", "ENTER to stop");

  render_init();

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
        return;
      }
      last_enter = ce;

      render_step(on_pixel);
      rendered++;

      // Tile 完成 → flush
      if ((rendered % TILE_PIXELS) == 0 && rendered < total_pixels) {
        boardLCD.endTileRender();

        cur_tile += TILE_HEIGHT;
        boardLCD.beginTileRender(cur_tile, TILE_HEIGHT);
        PD_SetTileWindow(cur_tile, TILE_HEIGHT);
      }
    }

    // 最后一个 tile
    boardLCD.endTileRender();

    LOG_I("3DOX", "Frame %d done", current_frame);
  }
}

void render_3dox_activity_with_exit(void) { render_3dox_activity(); }
