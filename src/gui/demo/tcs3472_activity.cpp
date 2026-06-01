#include "include/tcs3472_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/tcs3472.hpp"
#include <stdio.h>
#include "syslog.h"
#include <string.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;
extern TCS3472 boardTCS3472;

#define TCS3472_MENU_ITEMS 5
static const char *menus[TCS3472_MENU_ITEMS] = {
    "01 Read", "02 Color Demo", "03 CCT & Lux", "04 Chart", "05 Back"};

static int menu_select = 0;

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

#define CHART_HISTORY 240
static CCMRAM uint16_t chart_r[CHART_HISTORY] = {0};
static CCMRAM uint16_t chart_g[CHART_HISTORY] = {0};
static CCMRAM uint16_t chart_b[CHART_HISTORY] = {0};
static CCMRAM int chart_index = 0;
static CCMRAM uint16_t chart_max_value = 65535;

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
  for (int i = 0; i < TCS3472_MENU_ITEMS; i++) {
    int cy = 33 + i * 25;
    if (i == sel) {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }
    PD_DrawString(26, cy + 2, menus[i]);
  }
}

static void show_color_block(uint16_t r, uint16_t g, uint16_t b) {
  uint32_t color = ((r >> 8) << 16) | ((g >> 8) << 8) | (b >> 8);
  PD_SetColor(color);
  PD_SetFill(true);
  PD_DrawRoundRect(20, 145, 200, 55, 4);
  PD_SetFill(false);
  PD_SetColor(LV_BORDER);
  PD_DrawRoundRect(20, 145, 200, 55, 4);
}

static void draw_chart_axes(int x, int y, int w, int h, uint16_t max_val) {
  PD_SetColor(LV_BORDER);
  PD_DrawRect(x, y, w, h);
  for (int i = 1; i <= 3; i++) {
    int line_y = y + (h * i / 4);
    PD_SetColor(LV_BORDER);
    PD_DrawLine(x, line_y, x + w, line_y);
  }
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LV_TEXT_HINT);
  char label[8];
  snprintf(label, sizeof(label), "%u", max_val);
  PD_DrawString(x - 25, y - 4, label);
  snprintf(label, sizeof(label), "%u", max_val / 2);
  PD_DrawString(x - 25, y + h / 2 - 4, label);
  snprintf(label, sizeof(label), "0");
  PD_DrawString(x - 10, y + h - 4, label);
}

static void draw_chart_line(uint16_t *data, int count, int x, int y, int w,
                            int h, uint16_t max_val, uint32_t color) {
  if (count < 2) return;
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
  draw_chart_line(chart_r, CHART_HISTORY, x, y, w, h, max_val, LV_ERROR);
  draw_chart_line(chart_g, CHART_HISTORY, x, y, w, h, max_val, LV_SUCCESS);
  draw_chart_line(chart_b, CHART_HISTORY, x, y, w, h, max_val, LV_PRIMARY);
}

static void update_chart_data(uint16_t r, uint16_t g, uint16_t b) {
  chart_r[chart_index] = r; chart_g[chart_index] = g; chart_b[chart_index] = b;
  chart_index++;
  if (chart_index >= CHART_HISTORY) chart_index = 0;
  static uint16_t max_r = 0, max_g = 0, max_b = 0;
  if (r > max_r) max_r = r;
  if (g > max_g) max_g = g;
  if (b > max_b) max_b = b;
  uint16_t new_max = (max_r > max_g) ? max_r : max_g;
  new_max = (new_max > max_b) ? new_max : max_b;
  if (new_max > chart_max_value) chart_max_value = new_max;
  else if (chart_max_value > 500 && new_max < chart_max_value / 2) chart_max_value = chart_max_value * 3 / 4;
  if (chart_max_value < 1000) chart_max_value = 1000;
}

static void reset_chart(void) {
  for (int i = 0; i < CHART_HISTORY; i++) { chart_r[i] = 0; chart_g[i] = 0; chart_b[i] = 0; }
  chart_index = 0; chart_max_value = 65535;
}

