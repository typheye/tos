/**
 ******************************************************************************
 * @file    jyro_activity.cpp
 * @author  Typheye
 * @brief   Jyro Activity implementation.
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

#include "include/jyro_activity.hpp"
#include "library/include/libdly.h"
#include "core/sys/include/sysdram.h"


#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern LCD boardLCD;
extern JY901S boardJY901S;
extern KeyManager keyManager;

/* ── Shared chart state ── */
#define CHART_HISTORY 240
static float (*chart_data)[CHART_HISTORY] = nullptr;
static CCMRAM int chart_index = 0;
static CCMRAM float chart_max_value = 10.0f;
static CCMRAM int chart_param_group = 0;

static const char *group_names[3] = {"Acceleration", "Angular Velocity",
                                     "Euler Angles"};

static bool chart_alloc(void) {
  if (chart_data)
    return true;
  chart_data = (float (*)[CHART_HISTORY])SysDram_AllocFast(sizeof(float) * 3U * CHART_HISTORY);
  return chart_data != nullptr;
}

static void chart_free(void) {
  SysDram_Free(chart_data);
  chart_data = nullptr;
}

/* ── Standard template functions ── */
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

/* ── Chart helper functions ── */
static void reset_chart(void) {
  if (!chart_data)
    return;
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_data[0][i] = 0;
    chart_data[1][i] = 0;
    chart_data[2][i] = 0;
  }
  chart_index = 0;
  chart_max_value = 10.0f;
}

static void update_chart_data(float v0, float v1, float v2) {
  if (!chart_data)
    return;
  chart_data[0][chart_index] = v0;
  chart_data[1][chart_index] = v1;
  chart_data[2][chart_index] = v2;
  chart_index++;
  if (chart_index >= CHART_HISTORY)
    chart_index = 0;

  float max_val = fabsf(v0);
  if (fabsf(v1) > max_val)
    max_val = fabsf(v1);
  if (fabsf(v2) > max_val)
    max_val = fabsf(v2);

  if (max_val > chart_max_value)
    chart_max_value = max_val * 1.1f;
  else if (chart_max_value > 5.0f && max_val < chart_max_value / 2)
    chart_max_value = chart_max_value * 0.9f;
  if (chart_max_value < 0.1f)
    chart_max_value = 1.0f;
}

static void draw_chart_axes(int x, int y, int width, int height,
                            float max_val) {
  PD_SetColor(TOS_TEXT_SEC);
  PD_DrawRect(x, y, width, height);
  for (int i = 1; i <= 3; i++) {
    int line_y = y + (height * i / 4);
    PD_SetColor(TOS_TEXT_SEC);
    PD_DrawLine(x, line_y, x + width, line_y);
  }
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(TOS_TEXT_SEC);
  char label[16];
  snprintf(label, sizeof(label), "%.0f", max_val);
  PD_DrawString(x - 25, y - 4, label);
  snprintf(label, sizeof(label), "%.0f", -max_val);
  PD_DrawString(x - 25, y + height - 4, label);
}

static void draw_chart_line(float *data, int count, int x, int y, int width,
                            int height, float max_val, uint32_t color) {
  if (count < 2)
    return;
  PD_SetColor(color);
  int center_y = y + height / 2;
  for (int i = 1; i < width && i < count; i++) {
    int idx_prev = (chart_index - 1 - i + count) % count;
    int idx_curr = (chart_index - i + count) % count;
    float val_prev = data[idx_prev];
    float val_curr = data[idx_curr];
    if (val_prev > max_val)
      val_prev = max_val;
    if (val_prev < -max_val)
      val_prev = -max_val;
    if (val_curr > max_val)
      val_curr = max_val;
    if (val_curr < -max_val)
      val_curr = -max_val;
    int y1 = center_y - (int)(val_prev * height / (2 * max_val));
    int y2 = center_y - (int)(val_curr * height / (2 * max_val));
    int x1 = x + width - i;
    int x2 = x + width - (i - 1);
    if (y1 >= y && y1 <= y + height && y2 >= y && y2 <= y + height) {
      PD_DrawLine(x1, y1, x2, y2);
    }
  }
}

static void draw_chart_all(int x, int y, int width, int height, float max_val) {
  uint32_t colors[3] = {0xFF0000, 0x00FF00, 0x0000FF};
  for (int i = 0; i < 3; i++) {
    draw_chart_line(chart_data[i], CHART_HISTORY, x, y, width, height, max_val,
                    colors[i]);
  }
}

