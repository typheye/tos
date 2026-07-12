/**
 ******************************************************************************
 * @file    sound_activity.cpp
 * @author  Typheye
 * @brief   Sound Activity implementation.
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

#include "include/sound_activity.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"


extern KeyManager keyManager;
extern LCD boardLCD;

void sound_activity_run(void) {
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;
  bool edit = false;
  bool boot_gfx = SM_BootGfx();

  while (1) {
    keyManager._collisionA8.tick(); keyManager._collisionD0.tick(); keyManager._btnEnter.tick();
    if (keyManager._collisionA8.getState() == KEY_PRESSED) {
      if (edit && sel == 2) {
        boot_gfx = !boot_gfx;
      } else {
        sel = (sel + 1) % 3;
      }
      JPDelay(45);
    }
    if (keyManager._collisionD0.getState() == KEY_PRESSED) {
      if (edit && sel == 2) {
        boot_gfx = !boot_gfx;
      } else {
        sel = (sel - 1 + 3) % 3;
      }
      JPDelay(45);
    }

    uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (edit) {
        if (sel == 2) {
          SM_SetBootGfx(boot_gfx);
        }
        edit = false;
      } else if (sel == 0) {
        return;
      } else if (sel == 2) {
        boot_gfx = SM_BootGfx();
        edit = true;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      bool muted = keyManager.isMuted();
      LCD_FLUSH({
        UI_DrawFrameTitle("SOUND");
        PD_SetFont(FONT_ASCII_16);
        UI_DrawMenuCard(0, sel, 33, "00 Return");
        UI_DrawMenuBool(1, sel, 58, "01 Mute", !muted, false);
        UI_DrawMenuBool(2, sel, 83, "02 Boot GFX", boot_gfx, edit && sel == 2);
        UI_DrawStdFooter();
      });
    }
    JPDelay(1);
  }
}
