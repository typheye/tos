/**
 ******************************************************************************
 * @file    key_activity.cpp
 * @author  Typheye
 * @brief   Key Activity implementation.
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

#include "include/key_activity.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern LCD boardLCD;

/* Standard template functions (exact copy from about-page) */
#define KEY_N 14
#define KEY_VIS 7

void key_test_activity(void) {
  const char *key_labels[KEY_N] = {
      "00 Return", "01 SW1",  "   SW2",  "   SW3",  "   SW4",
      "02 SW5",    "   SW6",  "   SW7",  "   SW8",  "   SW9",
      "03 SW10",   "   SW11", "   SW12", "   SW13",
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % KEY_N;
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + KEY_N) % KEY_N;
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le)
      return;
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();

      /* Read all states */
      bool s1 = keyManager.sw1_E0.isOn();
      bool s2 = keyManager.sw2_G13.isOn();
      bool s3 = keyManager.sw3_E2.isOn();
      bool s4 = keyManager.sw4_E4.isOn();
      bool s5 = keyManager.sw5_D6.isOn();
      bool s6 = keyManager.sw6_G9.isOn();
      bool s7 = keyManager.sw7_G11.isOn();
      bool s8 = keyManager.sw8_G10.isOn();
      bool s9 = keyManager.sw9_G15.isOn();
      bool s10 = keyManager.sw10_G3.isOn();
      bool s11 = keyManager.sw11_D15.isOn();
      bool s12 = keyManager.sw12_B12.isOn();
      bool s13 = keyManager.sw13_B14.isOn();

      /* Build value strings & colors ON=TOS_TEXT, OFF=TOS_TEXT_SEC */
      char key_vals[KEY_N][8];
      key_vals[0][0] = '\0';

      // Group 1: SW1-SW4 (normal logic)
      snprintf(key_vals[1], 8, "%s", s1 ? "ON" : "OFF");
      snprintf(key_vals[2], 8, "%s", s2 ? "ON" : "OFF");
      snprintf(key_vals[3], 8, "%s", s3 ? "ON" : "OFF");
      snprintf(key_vals[4], 8, "%s", s4 ? "ON" : "OFF");

      // Group 2: SW5-SW9 (inverted logic true=OFF, false=ON)
      snprintf(key_vals[5], 8, "%s", s5 ? "ON" : "OFF");
      snprintf(key_vals[6], 8, "%s", s6 ? "ON" : "OFF");
      snprintf(key_vals[7], 8, "%s", s7 ? "ON" : "OFF");
      snprintf(key_vals[8], 8, "%s", s8 ? "ON" : "OFF");
      snprintf(key_vals[9], 8, "%s", s9 ? "ON" : "OFF");

      // Group 3: SW10-SW13 (normal logic)
      snprintf(key_vals[10], 8, "%s", s10 ? "ON" : "OFF");
      snprintf(key_vals[11], 8, "%s", s11 ? "ON" : "OFF");
      snprintf(key_vals[12], 8, "%s", s12 ? "ON" : "OFF");
      snprintf(key_vals[13], 8, "%s", s13 ? "ON" : "OFF");

      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");
        PD_SetFont(FONT_ASCII_16);

        int vis = KEY_VIS;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > KEY_N)
          start = KEY_N - vis;

        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= KEY_N)
            break;
          int cy = 33 + i * 25;
          if (idx == 0)
            UI_DrawMenuCard(idx, sel, cy, key_labels[idx]);
          else
            UI_DrawMenuValue(idx, sel, cy, key_labels[idx], key_vals[idx],
                             false);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}