/* ── 01 3D Cube sub-page ── */
static void jyro_cube_subpage(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  /* Loading screen */
  LCD_FLUSH({
    draw_frame_title("DEMO");
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(16, 33, "Initializing gyro...");
  });
  JPDelay(500);

  gyro_cube_init(120, 120, 80);

#define SMOOTH_WINDOW 3
  float gyro_x_history[SMOOTH_WINDOW] = {0};
  float gyro_y_history[SMOOTH_WINDOW] = {0};
  float gyro_z_history[SMOOTH_WINDOW] = {0};
  int history_index = 0;
  float roll = 0, pitch = 0, yaw = 0;
  uint32_t last_time = HAL_GetTick();
  char fstr[16];
  char display_str[32];

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      break;

    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f)
      dt = 0.02f;
    last_time = now;

    JY901S_Data_t data = boardJY901S.readData();

    gyro_x_history[history_index] = data.gyro_x;
    gyro_y_history[history_index] = data.gyro_y;
    gyro_z_history[history_index] = data.gyro_z;

    float smooth_gyro_x = 0, smooth_gyro_y = 0, smooth_gyro_z = 0;
    for (int i = 0; i < SMOOTH_WINDOW; i++) {
      smooth_gyro_x += gyro_x_history[i];
      smooth_gyro_y += gyro_y_history[i];
      smooth_gyro_z += gyro_z_history[i];
    }
    smooth_gyro_x /= SMOOTH_WINDOW;
    smooth_gyro_y /= SMOOTH_WINDOW;
    smooth_gyro_z /= SMOOTH_WINDOW;
    history_index = (history_index + 1) % SMOOTH_WINDOW;

    roll += smooth_gyro_x * dt * 0.0174533f;
    pitch += smooth_gyro_y * dt * 0.0174533f;
    yaw += smooth_gyro_z * dt * 0.0174533f;

    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(
        -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));
    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    if (roll > 3.14159f)
      roll -= 6.28318f;
    if (roll < -3.14159f)
      roll += 6.28318f;
    if (pitch > 3.14159f)
      pitch -= 6.28318f;
    if (pitch < -3.14159f)
      pitch += 6.28318f;
    if (yaw > 3.14159f)
      yaw -= 6.28318f;
    if (yaw < -3.14159f)
      yaw += 6.28318f;

    LCD_FLUSH({
      PD_FillScreen(TOS_BG);
      gyro_cube_draw(roll, pitch, yaw);

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(TOS_YELLOW);
      float_to_str(roll * 57.29578f, fstr);
      sprintf(display_str, "Roll : %s", fstr);
      PD_DrawString(5, 5, display_str);

      float_to_str(pitch * 57.29578f, fstr);
      sprintf(display_str, "Pitch: %s", fstr);
      PD_DrawString(5, 18, display_str);

      float_to_str(yaw * 57.29578f, fstr);
      sprintf(display_str, "Yaw  : %s", fstr);
      PD_DrawString(5, 31, display_str);

      PD_DrawFooterCenter("ENTER", NULL, NULL);
    });
  }
}

/* ── 02 Text Data sub-page (scrollable menu) ── */
#define TEXT_N 10
static const char *text_labels[TEXT_N] = {
    "00 Return", "01 Acc X", "   Acc Y", "   Acc Z", "02 Gyr X",
    "   Gyr Y",  "   Gyr Z", "03 Roll",  "   Pitch", "   Yaw"};

