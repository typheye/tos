/**
 ******************************************************************************
 * @file    sysui.hpp
 * @author  Typheye
 * @brief   Sysui interface.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef SYSUI_HPP
#define SYSUI_HPP

#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "library/include/libpd.h"
#include "main.h"
#include <cstdio>
#include <cstring>

#define UI_DASHBOARD    0
#define UI_LAUNCHER     1
#define UI_RUNNING_TEST 2
#define UI_PET          3

class SysUI {
public:
  static void init(void);
  static void loop(void);
  static void setActivity(int activity);
  static int getActivity(void);
  static void setCurrentTest(void (*test_func)(void));
  static void resetMenuPosition(void);
  static void updateCpuUsage(uint32_t work_ms);

private:
  static int now_activity;
  static uint32_t last_tick;
  static void (*current_test_func)(void);
  static int cpu_usage;

  static void drawLauncher(void);
  static void handleLauncherInput(void);
  static void runCurrentTest(void);
};

#endif
