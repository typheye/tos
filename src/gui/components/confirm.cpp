/**
 ******************************************************************************
 * @file    confirm.cpp
 * @author  Typheye
 * @brief   Confirm implementation.
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

#include "include/confirm.hpp"
#include "library/include/libdly.h"


extern KeyManager keyManager;
extern LCD boardLCD;

bool confirm_show(const char *title, const char *msg) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int sel = 0; /* 0=Yes, 1=No */
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    SysWatchdog_Tick();
    TosApi_Tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 2;
      JPDelay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 2) % 2;
      JPDelay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      le = ce;
      return (sel == 0);
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        PD_Init();
        PD_FillScreen(TOS_BG);

        /* Update header time */
        extern TRTC boardTRTC;
        Time_t t;
        Date_t d;
        boardTRTC.getDateTime(&t, &d);
        char ts[8];
        time_fmt(ts, sizeof(ts), t.hours, t.minutes);
        PD_SetHeaderTime(ts);

        PD_DrawFrame();
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_ACCENT);
        PD_DrawString(22, 5, title);

        /* Message */
        PD_SetColor(TOS_TEXT);
        PD_DrawString(16, 33, msg);

        /* Yes / No at positions 6 and 7 (closer to bottom) */
        int y0 = 33 + 5 * 25; /* position 5: y=158 */
        int y1 = 33 + 6 * 25; /* position 6: y=183 */

        PD_DrawAngledCard(14, y0, 212, 20, 5,
                          sel == 0 ? TOS_ACCENT : TOS_CARD_BG);
        PD_SetColor(sel == 0 ? TOS_TEXT : TOS_TEXT_SEC);
        PD_DrawString(26, y0 + 2, "01 Yes, Confirm");

        PD_DrawAngledCard(14, y1, 212, 20, 5,
                          sel == 1 ? TOS_ACCENT : TOS_CARD_BG);
        PD_SetColor(sel == 1 ? TOS_TEXT : TOS_TEXT_SEC);
        PD_DrawString(26, y1 + 2, "02 No, Cancel");

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    SysWatchdog_Tick();
    JPDelay(1);
  }
}
