/**
 ******************************************************************************
 * @file    tcs3472_activity.cpp
 * @author  Typheye
 * @brief   Tcs3472 Activity implementation.
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

#include "include/tcs3472_activity.hpp"
#include "dram.h"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern TCS3472 boardTCS3472;

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

/* Chart state */
#define CHART_HISTORY 240
static uint16_t *chart_r = nullptr;
static uint16_t *chart_g = nullptr;
static uint16_t *chart_b = nullptr;
static CCMRAM int chart_index = 0;
static CCMRAM uint16_t chart_max_value = 65535;

static bool chart_alloc(void) {
  if (chart_r && chart_g && chart_b)
    return true;
  chart_r = (uint16_t *)SysDram_AllocFast(sizeof(uint16_t) * CHART_HISTORY);
  chart_g = (uint16_t *)SysDram_AllocFast(sizeof(uint16_t) * CHART_HISTORY);
  chart_b = (uint16_t *)SysDram_AllocFast(sizeof(uint16_t) * CHART_HISTORY);
  if (chart_r && chart_g && chart_b)
    return true;
  SysDram_Free(chart_r);
  SysDram_Free(chart_g);
  SysDram_Free(chart_b);
  chart_r = nullptr;
  chart_g = nullptr;
  chart_b = nullptr;
  return false;
}

static void chart_free(void) {
  SysDram_Free(chart_r);
  SysDram_Free(chart_g);
  SysDram_Free(chart_b);
  chart_r = nullptr;
  chart_g = nullptr;
  chart_b = nullptr;
}

/* Standard template functions (exact copy from about-page) */
/* Chart helper functions */
static void reset_chart(void) {
  if (!chart_r || !chart_g || !chart_b)
    return;
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_r[i] = 0;
    chart_g[i] = 0;
    chart_b[i] = 0;
  }
  chart_index = 0;
  chart_max_value = 65535;
}

static void update_chart_data(uint16_t r, uint16_t g, uint16_t b) {
  if (!chart_r || !chart_g || !chart_b)
    return;
  chart_r[chart_index] = r;
  chart_g[chart_index] = g;
  chart_b[chart_index] = b;
  chart_index++;
  if (chart_index >= CHART_HISTORY)
    chart_index = 0;
  static uint16_t max_r = 0, max_g = 0, max_b = 0;
  if (r > max_r)
    max_r = r;
  if (g > max_g)
    max_g = g;
  if (b > max_b)
    max_b = b;
  uint16_t new_max = (max_r > max_g) ? max_r : max_g;
  new_max = (new_max > max_b) ? new_max : max_b;
  if (new_max > chart_max_value)
    chart_max_value = new_max;
  else if (chart_max_value > 500 && new_max < chart_max_value / 2)
    chart_max_value = chart_max_value * 3 / 4;
  if (chart_max_value < 1000)
    chart_max_value = 1000;
}

static void draw_chart_axes(int x, int y, int w, int h, uint16_t max_val) {
  PD_SetColor(LV_PRIMARY);
  PD_DrawRect(x, y, w, h);
  for (int i = 1; i <= 3; i++) {
    int ly = y + (h * i / 4);
    PD_SetColor(LV_BG_DARK);
    PD_DrawLine(x, ly, x + w, ly);
  }
}

static void draw_chart_line(uint16_t *data, int count, int x, int y, int w,
                            int h, uint16_t max_val, uint32_t color) {
  if (count < 2)
    return;
  PD_SetColor(color);
  int start_y = y + h;
  for (int i = 1; i < w && i < count; i++) {
    int idx_prev = (chart_index - 1 - i + count) % count;
    int idx_curr = (chart_index - i + count) % count;
    int x1 = x + w - i;
    int y1 = start_y - (int)((float)data[idx_prev] * h / max_val);
    int x2 = x + w - (i - 1);
    int y2 = start_y - (int)((float)data[idx_curr] * h / max_val);
    if (y1 >= y && y1 <= y + h && y2 >= y && y2 <= y + h)
      PD_DrawLine(x1, y1, x2, y2);
  }
}

static void draw_chart_all(int x, int y, int w, int h, uint16_t max_val) {
  draw_chart_line(chart_r, CHART_HISTORY, x, y, w, h, max_val, 0xFF0000);
  draw_chart_line(chart_g, CHART_HISTORY, x, y, w, h, max_val, 0x00FF00);
  draw_chart_line(chart_b, CHART_HISTORY, x, y, w, h, max_val, 0x0000FF);
}

