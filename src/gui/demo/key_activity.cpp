/**
 ******************************************************************************
 * @file    key_activity.cpp
 * @author  Typheye
 * @brief   Key Activity implementation.
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
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (keyManager._collisionA8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % KEY_N;
      JPDelay(45);
    }
    if (keyManager._collisionD0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + KEY_N) % KEY_N;
      JPDelay(45);
    }

    uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (ce && !le)
      return;
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();

      /* Read all states */
      bool s1 = keyManager._sw1E0.isOn();
      bool s2 = keyManager._sw2G13.isOn();
      bool s3 = keyManager._sw3E2.isOn();
      bool s4 = keyManager._sw4E4.isOn();
      bool s5 = keyManager._sw5D6.isOn();
      bool s6 = keyManager._sw6G9.isOn();
      bool s7 = keyManager._sw7G11.isOn();
      bool s8 = keyManager._sw8G10.isOn();
      bool s9 = keyManager._sw9G15.isOn();
      bool s10 = keyManager._sw10G3.isOn();
      bool s11 = keyManager._sw11D15.isOn();
      bool s12 = keyManager._sw12B12.isOn();
      bool s13 = keyManager._sw13B14.isOn();

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
