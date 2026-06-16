/**
 ******************************************************************************
 * @file    demo_activity.cpp
 * @author  Typheye
 * @brief   Demo Activity implementation.
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

#include "include/demo_activity.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"


extern KeyManager keyManager;
extern LCD boardLCD;

#define DEMO_ITEMS 8
static const char *demo_m[DEMO_ITEMS] = {
    "00 Return",        "01 Key Test",       "03 I2C Scan",
    "04 JY901S Sensor", "05 BMP180 Sensor",  "07 3D Path Tracer",
    "09 TCS3472 Test",  "10 SN74HC00N Test",
};
static void (*demo_f[DEMO_ITEMS])(void) = {
    NULL,
    key_test_activity,
    i2c_scan_activity,
    jyro_activity,
    bmp180_activity,
    render_3dox_activity_with_exit,
    tcs3472_activity,
    hc00n_activity,
};

// ============ Original TOS-style menu ============

void demo_list_run(void) {
  static int sel = 0;
  while (1) {
    sel = UI_MenuLoop("DEMO", demo_m, DEMO_ITEMS, sel);
    if (sel == 0)
      return;
    if (demo_f[sel]) {
      demo_f[sel]();
    }
  }
}
