/**
 ******************************************************************************
 * @file    3dox_activity.cpp
 * @author  Typheye
 * @brief   3Dox Activity implementation.
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

#include "include/3dox_activity.hpp"
#include "library/include/libdly.h"


extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

static constexpr int OUT_X = (LCD_WIDTH - RENDER_WIDTH) / 2;
static constexpr int OUT_Y = (LCD_HEIGHT - RENDER_HEIGHT) / 2;

static void draw_render_tile(uint16_t tile_y, uint16_t tile_h, int frame) {
  uint16_t *fb = LCD_GetFrameBuffer();
  if (!fb)
    return;

  int y0 = OUT_Y;
  int y1 = OUT_Y + RENDER_HEIGHT;
  int ty0 = tile_y;
  int ty1 = tile_y + tile_h;
  if (y0 < ty0)
    y0 = ty0;
  if (y1 > ty1)
    y1 = ty1;

  for (int sy = y0; sy < y1; ++sy) {
    int ry = sy - OUT_Y;
    uint16_t *dst = fb + (sy - tile_y) * LCD_WIDTH + OUT_X;
    for (int rx = 0; rx < RENDER_WIDTH; ++rx) {
      dst[rx] = render_get_pixel565(rx, ry);
    }
  }

  PD_Init();
  PD_SetFill(false);
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawRect(OUT_X - 1, OUT_Y - 1, RENDER_WIDTH + 2, RENDER_HEIGHT + 2);

  char info[32];
  snprintf(info, sizeof(info), "3DRT Frame:%d", frame);
  PD_DrawString(OUT_X, OUT_Y - 18, info);

  snprintf(info, sizeof(info), "Progress:%d%%", get_render_progress());
  PD_DrawString(OUT_X, OUT_Y + RENDER_HEIGHT + 8, info);
}

static void flush_render_frame(int frame) {
  for (uint16_t y = 0; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    uint16_t h = (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y;
    boardLCD.beginTileRender(y, h);
    draw_render_tile(y, h, frame);
    boardLCD.endTileRender();
  }
}

void render_3dox_activity(void) {
  uint8_t last_enter = 0;

  LOG_I("3DOX", "3D Path Tracer %dx%d", RENDER_WIDTH, RENDER_HEIGHT);
  LOG_I("3DOX", "ENTER to stop");

  render_init();
  if (!render_is_ready()) {
    LOG_E("3DOX", "Not enough dynamic memory");
    LCD_FLUSH({
      PD_Init();
      PD_FillScreen(TOS_BG);
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(TOS_RED);
      PD_DrawString(24, 94, "3D memory failed");
    });
    JPDelay(900);
    render_deinit();
    return;
  }
  flush_render_frame(0);

  while (1) {
    int done = 0;
    int last_progress_bucket = 0;
    while (!done) {
      keyManager._btnEnter.tick();
      uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED) ? 1 : 0;
      if (ce && !last_enter) {
        LOG_I("3DOX", "Stopped at frame %d", render_get_frame_count());
        render_deinit();
        return;
      }
      last_enter = ce;

      done = render_step(NULL);
      int progress = get_render_progress();
      int bucket = progress / 25;
      if (bucket > last_progress_bucket) {
        last_progress_bucket = bucket;
        flush_render_frame(render_get_frame_count() + 1);
      }
    }

    int frame = render_get_frame_count();
    flush_render_frame(frame);
    LOG_I("3DOX", "Frame %d done", frame);
  }
}

void render_3dox_activity_with_exit(void) { render_3dox_activity(); }
