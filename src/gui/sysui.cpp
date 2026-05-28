#include "include/sysui.hpp"
#include "demo/include/3dox_activity.hpp"
#include "demo/include/bmp_activity.hpp"
#include "demo/include/display_activity.hpp"
#include "demo/include/i2c_activity.hpp"
#include "demo/include/jyro_activity.hpp"
#include "demo/include/key_activity.hpp"
#include "demo/include/pot_activity.hpp"
#include "demo/include/sd_activity.hpp"
#include "demo/include/sn74hc00n_activity.hpp"
#include "demo/include/tcs3472_activity.hpp"
#include "hardware/include/trtc.hpp"
#include "include/esp8266_activity.hpp"
#include "include/jy901s.hpp"
#include "include/launcher.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"

extern LCD boardLCD;
extern TRTC boardTRTC;

int SysUI::now_activity = UI_DASHBOARD;
uint32_t SysUI::last_tick = 0;
void (*SysUI::current_test_func)(void) = nullptr;
int SysUI::cpu_usage = 0;

#define MENUS_COUNT 12
#define VISIBLE_ITEMS 6

static const char *menus[MENUS_COUNT] = {
    "00 Return Launcher", "01 Key Test",       "02 SD Card Test",
    "03 I2C Scan",        "04 JY901S Sensor",  "05 BMP180 Sensor",
    "06 Display Tests",   "07 3D Path Tracer", "08 ESP8266 Test",
    "09 TCS3472 Test",    "10 SN74HC00N Test", "11 Pot Test",
};

static void (*test_functions[MENUS_COUNT])(void) = {
    NULL, // Return Launcher → handled specially
    key_test_activity,
    sd_card_activity,
    i2c_scan_activity,
    jyro_activity,
    bmp180_activity,
    display_test_menu_activity,
    render_3dox_activity_with_exit,
    esp8266_test_activity,
    tcs3472_activity,
    hc00n_activity,
    pot_activity,
};

static int menus_select = 0;

void SysUI::init(void) {
  last_tick = HAL_GetTick();
  menus_select = 0;
}

void SysUI::loop(void) {
  // Update shared header time for all pages
  Time_t now;
  Date_t today;
  boardTRTC.getDateTime(&now, &today);
  char time_str[6];
  sprintf(time_str, "%02d:%02d", now.hours, now.minutes);
  PD_SetHeaderTime(time_str);

  if (now_activity == UI_LAUNCHER) {
    handleLauncherInput();
    drawLauncher();
  } else if (now_activity == UI_RUNNING_TEST) {
    runCurrentTest();
  } else if (now_activity == UI_PET) {
    pet_launcher_run();
    now_activity = UI_LAUNCHER;
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  }
}

// ===================== Menu Launcher =====================

void SysUI::handleLauncherInput(void) {
  keyManager.collision_A8.tick();
  keyManager.collision_D0.tick();
  keyManager.btn_enter.tick();

  if (keyManager.collision_A8.getState() == KEY_PRESSED) {
    menus_select = (menus_select + 1) % MENUS_COUNT;
    HAL_Delay(150);
  }

  if (keyManager.collision_D0.getState() == KEY_PRESSED) {
    menus_select = (menus_select - 1 + MENUS_COUNT) % MENUS_COUNT;
    HAL_Delay(150);
  }

  if (keyManager.btn_enter.getState() == KEY_PRESSED) {
    if (menus_select == 0) {
      // Return to pet launcher
      now_activity = UI_PET;
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    } else if (test_functions[menus_select] != nullptr) {
      current_test_func = test_functions[menus_select];
      now_activity = UI_RUNNING_TEST;
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    HAL_Delay(150);
  }
}

void SysUI::drawLauncher(void) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  PD_DrawFrame();

  // Title
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, "Menu");

  // Menu cards — sliding window
  int visible = 7;
  int start_idx = menus_select - visible / 2;
  if (start_idx < 0)
    start_idx = 0;
  if (start_idx + visible > MENUS_COUNT)
    start_idx = MENUS_COUNT - visible;

  int card_x = 14;
  int card_w = 212;
  int card_h = 20;
  int card_r = 5;
  int card_gap = 25;

  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < visible; i++) {
    int index = start_idx + i;
    if (index >= MENUS_COUNT)
      break;

    int card_y = 28 + i * card_gap;

    if (index == menus_select) {
      PD_DrawAngledCard(card_x, card_y, card_w, card_h, card_r, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(card_x, card_y, card_w, card_h, card_r, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }

    PD_DrawString(card_x + 12, card_y + 2, menus[index]);
  }

  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");

  LCD_Flush();
}

// ===================== Test runner =====================

void SysUI::runCurrentTest(void) {
  if (current_test_func != nullptr) {
    boardLCD.fillScreen(LCD_COLOR_BLACK);
    current_test_func();
  }
  now_activity = UI_LAUNCHER;
  current_test_func = nullptr;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}

void SysUI::setActivity(int activity) {
  now_activity = activity;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}

int SysUI::getActivity(void) { return now_activity; }

void SysUI::setCurrentTest(void (*test_func)(void)) {
  current_test_func = test_func;
}

void SysUI::resetMenuPosition(void) { menus_select = 0; }

void SysUI::updateCpuUsage(uint32_t work_ms) {
  cpu_usage = (work_ms * 100) / (work_ms + 10);
  if (cpu_usage > 100)
    cpu_usage = 100;
}
