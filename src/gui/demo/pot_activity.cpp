#include "include/pot_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/pot.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern KeyManager keyManager;
extern LCD boardLCD;
extern Potentiometer boardPot;

#define POT_MENU_ITEMS 4
static const char *pot_menus[POT_MENU_ITEMS] = {
    "01 Monitor", "02 Chart", "03 Calibrate", "04 Back"};

static int menu_select = 0;

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

#define CHART_WIDTH 240
static CCMRAM uint16_t chart_data[CHART_WIDTH] = {0};
static CCMRAM int chart_index = 0;

static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, t);
}
static void bbar(const char *l, const char *m, const char *r) {
  PD_DrawFooterCenter(l, m, r);
}
static void menu_cards(int sel) {
  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < POT_MENU_ITEMS; i++) {
    int cy = 33 + i * 25;
    if (i == sel) {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }
    PD_DrawString(26, cy + 2, pot_menus[i]);
  }
}

static void draw_gauge(float percentage, int x, int y, int radius) {
  PD_SetColor(LV_BORDER);
  PD_DrawCircle(x, y, radius);

  int angle = (int)(percentage * 360 / 100);
  if (angle > 360) angle = 360;

  for (int a = 0; a < angle; a += 10) {
    float rad = a * 3.14159f / 180.0f;
    int x1 = x + (int)((radius - 5) * cosf(rad));
    int y1 = y + (int)((radius - 5) * sinf(rad));
    PD_SetColor(LV_SUCCESS);
    PD_DrawLine(x, y, x1, y1);
  }

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LV_ACCENT);
  char dbg[16];
  snprintf(dbg, sizeof(dbg), "%.0f%%", percentage);
  PD_DrawString(x - 20, y + radius + 4, dbg);
}

static void draw_chart(uint16_t *data, int count, int x, int y, int width,
                       int height) {
  PD_SetColor(LV_BORDER);
  PD_DrawRect(x, y, width, height);

  for (int i = 1; i <= 3; i++) {
    int line_y = y + (height * i / 4);
    PD_SetColor(LV_BORDER);
    PD_DrawLine(x, line_y, x + width, line_y);
  }

  uint16_t max_val = 0;
  for (int i = 0; i < count && i < width; i++) {
    if (data[i] > max_val) max_val = data[i];
  }
  if (max_val == 0) max_val = 4095;

  PD_SetColor(LV_ACCENT);
  for (int i = 1; i < width && i < count; i++) {
    int prev_x = x + i - 1;
    int prev_y = y + height - (int)((float)data[i - 1] * height / max_val);
    int curr_x = x + i;
    int curr_y = y + height - (int)((float)data[i] * height / max_val);
    if (prev_y >= y && prev_y <= y + height && curr_y >= y && curr_y <= y + height)
      PD_DrawLine(prev_x, prev_y, curr_x, curr_y);
  }
}

void pot_monitor_activity(void) {
  uint32_t lu = HAL_GetTick();
  char fstr[16]; char dbg[64];

  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    if (HAL_GetTick() - lu > 50) {
      lu = HAL_GetTick();
      Pot_Data_t data = boardPot.readAll();

      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);
        bar("11");

        PD_DrawAngledCard(8, 44, 224, 78, 6, TOS_CARD_BG);
        draw_gauge(data.percentage, 55, 80, 32);

        PD_SetFont(FONT_ASCII_16);
        snprintf(dbg, sizeof(dbg), "ADC: %4d", data.adc_raw);
        PD_SetColor(LV_ACCENT);
        PD_DrawString(110, 52, dbg);

        float_to_str(data.voltage, fstr);
        snprintf(dbg, sizeof(dbg), "V: %s V", fstr);
        PD_SetColor(LV_TEXT_PRIMARY);
        PD_DrawString(110, 74, dbg);

        float_to_str(data.resistance, fstr);
        snprintf(dbg, sizeof(dbg), "R: %s k", fstr);
        PD_SetColor(LV_TEXT_PRIMARY);
        PD_DrawString(110, 96, dbg);

        PD_SetFont(FONT_ASCII_20);
        float_to_str(data.percentage, fstr);
        snprintf(dbg, sizeof(dbg), "%s%%", fstr);
        PD_SetColor(LV_SUCCESS);
        PD_DrawString(190, 56, dbg);

        PD_DrawProgressBar(10, 140, 220, 16, data.percentage, LV_SUCCESS, LV_BG_DARK);

        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(LV_TEXT_HINT);
        PD_DrawString(16, 168, "Turn the potentiometer");

        bbar("EXIT", NULL, NULL);
      });
    }
    HAL_Delay(1);
  }
}