static void jyro_text_subpage(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  float roll = 0, pitch = 0, yaw = 0;
  uint32_t last_time = 0;

  char values[TEXT_N][32];

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % TEXT_N;
      JPDelay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + TEXT_N) % TEXT_N;
      JPDelay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    /* Refresh data every 200ms */
    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();

      JY901S_Data_t data = boardJY901S.readData();

      uint32_t now = HAL_GetTick();
      float dt = (now - last_time) / 1000.0f;
      if (dt > 0.05f)
        dt = 0.02f;
      last_time = now;

      roll += data.gyro_x * dt * 0.0174533f;
      pitch += data.gyro_y * dt * 0.0174533f;
      yaw += data.gyro_z * dt * 0.0174533f;
      float roll_acc = atan2(data.acc_y, data.acc_z);
      float pitch_acc = atan2(
          -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));
      roll = roll * 0.98f + roll_acc * 0.02f;
      pitch = pitch * 0.98f + pitch_acc * 0.02f;

      char fstr[16];

      float_to_str(data.acc_x, fstr);
      snprintf(values[1], sizeof(values[1]), "%s g", fstr);

      float_to_str(data.acc_y, fstr);
      snprintf(values[2], sizeof(values[2]), "%s g", fstr);

      float_to_str(data.acc_z, fstr);
      snprintf(values[3], sizeof(values[3]), "%s g", fstr);

      float_to_str(data.gyro_x, fstr);
      snprintf(values[4], sizeof(values[4]), "%s d/s", fstr);

      float_to_str(data.gyro_y, fstr);
      snprintf(values[5], sizeof(values[5]), "%s d/s", fstr);

      float_to_str(data.gyro_z, fstr);
      snprintf(values[6], sizeof(values[6]), "%s d/s", fstr);

      float_to_str(roll * 57.29578f, fstr);
      snprintf(values[7], sizeof(values[7]), "%s deg", fstr);

      float_to_str(pitch * 57.29578f, fstr);
      snprintf(values[8], sizeof(values[8]), "%s deg", fstr);

      float_to_str(yaw * 57.29578f, fstr);
      snprintf(values[9], sizeof(values[9]), "%s deg", fstr);

      LCD_FLUSH({
        draw_frame_title("DEMO");
        PD_SetFont(FONT_ASCII_16);

        int vis = TEXT_N < 7 ? TEXT_N : 7;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > TEXT_N)
          start = TEXT_N - vis;

        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= TEXT_N)
            break;
          int cy = 33 + i * 25;
          if (idx == 0)
            draw_card(idx, sel, cy, text_labels[idx]);
          else
            draw_card_r(idx, sel, cy, text_labels[idx], values[idx]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

/* ── 03 Chart sub-page ── */
static void jyro_chart_subpage(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  if (!chart_alloc()) {
    LCD_FLUSH({
      PD_Init();
      PD_FillScreen(TOS_BG);
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(TOS_RED);
      PD_DrawString(24, 94, "Chart memory failed");
    });
    JPDelay(900);
    return;
  }
  reset_chart();
  chart_param_group = 0;

  uint32_t lu = 0;
  uint8_t le_a8 = 0, le_d0 = 0;
  float roll = 0, pitch = 0, yaw = 0;
  uint32_t last_time = 0;

  while (1) {
    keyManager.btn_enter.tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      chart_free();
      return;
    }

    uint8_t ca8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (ca8 && !le_a8) {
      chart_param_group = (chart_param_group + 1) % 3;
      reset_chart();
    }
    le_a8 = ca8;

    uint8_t cd0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (cd0 && !le_d0) {
      chart_param_group = (chart_param_group - 1 + 3) % 3;
      reset_chart();
    }
    le_d0 = cd0;

    JY901S_Data_t data = boardJY901S.readData();
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f)
      dt = 0.02f;
    last_time = now;

    roll += data.gyro_x * dt * 0.0174533f;
    pitch += data.gyro_y * dt * 0.0174533f;
    yaw += data.gyro_z * dt * 0.0174533f;
    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(
        -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));
    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    float val0, val1, val2;
    switch (chart_param_group) {
    case 0:
      val0 = data.acc_x;
      val1 = data.acc_y;
      val2 = data.acc_z;
      break;
    case 1:
      val0 = data.gyro_x;
      val1 = data.gyro_y;
      val2 = data.gyro_z;
      break;
    case 2:
      val0 = roll * 57.29578f;
      val1 = pitch * 57.29578f;
      val2 = yaw * 57.29578f;
      break;
    default:
      val0 = val1 = val2 = 0;
    }
    update_chart_data(val0, val1, val2);

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        draw_frame_title("DEMO");

        /* Group name �?left-aligned at x=16 with TOS_TEXT */
        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(TOS_TEXT);
        char title[32];
        snprintf(title, sizeof(title), "Chart: %s",
                 group_names[chart_param_group]);
        PD_DrawString(16, 33, title);

        /* Chart �?shifted down: chart_y = 50 */
        int chart_x = 10, chart_y = 50, chart_w = 220, chart_h = 90;
        draw_chart_axes(chart_x, chart_y, chart_w, chart_h, chart_max_value);
        draw_chart_all(chart_x, chart_y, chart_w, chart_h, chart_max_value);

        /* Color legend �?small filled rectangles with abbreviated labels */
        const char *lnames[3];
        switch (chart_param_group) {
        case 0:
          lnames[0] = "AccX";
          lnames[1] = "AccY";
          lnames[2] = "AccZ";
          break;
        case 1:
          lnames[0] = "GyrX";
          lnames[1] = "GyrY";
          lnames[2] = "GyrZ";
          break;
        case 2:
          lnames[0] = "Roll";
          lnames[1] = "Pitch";
          lnames[2] = "Yaw";
          break;
        }

        uint32_t lcolors[3] = {0xFF0000, 0x00FF00, 0x0000FF};
        PD_SetFont(FONT_ASCII_12);
        for (int i = 0; i < 3; i++) {
          PD_FillRect(10 + i * 75, 148, 10, 8, lcolors[i]);
          PD_SetColor(TOS_TEXT);
          PD_DrawString(23 + i * 75, 146, lnames[i]);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

/* ── Main activity (standard menu loop) ── */
#define JYRO_N 4
void jyro_activity(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  const char *items[JYRO_N] = {"00 Return", "01 3D Cube", "02 Text Data",
                               "03 Chart"};

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % JYRO_N;
      JPDelay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + JYRO_N) % JYRO_N;
      JPDelay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 1:
        jyro_cube_subpage();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 2:
        jyro_text_subpage();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 3:
        jyro_chart_subpage();
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

        int vis = JYRO_N < 7 ? JYRO_N : 7;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > JYRO_N)
          start = JYRO_N - vis;

        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= JYRO_N)
            break;
          int cy = 33 + i * 25;
          draw_card(idx, sel, cy, items[idx]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}