/* 01 Read Raw sub-page (scrollable menu) */
static void tcs3472_read_subpage(void) {
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 7;
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 7) % 7;
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      TCS3472_RawData_t raw = boardTCS3472.readRaw();
      TCS3472_ColorData_t color = boardTCS3472.readColor();

      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");
        PD_SetFont(FONT_ASCII_16);

        char buf[32];
        char fstr[16];
        int cy;

        /* Item 0: 00 Return */
        cy = 33;
        UI_DrawMenuCard(0, sel, cy, "00 Return");

        /* Item 1: 01 R */
        cy += 25;
        snprintf(buf, sizeof(buf), "%4u", raw.red);
        UI_DrawMenuValue(1, sel, cy, "01 R", buf, false);

        /* Item 2: 02 G */
        cy += 25;
        snprintf(buf, sizeof(buf), "%4u", raw.green);
        UI_DrawMenuValue(2, sel, cy, "   G", buf, false);

        /* Item 3: 03 B */
        cy += 25;
        snprintf(buf, sizeof(buf), "%4u", raw.blue);
        UI_DrawMenuValue(3, sel, cy, "   B", buf, false);

        /* Item 4: 04 C */
        cy += 25;
        snprintf(buf, sizeof(buf), "%4u", raw.clear);
        UI_DrawMenuValue(4, sel, cy, "   C", buf, false);

        /* Item 5: 05 CCT */
        cy += 25;
        float_to_str(color.color_temp, fstr);
        snprintf(buf, sizeof(buf), "%s K", fstr);
        UI_DrawMenuValue(5, sel, cy, "02 CCT", buf, false);

        /* Item 6: 06 Lux */
        cy += 25;
        float_to_str(color.lux, fstr);
        snprintf(buf, sizeof(buf), "%s lx", fstr);
        UI_DrawMenuValue(6, sel, cy, "03 Lux", buf, false);

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

/* 02 Color Demo sub-page */
static void tcs3472_color_subpage(void) {
  boardTCS3472.ledOn();
  uint32_t lu = 0;

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      boardTCS3472.ledOff();
      return;
    }

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      TCS3472_RawData_t raw = boardTCS3472.readRaw();

      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");

        /* Left-aligned text values at (16, 33) alert.cpp style */
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_TEXT);
        char buf[64];
        snprintf(buf, sizeof(buf), "R:%-5u G:%-5u B:%-5u\nC:%u", raw.red,
                 raw.green, raw.blue, raw.clear);
        PD_DrawString(16, 33, buf);

        /* Color block inside a card much smaller, about 80px tall */
        PD_DrawAngledCard(20, 80, 200, 80, 6, TOS_CARD_BG);
        uint32_t display_color =
            ((raw.red >> 8) << 16) | ((raw.green >> 8) << 8) | (raw.blue >> 8);
        PD_SetColor(display_color);
        PD_SetFill(true);
        PD_DrawRect(22, 82, 196, 76);
        PD_SetFill(false);

        PD_DrawFooterCenter("ENTER", NULL, NULL);
      });

      LOG_D("TACT", "RGB: %u,%u,%u", raw.red, raw.green, raw.blue);
    }
    JPDelay(1);
  }
}

/* 03 CCT & Lux sub-page */
static void tcs3472_cct_subpage(void) {
  /* Sampling phase */
  LCD_FLUSH({
    UI_DrawFrameTitle("DEMO");
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(16, 33, "Sampling...");
  });

  const uint32_t samples = 10;
  float cct_sum = 0, lux_sum = 0;
  uint16_t r_sum = 0, g_sum = 0, b_sum = 0;

  for (uint32_t i = 0; i < samples; i++) {
    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();
    r_sum += raw.red;
    g_sum += raw.green;
    b_sum += raw.blue;
    cct_sum += color.color_temp;
    lux_sum += color.lux;
    JPDelay(50);
  }

  r_sum /= samples;
  g_sum /= samples;
  b_sum /= samples;
  cct_sum /= samples;
  lux_sum /= samples;

  /* Classification */
  const char *class_str;
  if (cct_sum < 3000)
    class_str = "Warm White";
  else if (cct_sum < 4500)
    class_str = "Neutral White";
  else if (cct_sum < 5500)
    class_str = "Daylight";
  else if (cct_sum < 7000)
    class_str = "Cool White";
  else
    class_str = "Overcast/Cool";

  /* Results display loop */
  uint32_t lu = 0;
  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      return;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_TEXT);

        char fstr[16];
        char line[64];
        int msg_y = 33;

        snprintf(line, sizeof(line), "Avg R: %4u", (unsigned int)r_sum);
        PD_DrawString(16, msg_y, line);
        msg_y += 20;

        snprintf(line, sizeof(line), "Avg G: %4u", (unsigned int)g_sum);
        PD_DrawString(16, msg_y, line);
        msg_y += 20;

        snprintf(line, sizeof(line), "Avg B: %4u", (unsigned int)b_sum);
        PD_DrawString(16, msg_y, line);
        msg_y += 20;

        float_to_str(cct_sum, fstr);
        snprintf(line, sizeof(line), "CCT: %s K", fstr);
        PD_DrawString(16, msg_y, line);
        msg_y += 20;

        snprintf(line, sizeof(line), "Lux: %.1f lx", (double)lux_sum);
        PD_DrawString(16, msg_y, line);
        msg_y += 20;

        snprintf(line, sizeof(line), "CF: %s", class_str);
        PD_DrawString(16, msg_y, line);

        PD_DrawFooterCenter("ENTER", NULL, NULL);
      });
    }
    JPDelay(1);
  }
}

