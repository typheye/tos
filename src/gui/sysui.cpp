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
#include "hardware/include/bmp180.hpp"
#include "hardware/include/trtc.hpp"
#include "include/esp8266_activity.hpp"
#include "include/jy901s.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"

extern LCD boardLCD;
extern BMP180 boardBMP180;
extern JY901S boardJY901S;
extern TRTC boardTRTC;

int SysUI::now_activity = UI_DASHBOARD;
uint32_t SysUI::last_tick = 0;
void (*SysUI::current_test_func)(void) = nullptr;
int SysUI::cpu_usage = 0;

#define MENUS_COUNT 12
#define VISIBLE_ITEMS 6

static const char *menus[MENUS_COUNT] = {
    "00 Back to Home",   "01 Key Test",       "02 SD Card Test",
    "03 I2C Scan",       "04 JY901S Sensor",  "05 BMP180 Sensor",
    "06 Display Tests",  "07 3D Path Tracer", "08 ESP8266 Test",
    "09 TCS3472 Test",   "10 SN74HC00N Test", "11 Pot Test",
};

static void (*test_functions[MENUS_COUNT])(void) = {
    NULL, // Back to Home
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

  if (now_activity == UI_DASHBOARD) {
    handleDashboardInput();
    drawDashboard();
  } else if (now_activity == UI_LAUNCHER) {
    handleLauncherInput();
    drawLauncher();
  } else if (now_activity == UI_RUNNING_TEST) {
    runCurrentTest();
  }
}

// ===================== Dashboard =====================

void SysUI::handleDashboardInput(void) {
  keyManager.btn_enter.tick();
  if (keyManager.btn_enter.getState() == KEY_PRESSED) {
    now_activity = UI_LAUNCHER;
    menus_select = 0;
    HAL_Delay(200);
  }
}

void SysUI::drawDashboard(void) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  PD_DrawFrame();

  Time_t now;
  Date_t today;
  boardTRTC.getDateTime(&now, &today);

  // Title
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, "TOS");

  // == 品字形居中布局 ==
  // Inner frame: y=22..213 (191px). Content: 70+16+75=161.
  // Padding=(191-161)/2=15.
  int top_y = 37;
  int bot_y = 123;

  // Top time card (like reference _draw_time_card)
  int tc_x = 14, tc_w = 212, tc_h = 70;
  PD_DrawAngledCard(tc_x, top_y, tc_w, tc_h, 6, TOS_CARD_BG);

  char time_str[16];
  sprintf(time_str, "%02d:%02d", now.hours, now.minutes);
  PD_SetFont(FONT_ASCII_32);
  PD_SetColor(TOS_TEXT);
  PD_DrawStringCentered(tc_x, top_y + 4, tc_w, 38, time_str);

  char date_str[32];
  sprintf(date_str, "%04d / %02d / %02d", 2000 + today.year, today.month, today.date);
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_TEXT_SEC);
  PD_DrawStringCentered(tc_x, top_y + 42, tc_w, 22, date_str);

  // Bottom status cards (like reference _draw_status_cards)
  int card_w = 96, card_h = 75;
  int gap = 20;
  int card1_x = 14;
  int card2_x = card1_x + card_w + gap;

  // Read temperature (throttled)
  static float cached_temp = 0;
  static bool has_temp = false;
  static uint32_t last_temp_read = 0;
  if (HAL_GetTick() - last_temp_read > 500) {
    last_temp_read = HAL_GetTick();
    if (boardBMP180.isInitialized()) {
      BMP180_Data_t d = boardBMP180.readData(BMP180_MODE_STD);
      cached_temp = d.temperature;
      has_temp = true;
    }
  }

  // Left card: Temperature
  PD_DrawAngledCard(card1_x, bot_y, card_w, card_h, 5, TOS_CARD_BG);

  char val_str[16];
  if (has_temp) {
    int ti = (int)(cached_temp + 0.5f);
    int td = (int)((cached_temp - ti) * 10);
    if (td < 0)
      td = -td;
    sprintf(val_str, "%d.%d", ti, td);
  } else {
    sprintf(val_str, "--");
  }
  PD_SetFont(FONT_ASCII_32);
  PD_SetColor(TOS_ACCENT);
  PD_DrawStringCentered(card1_x, bot_y + 6, card_w, 36, val_str);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_TEXT);
  PD_DrawString(card1_x + 14, bot_y + 50, "Temp");

  PD_SetColor(TOS_GREY);
  PD_DrawString(card1_x + 56, bot_y + 50, "C");

  // Right card: CPU usage
  PD_DrawAngledCard(card2_x, bot_y, card_w, card_h, 5, TOS_CARD_BG);

  char cpu_str[16];
  sprintf(cpu_str, "%d", cpu_usage);
  PD_SetFont(FONT_ASCII_32);
  PD_SetColor(TOS_ACCENT);
  PD_DrawStringCentered(card2_x, bot_y + 6, card_w, 36, cpu_str);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_TEXT);
  PD_DrawString(card2_x + 14, bot_y + 50, "CPU");

  PD_SetColor(TOS_GREY);
  PD_DrawString(card2_x + 50, bot_y + 50, "%");

  PD_DrawFooterCenter("MENU", NULL, NULL);

  LCD_Flush();
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
      // Back to Home
      now_activity = UI_DASHBOARD;
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
