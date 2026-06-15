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


extern KeyManager keyManager;
extern LCD boardLCD;

static void draw_frame_title(const char *title) {
  PD_Init(); PD_FillScreen(TOS_BG);
  extern TRTC boardTRTC;
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 1000) {
    last_tm = HAL_GetTick();
    Time_t t; Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8]; time_fmt(ts, sizeof(ts), t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        bool on, bool edit) {
  bool s = (idx == sel);
  uint32_t card_c = s ? TOS_ACCENT : TOS_CARD_BG;
  if (edit && s && ((HAL_GetTick() / 300U) & 1U)) {
    card_c = TOS_CARD_BG;
  }
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);
  const char *val = on ? "ON" : "OFF";
  uint16_t vw = PD_GetStringWidth(val);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(220 - vw, cy + 2, val);
}

void sound_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
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
        draw_frame_title("SOUND");
        PD_SetFont(FONT_ASCII_16);
        draw_card(0, sel, 33, "00 Return");
        draw_card_r(1, sel, 58, "01 Mute", !muted, false);
        draw_card_r(2, sel, 83, "02 Boot GFX", boot_gfx, edit && sel == 2);
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}
