#include "include/sysui.hpp"
#include "demo/include/demo_activity.hpp"
#include "hardware/include/trtc.hpp"
#include "include/launcher.hpp"
#include "include/libpd.h"
#include "syslog.h"

extern LCD boardLCD;
extern TRTC boardTRTC;

int SysUI::now_activity = UI_DASHBOARD;
uint32_t SysUI::last_tick = 0;
void (*SysUI::current_test_func)(void) = nullptr;
int SysUI::cpu_usage = 0;

void SysUI::init(void) { last_tick = HAL_GetTick(); }

void SysUI::loop(void) {
  Time_t now; Date_t today;
  boardTRTC.getDateTime(&now, &today);
  char time_str[8];
  sprintf(time_str, "%02d:%02d", now.hours, now.minutes);
  PD_SetHeaderTime(time_str);

  if (now_activity == UI_LAUNCHER) {
    if (demo_activity_run()) now_activity = UI_PET;
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  } else if (now_activity == UI_RUNNING_TEST) {
    runCurrentTest();
  } else if (now_activity == UI_PET) {
    pet_launcher_run();
    now_activity = UI_LAUNCHER;
  }
}

void SysUI::runCurrentTest(void) {
  if (current_test_func) { boardLCD.fillScreen(LCD_COLOR_BLACK); current_test_func(); }
  now_activity = UI_LAUNCHER; current_test_func = nullptr;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}

void SysUI::setActivity(int a) { now_activity = a; boardLCD.fillScreen(LCD_COLOR_BLACK); }
int  SysUI::getActivity(void)  { return now_activity; }
void SysUI::setCurrentTest(void (*f)(void)) { current_test_func = f; }
void SysUI::resetMenuPosition(void) {}
void SysUI::updateCpuUsage(uint32_t w) { cpu_usage = (w * 100) / (w + 10); if (cpu_usage > 100) cpu_usage = 100; }
