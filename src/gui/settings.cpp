#include "include/settings.hpp"
#include "demo/include/demo_activity.hpp"
#include "settings/wlan_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

#define SET_N 10
static const char *set_m[SET_N] = {
  "00 Return", "01 WLAN", "02 Bluetooth", "03 Storage", "04 Display",
  "05 Sound",  "06 Apps", "07 About",     "08 Restore", "09 Debugs",
};
#define DBG_N 2
static const char *dbg_m[DBG_N] = {"00 Return", "01 Run Demo"};

static void draw_menu(const char *title, const char **items, int count, int sel) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  PD_DrawFrame();

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);

  int visible = count < 7 ? count : 7;
  int start = sel - visible / 2;
  if (start < 0) start = 0;
  if (start + visible > count) start = count - visible;

  for (int i = 0; i < visible; i++) {
    int idx = start + i;
    if (idx >= count) break;
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
  LCD_Flush();
}

static int menu_loop(const char *title, const char **items, int count) {
  int sel = 0; uint8_t le = 0; uint32_t lu = 0;
  while (1) {
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED) { sel = (sel + 1) % count; HAL_Delay(150); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) { sel = (sel - 1 + count) % count; HAL_Delay(150); }
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) { le = ce; return sel; }
    le = ce;
    if (HAL_GetTick() - lu > 100) { lu = HAL_GetTick(); draw_menu(title, items, count, sel); }
    HAL_Delay(20);
  }
}

void settings_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  while (1) {
    int sel = menu_loop("SET", set_m, SET_N);
    if (sel == 0) return;
    if (sel == 1) { wlan_activity_run(); boardLCD.fillScreen(LCD_COLOR_BLACK); }
    if (sel == 9) {
      boardLCD.fillScreen(LCD_COLOR_BLACK);
      while (1) { int ds = menu_loop("DEBUG", dbg_m, DBG_N);
        if (ds == 0) break;
        if (ds == 1) { demo_list_run(); boardLCD.fillScreen(LCD_COLOR_BLACK); }
      }
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
  }
}