/* 04 Chart sub-page */
static void tcs3472_chart_subpage(void) {
  if (!chart_alloc()) {
    alert_show("ALERT", "Chart memory failed");
    return;
  }
  reset_chart();
  bool led_on = false;
  boardTCS3472.ledOff();

  uint32_t lu = 0;
  uint8_t last_a8 = 0, last_d0 = 0;
  uint32_t last_led_toggle = 0;

  while (1) {
    keyManager.btn_enter.tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      boardTCS3472.ledOff();
      chart_free();
      return;
    }

    /* UP (A8): toggle LED  (300ms debounce) */
    uint8_t ca8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (ca8 && !last_a8 && (HAL_GetTick() - last_led_toggle > 300)) {
      led_on = !led_on;
      if (led_on)
        boardTCS3472.ledOn();
      else
        boardTCS3472.ledOff();
      last_led_toggle = HAL_GetTick();
    }
    last_a8 = ca8;

    /* DOWN (D0): reset chart */
    uint8_t cd0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (cd0 && !last_d0) {
      reset_chart();
    }
    last_d0 = cd0;

    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    update_chart_data(raw.red, raw.green, raw.blue);

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();

      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");

        /* Left-aligned chart title jyro chart 03 style */
        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(16, 33, "Chart: RGB");

        /* Chart shifted down: chart_y = 50, TOS_GREY axes */
        int chart_x = 10, chart_y = 50, chart_w = 220, chart_h = 90;
        draw_chart_axes(chart_x, chart_y, chart_w, chart_h, chart_max_value);
        draw_chart_all(chart_x, chart_y, chart_w, chart_h, chart_max_value);

        /* LED indicator */
        PD_SetFont(FONT_ASCII_12);
        PD_FillRect(170, 147, 30, 16, led_on ? TOS_ACCENT : TOS_GREY);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(176, 149, "LED");

        /* Legend colored fill rectangles with R/G/B labels (jyro style) */
        PD_FillRect(10, 148, 10, 8, 0xFF0000);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(23, 147, "R");

        PD_FillRect(60, 148, 10, 8, 0x00FF00);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(73, 147, "G");

        PD_FillRect(110, 148, 10, 8, 0x0000FF);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(123, 147, "B");

        /* Current values below chart in matching colors */
        char dbg[48];
        PD_SetColor(0xFF0000);
        snprintf(dbg, sizeof(dbg), "R:%-5u", raw.red);
        PD_DrawString(10, 165, dbg);

        PD_SetColor(0x00FF00);
        snprintf(dbg, sizeof(dbg), "G:%-5u", raw.green);
        PD_DrawString(60, 165, dbg);

        PD_SetColor(0x0000FF);
        snprintf(dbg, sizeof(dbg), "B:%-5u", raw.blue);
        PD_DrawString(110, 165, dbg);

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

/* Main activity (standard menu loop) */
#define TCS3472_N 5
void tcs3472_activity(void) {
  if (!boardTCS3472.isInitialized()) {
    boardTCS3472.init();
    if (!boardTCS3472.isInitialized()) {
      alert_show("ALERT", "Init Failed!\nCheck I2C.");
      return;
    }
  }

  const char *items[TCS3472_N] = {"00 Return", "01 Read Raw", "02 Color Demo",
                                  "03 CCT & Lux", "04 Chart"};

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % TCS3472_N;
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + TCS3472_N) % TCS3472_N;
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 1:
        tcs3472_read_subpage();
        break;
      case 2:
        tcs3472_color_subpage();
        break;
      case 3:
        tcs3472_cct_subpage();
        break;
      case 4:
        tcs3472_chart_subpage();
        break;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");
        PD_SetFont(FONT_ASCII_16);
        int vis = TCS3472_N < 7 ? TCS3472_N : 7;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > TCS3472_N)
          start = TCS3472_N - vis;
        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= TCS3472_N)
            break;
          int cy = 33 + i * 25;
          UI_DrawMenuCard(idx, sel, cy, items[idx]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}
