#include "include/sysui.hpp"
#include "core/include/systime.h"
#include "gui/demo/include/demo_activity.hpp"
#include "gui/miniapp/hid_tools/include/app.h"
#include "gui/include/settings.hpp"
#include "gui/miniapp/file_manager/include/app.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/launcher.hpp"
#include "include/libpd.h"
#include "syslog.h"

extern LCD boardLCD;
extern TRTC boardTRTC;
extern KeyManager keyManager;

int SysUI::now_activity = UI_DASHBOARD;
uint32_t SysUI::last_tick = 0;
void (*SysUI::current_test_func)(void) = nullptr;
int SysUI::cpu_usage = 0;

#define TOS_ITEMS 5
static const char *tos_m[TOS_ITEMS] = {"00 Return", "01 Settings",
                                       "02 File Manager", "03 HID Tools",
                                       "04 Demo Activities"};

static void draw_menu(const char *title, const char **items, int count,
                      int sel) {
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);
    extern TRTC boardTRTC;
    static uint32_t lt = 0;
    if (HAL_GetTick() - lt > 1000) {
      lt = HAL_GetTick();
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
    int v = count < 7 ? count : 7;
    int st = sel - v / 2;
    if (st < 0)
      st = 0;
    if (st + v > count)
      st = count - v;
    for (int i = 0; i < v; i++) {
      int idx = st + i;
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
    HAL_Delay(1);
  }
}

void SysUI::init(void) { last_tick = HAL_GetTick(); }

void SysUI::loop(void) {
  Time_t now;
  Date_t today;
  boardTRTC.getDateTime(&now, &today);
  char time_str[8];
  time_fmt(time_str, sizeof(time_str), now.hours, now.minutes);
  PD_SetHeaderTime(time_str);

  if (now_activity == UI_LAUNCHER) {
    static int sel = 0;
    sel = menu_loop("TOS", tos_m, TOS_ITEMS, sel);
    if (sel == 0) {
      now_activity = UI_PET;
    } else if (sel == 1) {
      settings_run();
    } else if (sel == 2) {
      file_manager_run();
    } else if (sel == 3) {
      hid_tools_run();
    } else if (sel == 4) {
      demo_list_run();
    }
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  } else if (now_activity == UI_RUNNING_TEST) {
    runCurrentTest();
  } else if (now_activity == UI_PET) {
    pet_launcher_run();
    now_activity = UI_LAUNCHER;
  }
}

void SysUI::runCurrentTest(void) {
  if (current_test_func) {
    boardLCD.fillScreen(LCD_COLOR_BLACK);
    current_test_func();
  }
  now_activity = UI_LAUNCHER;
  current_test_func = nullptr;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}

void SysUI::setActivity(int a) {
  now_activity = a;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}
int SysUI::getActivity(void) { return now_activity; }
void SysUI::setCurrentTest(void (*f)(void)) { current_test_func = f; }
void SysUI::resetMenuPosition(void) {}
void SysUI::updateCpuUsage(uint32_t w) {
  cpu_usage = (w * 100) / (w + 10);
  if (cpu_usage > 100)
    cpu_usage = 100;
}