void pot_chart_activity(void) {
  uint32_t lu = HAL_GetTick();
  uint8_t la8 = 0;

  for (int i = 0; i < CHART_WIDTH; i++) chart_data[i] = 0;
  chart_index = 0;
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    uint8_t ca = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (ca && !la8) {
      for (int i = 0; i < CHART_WIDTH; i++) chart_data[i] = 0;
      chart_index = 0;
    }
    la8 = ca;

    uint16_t adc_raw = boardPot.readRaw();
    chart_data[chart_index] = adc_raw;
    chart_index++;
    if (chart_index >= CHART_WIDTH) {
      for (int i = 0; i < CHART_WIDTH - 1; i++) chart_data[i] = chart_data[i + 1];
      chart_index = CHART_WIDTH - 1;
    }

    if (HAL_GetTick() - lu > 50) {
      lu = HAL_GetTick();

      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);
        bar("11");

        draw_chart(chart_data, CHART_WIDTH, 8, 44, 224, 90);

        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(LV_ACCENT);
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "ADC: %4d  (%.2fV)", adc_raw, (float)adc_raw * 3.3f / 4095.0f);
        PD_DrawString(8, 142, dbg);

        float percentage = (float)adc_raw * 100.0f / 4095.0f;
        snprintf(dbg, sizeof(dbg), "Position: %.1f%%", percentage);
        PD_SetColor(LV_TEXT_PRIMARY);
        PD_DrawString(8, 164, dbg);

        PD_DrawProgressBar(8, 190, 224, 14, percentage, LV_SUCCESS, LV_BG_DARK);

        bbar("EXIT", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}

void pot_calibrate_activity(void) {
  int cal_step = 0;
  uint32_t lu = HAL_GetTick();
  uint8_t le = 0;

  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    keyManager.collision_D0.tick();

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);

    if (keyManager.collision_D0.getState() == KEY_PRESSED) break;

    if (ce && !le) {
      if (cal_step == 0) {
        boardPot.calibrateMin();
        cal_step = 1;
      } else if (cal_step == 1) {
        boardPot.calibrateMax();
        cal_step = 2;
        break;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();

      uint16_t raw = boardPot.readRaw();
      float voltage = (float)raw * 3.3f / 4095.0f;

      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);
        bar("11");

        PD_DrawAngledCard(8, 44, 224, 70, 6, TOS_CARD_BG);
        PD_SetFont(FONT_ASCII_16);

        if (cal_step == 0) {
          PD_SetColor(LV_WARNING);
          PD_DrawString(16, 54, "Step 1: Set MIN position");
          PD_SetColor(LV_TEXT_HINT);
          PD_DrawString(16, 76, "Rotate CCW");
        } else {
          PD_SetColor(LV_WARNING);
          PD_DrawString(16, 54, "Step 2: Set MAX position");
          PD_SetColor(LV_TEXT_HINT);
          PD_DrawString(16, 76, "Rotate CW");
        }

        PD_SetColor(LV_TEXT_PRIMARY);
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "ADC: %4d  (%.2f V)", raw, voltage);
        PD_DrawString(16, 126, dbg);

        float percentage = (float)raw * 100.0f / 4095.0f;
        PD_DrawProgressBar(16, 160, 208, 18, percentage, LV_SUCCESS, LV_BG_DARK);

        PD_SetColor(LV_ACCENT);
        PD_DrawString(16, 188, "Press Enter to set");

        bbar("EXIT", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(50);
  }
}

void pot_activity(void) {
  uint8_t le = 0;
  uint32_t lu = HAL_GetTick();

  PD_Init();
  menu_select = 0;
  boardPot.init();
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= POT_MENU_ITEMS) menu_select = POT_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0) menu_select--;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (menu_select) {
      case 0: pot_monitor_activity(); break;
      case 1: pot_chart_activity(); break;
      case 2: pot_calibrate_activity(); break;
      case 3: return;
      }
      PD_FillScreen(LV_BG_DARK);
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);
        bar("11");
        menu_cards(menu_select);

        uint16_t preview = boardPot.readRaw();
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(LV_ACCENT);
        char dbg[48];
        snprintf(dbg, sizeof(dbg), "ADC: %4d  (%.2fV)", preview, (float)preview * 3.3f / 4095.0f);
        PD_DrawString(16, 184, dbg);

        float pct = (float)preview * 100.0f / 4095.0f;
        PD_DrawProgressBar(12, 206, 216, 4, pct, LV_SUCCESS, LV_BG_DARK);

        bbar("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}
