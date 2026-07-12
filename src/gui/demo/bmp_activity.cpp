/**
 ******************************************************************************
 * @file    bmp_activity.cpp
 * @author  Typheye
 * @brief   Bmp Activity implementation.
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

#include "include/bmp_activity.hpp"
#include "dram.h"
#include "library/include/libdly.h"
#include "library/include/libui.h"

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

static float *chart_temp = nullptr;
static float *chart_press = nullptr;
static CCMRAM int chart_idx = 0;
static CCMRAM float temp_max = 50.0f;
static CCMRAM float temp_min = -20.0f;
static CCMRAM float press_max = 1100.0f;
static CCMRAM float press_min = 900.0f;
static CCMRAM int chart_mode = 0;

static bool chart_alloc(void) {
  if (chart_temp && chart_press)
    return true;
  chart_temp = (float *)SysDram_AllocFast(sizeof(float) * CHART_HISTORY);
  chart_press = (float *)SysDram_AllocFast(sizeof(float) * CHART_HISTORY);
  if (chart_temp && chart_press)
    return true;
  SysDram_Free(chart_temp);
  SysDram_Free(chart_press);
  chart_temp = nullptr;
  chart_press = nullptr;
  return false;
}

static void chart_free(void) {
  SysDram_Free(chart_temp);
  SysDram_Free(chart_press);
  chart_temp = nullptr;
  chart_press = nullptr;
}

/* ==================================================================
 *  Standard template functions
 * ================================================================== */

/* ==================================================================
 *  Chart data helpers
 * ================================================================== */

static void update_chart_data(float t, float p) {
  if (!chart_temp || !chart_press)
    return;
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
  if (temp_max - temp_min > 16) {
    temp_max = temp_min + 100;
  }
  if (press_max - press_min > 500) {
    press_max = press_min + 500;
  }
}

static void reset_chart(void) {
  if (!chart_temp || !chart_press)
    return;
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
  PD_SetColor(LV_PRIMARY);
  PD_DrawRect(x, y, w, h);
  for (int i = 1; i <= 3; i++) {
    int ly = y + (h * i / 4);
    PD_SetColor(LV_BG_DARK);
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
  /* Loading screen */
  LCD_FLUSH({
    UI_DrawFrameTitle("DEMO");
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, 33, "BMP180 Init...");
  });
  JPDelay(45);

  boardBMP180.init();
  if (!boardBMP180.isInitialized()) {
    alert_show("ALERT", "Init Failed!");
    return;
  }

  static const char *rt_items[BMP_RT_N] = {
      "00 Return", "01 Temp", "02 Press", "03 Altiu", "04 Ref-Pre",
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (keyManager._collisionA8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % BMP_RT_N;
      JPDelay(45);
    }
    if (keyManager._collisionD0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + BMP_RT_N) % BMP_RT_N;
      JPDelay(45);
    }

    uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
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
        UI_DrawFrameTitle("DEMO");
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
            UI_DrawMenuCard(idx, sel, cy, rt_items[idx]);
          else
            UI_DrawMenuValue(idx, sel, cy, rt_items[idx], rt_vals[idx], false);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

/* ==================================================================
 *  02  Chart (jyro chart 03 style)
 * ================================================================== */

void bmp180_chart_activity(void) {
  /* Loading screen */
  LCD_FLUSH({
    UI_DrawFrameTitle("DEMO");
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, 33, "BMP180 Init...");
  });
  JPDelay(45);

  boardBMP180.init();
  if (!boardBMP180.isInitialized()) {
    alert_show("ALERT", "Init Failed!");
    return;
  }
  if (!chart_alloc()) {
    alert_show("ALERT", "Chart memory failed");
    return;
  }

  reset_chart();
  chart_mode = 0;

  uint32_t lu = 0;
  uint8_t le_a8 = 0, le_d0 = 0;

  while (1) {
    keyManager._btnEnter.tick();
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();

    if (keyManager._btnEnter.getState() == KEY_PRESSED) {
      chart_free();
      return;
    }

    uint8_t ca8 = (keyManager._collisionA8.getState() == KEY_PRESSED);
    if (ca8 && !le_a8) {
      chart_mode = !chart_mode;
      reset_chart();
    }
    le_a8 = ca8;

    uint8_t cd0 = (keyManager._collisionD0.getState() == KEY_PRESSED);
    if (cd0 && !le_d0) {
      chart_mode = !chart_mode;
      reset_chart();
    }
    le_d0 = cd0;

    BMP180_Data_t d = boardBMP180.readData(BMP180_MODE_STD);
    update_chart_data(d.temperature, d.pressure);

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();

      float mx = chart_mode ? press_max : temp_max;
      float mn = chart_mode ? press_min : temp_min;
      const char *unit = chart_mode ? "hPa" : "C";
      float *cdata = chart_mode ? chart_press : chart_temp;
      float cval = chart_mode ? d.pressure : d.temperature;
      uint32_t lcol = chart_mode ? TOS_GREEN : TOS_RED;

      char f[16], db[64];

      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");

        /* Left-aligned title at x=16 with TOS_TEXT */
        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(16, 33, "Chart: Temp/Press");

        /* Chart at y=50 with proper axes using TOS_GREY for borders */
        int chart_x = 10, chart_y = 50, chart_w = 220, chart_h = 90;
        draw_chart_axes(chart_x, chart_y, chart_w, chart_h, mx, mn, unit);
        draw_chart_line(cdata, chart_x, chart_y, chart_w, chart_h, mx, mn,
                        lcol);

        /* Legend colored fill rectangles with text labels */
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
    JPDelay(30);
  }
}

/* ==================================================================
 *  Main BMP180 menu
 * ================================================================== */

void bmp180_activity(void) {
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (keyManager._collisionA8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % BM_N;
      JPDelay(45);
    }
    if (keyManager._collisionD0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + BM_N) % BM_N;
      JPDelay(45);
    }

    uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 1:
        bmp180_realtime_activity();
        break;
      case 2:
        bmp180_chart_activity();
        break;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");
        PD_SetFont(FONT_ASCII_16);

        for (int i = 0; i < BM_N; i++) {
          int cy = 33 + i * 25;
          UI_DrawMenuCard(i, sel, cy, bmp_menus[i]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}
