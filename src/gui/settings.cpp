/**
 ******************************************************************************
 * @file    settings.cpp
 * @author  Typheye
 * @brief   Settings application implementation.
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

#include "include/settings.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"


extern KeyManager keyManager;
extern LCD boardLCD;

#define SET_N 8
static const char *set_m[SET_N] = {
    "00 Return", "01 WLAN", "02 Hotspot", "03 Storage", "04 Display",
    "05 Sound & GFX", "06 Date & Time", "07 About & More",
};

void settings_run(void) {
  static int sel = 0;
  while (1) {
    sel = UI_MenuLoop("SET", set_m, SET_N, sel);
    if (sel == 0)
      return;
    if (sel == 1) {
      wlan_activity_run();
    }
    if (sel == 2) {
      hotspot_activity_run();
    }
    if (sel == 3) {
      storage_activity_run();
    }
    if (sel == 4) {
      display_activity_run();
    }
    if (sel == 5) {
      sound_activity_run();
    }
    if (sel == 6) {
      time_activity_run();
    }
    if (sel == 7) {
      about_activity_run();
    }
  }
}
