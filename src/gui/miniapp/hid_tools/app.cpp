/**
 ******************************************************************************
 * @file    app.cpp
 * @author  Typheye
 * @brief   App implementation.
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

#include "include/app.h"
#include "library/include/libdly.h"
#include "library/include/libui.h"


extern KeyManager keyManager;
extern LCD boardLCD;
extern TRTC boardTRTC;

#define HID_TOOLS_MENU_ITEMS 6
static const char *hid_tools_menu[HID_TOOLS_MENU_ITEMS] = {
    "00 Return",
    "01 USB Status",
    "02 Vendor IO",
    "03 Quick Keys",
    "04 Mouse Test",
    "05 Gyro Mouse",
};

typedef void (*HidPageFn)(void);
static HidPageFn hid_tools_pages[HID_TOOLS_MENU_ITEMS] = {
    nullptr,
    hid_tools_status_page,
    hid_tools_vendor_page,
    hid_tools_quickkeys_page,
    hid_tools_mouse_page,
    hid_tools_gyro_mouse_page,
};

void hid_tools_run(void) {
  static int sel = 0;

  while (1) {
    sel = UI_MenuLoop("HID", hid_tools_menu, HID_TOOLS_MENU_ITEMS, sel);
    if (sel == 0) return;
    if (hid_tools_pages[sel] != nullptr) {
      hid_tools_pages[sel]();
    }
  }
}
