#include "include/sysui.hpp"
#include "demo/include/3dox_activity.hpp"
#include "demo/include/bmp_activity.hpp"
#include "demo/include/display_activity.hpp"
#include "demo/include/i2c_activity.hpp"
#include "demo/include/jyro_activity.hpp"
#include "demo/include/key_activity.hpp"
#include "demo/include/sd_activity.hpp"
#include "hardware/include/trtc.hpp"
#include "include/esp8266_activity.hpp"
#include "include/jy901s.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"

extern LCD boardLCD;
extern JY901S boardJY901S;
extern TRTC boardTRTC;

int SysUI::now_activity = UI_LAUNCHER;
uint32_t SysUI::last_tick = 0;
void (*SysUI::current_test_func)(void) = nullptr;

// 菜单项定义 - 每页5项
#define MENU_PAGE_ITEM 5
#define MENUS_COUNT 8

static const char *menus[MENUS_COUNT] = {
    "01 Key Test",       "02 SD Card Test",  "03 I2C Scan",
    "04 JY901S Sensor",  "05 BMP180 Sensor", "06 Display Tests",
    "07 3D Path Tracer", "08 ESP8266 Test"};

// 对应的测试函数
static void (*test_functions[MENUS_COUNT])(void) = {
    key_test_activity,              // 0: Key Test
    sd_card_activity,               // 1: SD Card Test
    i2c_scan_activity,              // 2: I2C Scan
    gyro_cube_activity_with_exit,   // 3: JY901S Sensor
    bmp180_gui_activity,            // 4: BMP180 Sensor
    display_test_menu_activity,     // 5: Display Tests
    render_3dox_activity_with_exit, // 7: 3D Path Tracer
    esp8266_test_activity,          // 8: ESP8266 Test
};

// 保存菜单状态
static int saved_menus_select = 0;
static int saved_menus_page_now = 0;

static int menus_select = 0;
static int menus_page = (MENUS_COUNT % MENU_PAGE_ITEM == 0)
                            ? MENUS_COUNT / MENU_PAGE_ITEM
                            : MENUS_COUNT / MENU_PAGE_ITEM + 1;
static int menus_page_now = 0;

void SysUI::init(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  last_tick = HAL_GetTick();

  menus_select = saved_menus_select;
  menus_page_now = saved_menus_page_now;
}

void SysUI::loop(void) {
  if (now_activity == UI_LAUNCHER) {
    handleInput();
    drawLauncher();
  } else if (now_activity == UI_RUNNING_TEST) {
    runCurrentTest();
  }
}

void SysUI::handleInput(void) {
  keyManager.collision_A8.tick();
  keyManager.collision_D0.tick();
  keyManager.btn_enter.tick();

  // 碰撞开关 A8 - 向下选择菜单
  if (keyManager.collision_A8.getState() == KEY_PRESSED) {
    if (now_activity == UI_LAUNCHER) {
      int select = menus_select % MENU_PAGE_ITEM;
      if (select < MENU_PAGE_ITEM - 1 && menus_select < MENUS_COUNT - 1) {
        menus_select++;
      } else if (menus_page_now < menus_page - 1) {
        menus_page_now++;
        menus_select++;
      } else {
        menus_page_now = 0;
        menus_select = 0;
      }
      saved_menus_select = menus_select;
      saved_menus_page_now = menus_page_now;
    }
    HAL_Delay(150);
  }

  // 碰撞开关 D0 - 向上选择菜单
  if (keyManager.collision_D0.getState() == KEY_PRESSED) {
    if (now_activity == UI_LAUNCHER) {
      int select = menus_select % MENU_PAGE_ITEM;
      if (select > 0) {
        menus_select--;
      } else if (menus_page_now > 0) {
        menus_page_now--;
        menus_select--;
      } else {
        menus_page_now = menus_page - 1;
        menus_select = MENUS_COUNT - 1;
      }
      saved_menus_select = menus_select;
      saved_menus_page_now = menus_page_now;
    }
    HAL_Delay(150);
  }

  // Enter键 - 确认/进入
  if (keyManager.btn_enter.getState() == KEY_PRESSED) {
    if (now_activity == UI_LAUNCHER) {
      if (menus_select < MENUS_COUNT &&
          test_functions[menus_select] != nullptr) {
        current_test_func = test_functions[menus_select];
        now_activity = UI_RUNNING_TEST;
        boardLCD.fillScreen(LCD_COLOR_BLACK);
      }
    }
    HAL_Delay(150);
  }
}

void SysUI::drawLauncher(void) {
  int select = menus_select % MENU_PAGE_ITEM;
  char time_str[32];
  char date_str[32];

  PD_Init();
  PD_FillScreen(LCD_COLOR_BLACK);

  // 顶部状态栏
  Time_t now;
  Date_t today;
  boardTRTC.getDateTime(&now, &today);

  sprintf(time_str, "%02d:%02d:%02d", now.hours, now.minutes, now.seconds);
  sprintf(date_str, "%02d-%02d", today.month, today.date);

  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "TOS");

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(60, 6, date_str);

  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(170, 6, time_str);

  // 菜单区域 - 适配5个菜单项
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 165); // 高度增加到165以容纳5项

  PD_SetFont(FONT_ASCII_12);
  for (int i = 0; i < MENU_PAGE_ITEM; i++) {
    int index = i + menus_page_now * MENU_PAGE_ITEM;
    if (index >= MENUS_COUNT)
      break;

    int y = 42 + i * 28;

    if (i == select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 3, 210, 24);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }

    PD_DrawString(20, y, menus[index]);
  }

  // 底部提示栏
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 215, 240, 25);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(10, 219, "A8:Down");
  PD_DrawString(75, 219, "D0:Up");
  PD_DrawString(130, 219, "Enter:OK");

  char page_str[16];
  sprintf(page_str, "%d/%d", menus_page_now + 1, menus_page);
  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(200, 219, page_str);

  LCD_Flush();
}

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
  if (activity == UI_LAUNCHER) {
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  } else {
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  }
}

int SysUI::getActivity(void) { return now_activity; }

void SysUI::setCurrentTest(void (*test_func)(void)) {
  current_test_func = test_func;
}

void SysUI::resetMenuPosition(void) {
  menus_select = 0;
  menus_page_now = 0;
  saved_menus_select = 0;
  saved_menus_page_now = 0;
}