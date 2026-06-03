/**
 ******************************************************************************
 * @file    bmp_activity.cpp
 * @author  Typheye
 * @brief   Bmp Activity implementation.
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

#include "include/bmp_activity.hpp"
#include "components/include/alert.hpp"
#include "core/sys/include/systime.h"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include <cstdio>

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern BMP180 boardBMP180;
extern KeyManager keyManager;
extern LCD boardLCD;

#define BM_N 3
#define BMP_RT_N 5
#define BMP_RT_VIS 5
#define CHART_HISTORY 120

static const char *bmp_menus[BM_N] = {
    "00 Return",
    "01 Real-time",
    "02 Chart",
};

static float reference_pressure = 1013.25f;
static float altitude_offset = 0.0f;

static CCMRAM float chart_temp[CHART_HISTORY];
static CCMRAM float chart_press[CHART_HISTORY];
static CCMRAM int chart_idx = 0;
static CCMRAM float temp_max = 50.0f;
static CCMRAM float temp_min = -20.0f;
static CCMRAM float press_max = 1100.0f;
static CCMRAM float press_min = 900.0f;
static CCMRAM int chart_mode = 0;

/* ==================================================================
 *  Standard template functions
 * ================================================================== */

static void draw_frame_title(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  extern TRTC boardTRTC;
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 1000) {
    last_tm = HAL_GetTick();
    Time_t t;
    Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8];
    time_fmt(ts, sizeof(ts), t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        const char *value) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

/* ==================================================================
 *  Chart data helpers
 * ================================================================== */

static void update_chart_data(float t, float p) {
  chart_temp[chart_idx] = t;
  chart_press[chart_idx] = p;
  chart_idx++;
  if (chart_idx >= CHART_HISTORY)
    chart_idx = 0;

  if (t > temp_max)
    temp_max = t + 2;
  if (t < temp_min)
    temp_min = t - 2;
  if (p > press_max)
    press_max = p + 20;
  if (p < press_min)
    press_min = p - 20;
  if (temp_max - temp_min > 100) {
    temp_max = temp_min + 100;
  }
  if (press_max - press_min > 500) {
    press_max = press_min + 500;
  }
}

static void reset_chart(void) {
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_temp[i] = 25.0f;
    chart_press[i] = 1013.25f;
  }
  chart_idx = 0;
  temp_max = 50;
  temp_min = -20;
  press_max = 1100;
  press_min = 900;
}

static void draw_chart_axes(int x, int y, int w, int h, float mx, float mn,
                            const char *unit) {
  PD_SetColor(TOS_GREY);
  PD_DrawRect(x, y, w, h);
  for (int i = 1; i <= 3; i++) {
    int ly = y + (h * i / 4);
    PD_DrawLine(x, ly, x + w, ly);
  }
}

static void draw_chart_line(float *data, int x, int y, int w, int h, float mx,
                            float mn, uint32_t color) {
  PD_SetColor(color);
  float r = mx - mn;
  if (r < 0.01f)
    r = 1.0f;
  for (int i = 1; i < w && i < CHART_HISTORY; i++) {
    int pv = (chart_idx - 1 - i + CHART_HISTORY) % CHART_HISTORY;
    int cv = (chart_idx - i + CHART_HISTORY) % CHART_HISTORY;
    int y1 = y + h - (int)((data[pv] - mn) * h / r);
    int y2 = y + h - (int)((data[cv] - mn) * h / r);
    if (y1 < y)
      y1 = y;
    if (y1 > y + h)
      y1 = y + h;
    if (y2 < y)
      y2 = y;
    if (y2 > y + h)
      y2 = y + h;
    PD_DrawLine(x + w - i, y1, x + w - (i - 1), y2);
  }
}

/* ==================================================================
 *  01  Real-time display (scrollable menu with draw_card_r)
 * ================================================================== */

