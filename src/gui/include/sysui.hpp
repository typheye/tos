#ifndef SYSUI_HPP
#define SYSUI_HPP

#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "library/include/libpd.h"
#include "main.h"
#include <cstdio>
#include <cstring>

// 界面ID定义
#define UI_LAUNCHER 0
#define UI_RUNNING_TEST 1

class SysUI {
public:
  static void init(void);
  static void loop(void);
  static void setActivity(int activity);
  static int getActivity(void);
  static void setCurrentTest(void (*test_func)(void));
  static void resetMenuPosition(void); // 新增：重置菜单位置

private:
  static int now_activity;
  static uint32_t last_tick;
  static void (*current_test_func)(void);

  static void drawLauncher(void);
  static void handleInput(void);
  static void runCurrentTest(void);
};

#endif