void tcs3472_chart_activity(void) {
  uint32_t lu = HAL_GetTick();
  uint8_t la8 = 0, ld0 = 0;
  bool led_state = false;
  uint32_t llt = 0;
  reset_chart();
  boardTCS3472.ledOff();
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    keyManager.collision_A8.tick(); keyManager.collision_D0.tick();

    uint8_t ca = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (ca && !la8 && (HAL_GetTick() - llt > 300)) {
      led_state = !led_state;
      if (led_state) boardTCS3472.ledOn(); else boardTCS3472.ledOff();
      llt = HAL_GetTick();
    }
    la8 = ca;

    uint8_t cd = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (cd && !ld0) reset_chart();
    ld0 = cd;

    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();
    update_chart_data(raw.red, raw.green, raw.blue);

    if (HAL_GetTick() - lu > 50) {
      lu = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      bar("09");

      int chart_x = 10, chart_y = 44, chart_w = 220, chart_h = 90;
      draw_chart_axes(chart_x, chart_y, chart_w, chart_h, chart_max_value);
      draw_chart_all(chart_x, chart_y, chart_w, chart_h, chart_max_value);

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LV_ERROR); PD_DrawRect(10, 140, 10, 8);
      PD_SetColor(LV_TEXT_PRIMARY); PD_DrawString(23, 139, "R");
      PD_SetColor(LV_SUCCESS); PD_DrawRect(50, 140, 10, 8);
      PD_SetColor(LV_TEXT_PRIMARY); PD_DrawString(63, 139, "G");
      PD_SetColor(LV_PRIMARY); PD_DrawRect(90, 140, 10, 8);
      PD_SetColor(LV_TEXT_PRIMARY); PD_DrawString(103, 139, "B");

      if (led_state) {
        PD_SetColor(LV_WARNING); PD_SetFill(true);
        PD_DrawRoundRect(150, 137, 30, 14, 3); PD_SetFill(false);
        PD_SetColor(LV_TEXT_PRIMARY); PD_DrawString(154, 138, "LED");
      }

      char dbg[48]; char fstr[16];
      PD_SetColor(LV_TEXT_PRIMARY);
      snprintf(dbg, sizeof(dbg), "R:%-5u G:%-5u B:%-5u", raw.red, raw.green, raw.blue);
      PD_DrawString(10, 158, dbg);

      PD_SetColor(LV_ACCENT);
      float_to_str(color.color_temp, fstr);
      snprintf(dbg, sizeof(dbg), "CCT:%s K  Lux:", fstr);
      PD_DrawString(10, 178, dbg);

      bbar("EXIT", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(30);
  }
  boardTCS3472.ledOff();
}

void tcs3472_read_activity(void) {
  char fstr[16]; char dbg[64];
  if (!boardTCS3472.isInitialized()) {
    boardTCS3472.init();
    if (!boardTCS3472.isInitialized()) {
      PD_FillScreen(LV_BG_DARK);
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LV_ERROR);
      PD_DrawString(20, 60, "TCS3472 Init Failed!");
      PD_DrawString(20, 85, "Check I2C connection");
      bbar("EXIT", NULL, NULL);
      LCD_Flush();
      while (1) { keyManager.btn_enter.tick(); if (keyManager.btn_enter.getState() == KEY_PRESSED) break; HAL_Delay(50); }
      return;
    }
  }

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();

    PD_FillScreen(LV_BG_DARK);
    bar("09");

    PD_DrawAngledCard(8, 44, 224, 100, 6, TOS_CARD_BG);
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(LV_ERROR);
    sprintf(dbg, "R: %4u", raw.red); PD_DrawString(16, 52, dbg);
    PD_SetColor(LV_SUCCESS);
    sprintf(dbg, "G: %4u", raw.green); PD_DrawString(16, 70, dbg);
    PD_SetColor(LV_PRIMARY);
    sprintf(dbg, "B: %4u", raw.blue); PD_DrawString(16, 88, dbg);
    PD_SetColor(LV_TEXT_PRIMARY);
    sprintf(dbg, "C: %4u", raw.clear); PD_DrawString(16, 106, dbg);

    PD_SetColor(LV_ACCENT);
    float_to_str(color.color_temp, fstr); sprintf(dbg, "CCT: %s K", fstr); PD_DrawString(16, 128, dbg);
    float_to_str(color.lux, fstr); sprintf(dbg, "Lux: %s lx", fstr); PD_DrawString(16, 146, dbg);

    show_color_block(raw.red, raw.green, raw.blue);

    bbar("EXIT", NULL, NULL);
    LCD_Flush();
    HAL_Delay(100);
  }
}

