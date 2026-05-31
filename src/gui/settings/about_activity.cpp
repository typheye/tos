/**
 * @file    about_activity.cpp
 * @brief   About page — scrollable menu cards, ENTER=exit
 */

#include "include/about_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "core/include/systime.h"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

#define ABT_N 9

static void draw_frame_title(const char *title) {
  PD_Init(); PD_FillScreen(TOS_BG);
  extern TRTC boardTRTC;
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 1000) {
    last_tm = HAL_GetTick();
    Time_t t; Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8]; time_fmt(ts, sizeof(ts), t.hours, t.minutes);
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

static void draw_card_r(int idx, int sel, int cy, const char *label, const char *value) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

struct AbtItem { const char *l, *v; };

void about_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  static const AbtItem items[ABT_N] = {
    {"00 Return", ""},
    {"01 Model", "TOS-CNAEK7"},
    {"   MCU",   "STM32F407"},
    {"   RAM",   "192K"},
    {"   ROM",   "1024K"},
    {"02 TOS Version", "1"},
    {"   Build", "1.26.5.r1"},
    {"   Patch", "2026-05-01"},
    {"03 Hardware", "V1"},
  };

  int sel = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED)
    { sel = (sel + 1) % ABT_N; HAL_Delay(100); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED)
    { sel = (sel - 1 + ABT_N) % ABT_N; HAL_Delay(100); }
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce) { if (sel == 0) return; }

    if (HAL_GetTick() - lu > 100) { lu = HAL_GetTick();
      draw_frame_title("ABOUT");
      PD_SetFont(FONT_ASCII_16);

      int vis = ABT_N < 7 ? ABT_N : 7;
      int start = sel - vis / 2;
      if (start < 0) start = 0;
      if (start + vis > ABT_N) start = ABT_N - vis;

      for (int i = 0; i < vis; i++) {
        int idx = start + i; if (idx >= ABT_N) break;
        int cy = 33 + i * 25;
        if (items[idx].v[0])
          draw_card_r(idx, sel, cy, items[idx].l, items[idx].v);
        else
          draw_card(idx, sel, cy, items[idx].l);
      }
      PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
