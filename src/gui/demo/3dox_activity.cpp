/**
 ******************************************************************************
 * @file    3dox_activity.cpp
 * @author  Typheye
 * @brief   3Dox Activity implementation.
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

#include "include/3dox_activity.hpp"


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
  flush_render_frame(0);

  while (1) {
    int done = 0;
    int last_progress_bucket = 0;
    while (!done) {
      keyManager.btn_enter.tick();
      uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
      if (ce && !last_enter) {
        LOG_I("3DOX", "Stopped at frame %d", render_get_frame_count());
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
