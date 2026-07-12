/**
 ******************************************************************************
 * @file    settings.cpp
 * @author  Typheye
 * @brief   Settings application implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
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