void tcs3472_color_demo(void) {
  if (!boardTCS3472.isInitialized()) boardTCS3472.init();
  boardTCS3472.ledOn();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();

    PD_FillScreen(LV_BG_DARK);
    uint32_t display_color = ((raw.red >> 8) << 16) | ((raw.green >> 8) << 8) | (raw.blue >> 8);
    PD_SetColor(display_color); PD_SetFill(true);
    PD_DrawRect(0, 40, 240, 170); PD_SetFill(false);

    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(LV_TEXT_PRIMARY);
    char dbg[32];
    sprintf(dbg, "R:%-5u G:%-5u B:%-5u", raw.red, raw.green, raw.blue);
    PD_DrawString(8, 8, dbg);

    char fstr[16];
    float_to_str(color.color_temp, fstr);
    sprintf(dbg, "CCT: %s K", fstr);
    PD_SetColor(LV_ACCENT);
    PD_DrawString(8, 28, dbg);

    bbar("EXIT", NULL, NULL);
    LCD_Flush();

    LOG_D("TACT", "RGB: %u,%u,%u | CCT: %.0f K | Lux: %.1f", raw.red, raw.green, raw.blue, color.color_temp, color.lux);
    HAL_Delay(200);
  }
  boardTCS3472.ledOff();
}

void tcs3472_cct_demo(void) {
  if (!boardTCS3472.isInitialized()) boardTCS3472.init();

  const uint32_t samples = 10;
  float cct_sum = 0, lux_sum = 0;
  uint16_t r_sum = 0, g_sum = 0, b_sum = 0;

  for (uint32_t i = 0; i < samples; i++) {
    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();
    r_sum += raw.red; g_sum += raw.green; b_sum += raw.blue;
    cct_sum += color.color_temp; lux_sum += color.lux;
    HAL_Delay(50);
  }

  r_sum /= samples; g_sum /= samples; b_sum /= samples;
  cct_sum /= samples; lux_sum /= samples;

  PD_FillScreen(LV_BG_DARK);
  bar("09");

  PD_DrawAngledCard(8, 44, 224, 122, 6, TOS_CARD_BG);
  PD_SetFont(FONT_ASCII_16);
  char dbg[64]; char fstr[16];

  sprintf(dbg, "Avg R: %4u", r_sum); PD_SetColor(LV_ERROR); PD_DrawString(20, 52, dbg);
  sprintf(dbg, "Avg G: %4u", g_sum); PD_SetColor(LV_SUCCESS); PD_DrawString(20, 72, dbg);
  sprintf(dbg, "Avg B: %4u", b_sum); PD_SetColor(LV_PRIMARY); PD_DrawString(20, 92, dbg);

  PD_SetColor(LV_TEXT_PRIMARY);
  float_to_str(cct_sum, fstr); sprintf(dbg, "CCT: %s K", fstr);
  PD_DrawString(20, 120, dbg);
  float_to_str(lux_sum, fstr); sprintf(dbg, "Lux: %s lx", fstr);
  PD_DrawString(20, 140, dbg);

  PD_SetColor(LV_ACCENT);
  if (cct_sum < 3000) PD_DrawString(20, 175, "Warm White");
  else if (cct_sum < 4500) PD_DrawString(20, 175, "Neutral White");
  else if (cct_sum < 5500) PD_DrawString(20, 175, "Daylight");
  else if (cct_sum < 7000) PD_DrawString(20, 175, "Cool White");
  else PD_DrawString(20, 175, "Overcast/Cool");

  bbar("EXIT", NULL, NULL);
  LCD_Flush();

  while (1) { keyManager.btn_enter.tick(); if (keyManager.btn_enter.getState() == KEY_PRESSED) break; HAL_Delay(50); }
}

void tcs3472_activity(void) {
  uint8_t le = 0;
  uint32_t lu = HAL_GetTick();

  PD_Init(); menu_select = 0;
  boardTCS3472.init();
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= TCS3472_MENU_ITEMS) menu_select = TCS3472_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0) menu_select--;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (menu_select) {
      case 0: tcs3472_read_activity(); break;
      case 1: tcs3472_color_demo(); break;
      case 2: tcs3472_cct_demo(); break;
      case 3: tcs3472_chart_activity(); break;
      case 4: return;
      }
      PD_FillScreen(LV_BG_DARK);
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      bar("09");
      menu_cards(menu_select);
      bbar("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