static void bmp180_realtime_activity(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  /* Loading screen */
  LCD_FLUSH({
    draw_frame_title("DEMO");
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, 33, "BMP180 Init...");
  });
  HAL_Delay(100);

  boardBMP180.init();
  if (!boardBMP180.isInitialized()) {
    alert_show("BMP180", "Init Failed!");
    return;
  }

  static const char *rt_items[BMP_RT_N] = {
      "00 Return", "01 Temp", "02 Press", "03 Altiu", "04 Ref-Pre",
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % BMP_RT_N;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + BMP_RT_N) % BMP_RT_N;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();

      BMP180_Data_t d = boardBMP180.readData(BMP180_MODE_STD);
      float alt = boardBMP180.calcAltitude(d.pressure, reference_pressure) -
                  altitude_offset;

      char f[16], db[64];

      /* Format values */
      float_to_str(d.temperature, f);
      snprintf(db, sizeof(db), "%s C", f);
      char val_temp[64];
      snprintf(val_temp, sizeof(val_temp), "%s", db);

      float_to_str(d.pressure, f);
      snprintf(db, sizeof(db), "%s hPa", f);
      char val_press[64];
      snprintf(val_press, sizeof(val_press), "%s", db);

      float_to_str(alt, f);
      snprintf(db, sizeof(db), "%s m", f);
      char val_alt[64];
      snprintf(val_alt, sizeof(val_alt), "%s", db);

      float_to_str(reference_pressure, f);
      snprintf(db, sizeof(db), "%s hPa", f);
      char val_ref[64];
      snprintf(val_ref, sizeof(val_ref), "%s", db);

      const char *rt_vals[BMP_RT_N] = {
          "",        /* 00 Return */
          val_temp,  /* 01 Temp */
          val_press, /* 02 Pressure */
          val_alt,   /* 03 Altitude */
          val_ref,   /* 04 Ref Press */
      };

      LCD_FLUSH({
        draw_frame_title("DEMO");
        PD_SetFont(FONT_ASCII_16);

        int vis = BMP_RT_VIS;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > BMP_RT_N)
          start = BMP_RT_N - vis;

        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= BMP_RT_N)
            break;
          int cy = 33 + i * 25;
          if (idx == 0)
            draw_card(idx, sel, cy, rt_items[idx]);
          else
            draw_card_r(idx, sel, cy, rt_items[idx], rt_vals[idx]);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}

/* ==================================================================
 *  02  Chart (jyro chart 03 style)
 * ================================================================== */

void bmp180_chart_activity(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  /* Loading screen */
  LCD_FLUSH({
    draw_frame_title("DEMO");
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, 33, "BMP180 Init...");
  });
  HAL_Delay(100);

  boardBMP180.init();
  if (!boardBMP180.isInitialized()) {
    alert_show("BMP180", "Init Failed!");
    return;
  }

  reset_chart();
  chart_mode = 0;

  uint32_t lu = 0;
  uint8_t le_a8 = 0, le_d0 = 0;

  while (1) {
    keyManager.btn_enter.tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      return;

    uint8_t ca8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (ca8 && !le_a8) {
      chart_mode = !chart_mode;
      reset_chart();
    }
    le_a8 = ca8;

    uint8_t cd0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (cd0 && !le_d0) {
      chart_mode = !chart_mode;
      reset_chart();
    }
    le_d0 = cd0;

    BMP180_Data_t d = boardBMP180.readData(BMP180_MODE_STD);
    update_chart_data(d.temperature, d.pressure);

    if (HAL_GetTick() - lu > 50) {
      lu = HAL_GetTick();

      float mx = chart_mode ? press_max : temp_max;
      float mn = chart_mode ? press_min : temp_min;
      const char *unit = chart_mode ? "hPa" : "C";
      float *cdata = chart_mode ? chart_press : chart_temp;
      float cval = chart_mode ? d.pressure : d.temperature;
      uint32_t lcol = chart_mode ? TOS_GREEN : TOS_RED;

      char f[16], db[64];

      LCD_FLUSH({
        draw_frame_title("DEMO");

        /* Left-aligned title at x=16 with TOS_TEXT */
        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(16, 33, "Chart: Temp/Press");

        /* Chart at y=50 with proper axes using TOS_GREY for borders */
        int chart_x = 10, chart_y = 50, chart_w = 220, chart_h = 90;
        draw_chart_axes(chart_x, chart_y, chart_w, chart_h, mx, mn, unit);
        draw_chart_line(cdata, chart_x, chart_y, chart_w, chart_h, mx, mn,
                        lcol);

        /* Legend — colored fill rectangles with text labels */
        PD_SetFont(FONT_ASCII_12);

        PD_FillRect(10, 148, 10, 8, TOS_RED);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(23, 146, "Temp");

        PD_FillRect(80, 148, 10, 8, TOS_GREEN);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(93, 146, "Press");

        /* Current value display below chart */
        PD_SetColor(lcol);
        float_to_str(cval, f);
        snprintf(db, sizeof(db), "%s %s", f, unit);
        PD_DrawString(10, 165, db);

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(30);
  }
}

/* ==================================================================
 *  Main BMP180 menu
 * ================================================================== */

void bmp180_activity(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % BM_N;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + BM_N) % BM_N;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 1:
        bmp180_realtime_activity();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 2:
        bmp180_chart_activity();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        draw_frame_title("DEMO");
        PD_SetFont(FONT_ASCII_16);

        for (int i = 0; i < BM_N; i++) {
          int cy = 33 + i * 25;
          draw_card(i, sel, cy, bmp_menus[i]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}
