/**
 ******************************************************************************
 * @file    display_activity.cpp
 * @author  Typheye
 * @brief   Display Activity implementation.
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

#include "include/display_activity.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern LCD boardLCD;

static bool disp_auto = false;
static int disp_bright = 10; // 1-10
static int disp_dir = 0;
static bool disp_edit = false;
static int edit_field = 0;
static bool disp_inited = false; // only apply defaults first time       //
                                 // 1=auto, 2=bright, 3=direction

static const char *dir_names[] = {"0", "90"};

static void apply(void) {
  boardLCD.setBrightness((uint16_t)disp_bright * 100);
  boardLCD.setRotation((uint8_t)disp_dir);
}

static int item_count(void) {
  return 5; // Return + Auto + Brightness + Direction + Resolution
}

static void draw_disp(int sel) {
  LCD_FLUSH({
    UI_DrawFrameTitle("DISP");
    PD_SetFont(FONT_ASCII_16);

    int n = item_count();
    int vis = n < 7 ? n : 7;
    int start = UI_MenuVisibleStart(n, sel, vis);

    for (int i = 0; i < vis; i++) {
      int idx = start + i;
      if (idx >= n)
        break;
      int cy = 33 + i * 25;

      switch (idx) {
      case 0:
        UI_DrawMenuCard(idx, sel, cy, "00 Return");
        break;
      case 1: {
        char buf[32];
        snprintf(buf, sizeof(buf), "01 Auto");
        UI_DrawMenuValue(idx, sel, cy, buf, disp_auto ? "ON" : "OFF",
                         disp_edit && edit_field == 1);
        break;
      }
      case 2: {
        char buf[32];
        snprintf(buf, sizeof(buf), "   Brightness");
        if (disp_auto) {
          UI_DrawMenuCardEx(idx, sel, cy, buf, true);
        } else {
          char val[8];
          snprintf(val, sizeof(val), "%d%%", disp_bright * 10);
          UI_DrawMenuValue(idx, sel, cy, buf, val,
                           disp_edit && edit_field == 2);
        }
        break;
      }
      case 3: {
        char buf[32];
        snprintf(buf, sizeof(buf), "02 Direction");
        UI_DrawMenuValue(idx, sel, cy, buf, dir_names[disp_dir],
                         disp_edit && edit_field == 3);
        break;
      }
      case 4:
        UI_DrawMenuCard(idx, sel, cy, "03 Resolution  240x240");
        break;
      }
    }
    UI_DrawStdFooter();
  });
}

// ============ Main ============

void display_activity_run(void) {
  if (!disp_inited) {
    disp_auto = SM_Disp_Auto();
    disp_bright = SM_Disp_Bright();
    disp_dir = SM_Disp_Dir();
    disp_inited = true;
  }
  /* Always re-apply on entry */
  boardLCD.setAutoBrightness(disp_auto);
  boardLCD.setRotation((uint8_t)disp_dir);
  if (!disp_auto)
    boardLCD.setBrightness((uint16_t)disp_bright * 100);
  disp_edit = false;
  edit_field = 0;

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (disp_edit) {
        if (edit_field == 1)
          disp_auto = !disp_auto;
        else if (edit_field == 2) {
          disp_bright++;
          if (disp_bright > 10)
            disp_bright = 10;
          apply(); /* live preview */
        } else if (edit_field == 3) {
          disp_dir = (disp_dir + 1) % 2;
          apply();
        }
      } else {
        sel = (sel + 1) % item_count();
      }
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (disp_edit) {
        if (edit_field == 1)
          disp_auto = !disp_auto;
        else if (edit_field == 2) {
          disp_bright--;
          if (disp_bright < 1)
            disp_bright = 1;
          apply(); /* live preview */
        } else if (edit_field == 3) {
          disp_dir = (disp_dir - 1 + 2) % 2;
          apply();
        }
      } else {
        sel = (sel - 1 + item_count()) % item_count();
      }
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (disp_edit) {
        disp_edit = false;
        edit_field = 0;
        Settings_t *s = SM_Get();
        s->disp_auto = disp_auto;
        s->disp_bright = disp_bright;
        s->disp_dir = disp_dir;
        SM_Save(); /* single save after all changes */
        boardLCD.setAutoBrightness(disp_auto);
        boardLCD.setRotation((uint8_t)disp_dir);
        if (!disp_auto)
          boardLCD.setBrightness((uint16_t)disp_bright * 100);
      } else {
        if (sel == 0)
          return;
        if (sel == 1) {
          disp_edit = true;
          edit_field = 1;
        }
        if (sel == 2) {
          if (disp_auto)
            alert_show("ALERT", "Please disable Auto first");
          else {
            disp_edit = true;
            edit_field = 2;
          }
        }
        if (sel == 3) {
          disp_edit = true;
          edit_field = 3;
        }
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      draw_disp(sel);
    }
    JPDelay(1);
  }
}
