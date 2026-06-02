#include "include/settings.hpp"
#include "core/sdk/include/tos_api.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "settings/include/hotspot_activity.hpp"
#include "settings/include/wlan_activity.hpp"
#include "settings/include/storage_activity.hpp"
#include "settings/include/display_activity.hpp"
#include "settings/include/sound_activity.hpp"
#include "settings/include/time_activity.hpp"
#include "settings/include/about_activity.hpp"
#include <cstdio>
#include "syslog.h"
#include "core/sys/include/systime.h"

extern KeyManager keyManager;
extern LCD boardLCD;

#define SET_N 8
static const char *set_m[SET_N] = {
    "00 Return", "01 WLAN", "02 Hotspot", "03 Storage", "04 Display",
    "05 Sound & GFX", "06 Date & Time", "07 About & More",
};

static void draw_menu(const char *title, const char **items, int count,
                      int sel) {
  LCD_FLUSH({
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

    int visible = count < 7 ? count : 7;
    int start = sel - visible / 2;
    if (start < 0)
      start = 0;
    if (start + visible > count)
      start = count - visible;

    for (int i = 0; i < visible; i++) {
      int idx = start + i;
      if (idx >= count)
        break;
      int cy = 33 + i * 25;
      if (idx == sel) {
        PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
        PD_SetColor(TOS_TEXT);
      } else {
        PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
        PD_SetColor(TOS_TEXT_SEC);
      }
      PD_DrawString(26, cy + 2, items[idx]);
    }
    PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  });
}

static int menu_loop(const char *title, const char **items, int count,
                     int start_sel) {
  int sel = start_sel;
  if (sel >= count)
    sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;
  while (1) {
    TosApi_Tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % count;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + count) % count;
      HAL_Delay(150);
    }
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      le = ce;
      return sel;
    }
    le = ce;
    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_menu(title, items, count, sel);
    }
    TosApi_Tick();
    HAL_Delay(1);
  }
}

void settings_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  static int sel = 0;
  while (1) {
    sel = menu_loop("SET", set_m, SET_N, sel);
    if (sel == 0)
      return;
    if (sel == 1) {
      wlan_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (sel == 2) {
      hotspot_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (sel == 3) {
      storage_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (sel == 4) {
      display_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (sel == 5) {
      sound_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (sel == 6) {
      time_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (sel == 7) {
      about_activity_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
  }
}
