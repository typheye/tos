/**
 ******************************************************************************
 * @file    sound_activity.cpp
 * @author  Typheye
 * @brief   Sound Activity implementation.
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
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (edit && sel == 2) {
        boot_gfx = !boot_gfx;
      } else {
        sel = (sel + 1) % 3;
      }
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (edit && sel == 2) {
        boot_gfx = !boot_gfx;
      } else {
        sel = (sel - 1 + 3) % 3;
      }
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
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
