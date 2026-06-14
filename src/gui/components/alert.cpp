/**
 ******************************************************************************
 * @file    alert.cpp
 * @author  Typheye
 * @brief   Alert implementation.
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

#include "include/alert.hpp"
#include "library/include/libdly.h"


extern KeyManager keyManager;
extern LCD boardLCD;
extern TRTC boardTRTC;

void alert_show(const char *title, const char *msg) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  uint32_t lu = 0;

  while (1) {
    SysWatchdog_Tick();
    TosApi_Tick();
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) return;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();

      LCD_FLUSH({
        /* Update header time */
        Time_t now; Date_t today;
        boardTRTC.getDateTime(&now, &today);
        char ts[8]; time_fmt(ts, sizeof(ts), now.hours, now.minutes);
        PD_SetHeaderTime(ts);

        PD_Init();
        PD_FillScreen(TOS_BG);
        PD_DrawFrame();

        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_ACCENT);
        PD_DrawString(22, 5, title);

        // Message â€?split on \n, draw each line
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_TEXT);
        int msg_y = 33;
        const char *p = msg;
        while (*p && msg_y < 200) {
          /* Find end of this segment (\n or \0) */
          const char *eol = p;
          while (*eol && *eol != '\n') eol++;
          while (*p == ' ') p++;

          while (p < eol && msg_y < 200) {
            int len = (int)(eol - p);
            if (len > 22) {
              /* Word-wrap: find last space before the 22-char limit */
              int brk = 22;
              while (brk > 0 && p[brk] != ' ') brk--;
              if (brk == 0) brk = 22;
              len = brk;
            }
            char line[24];
            memcpy(line, p, len); line[len] = '\0';
            PD_DrawString(16, msg_y, line);
            msg_y += 20;
            p += len;
            while (*p == ' ') p++;
          }

          if (*p == '\n') p++;
        }

        PD_DrawFooterCenter("ENTER", NULL, NULL);
      });
    }
    SysWatchdog_Tick();
    JPDelay(1);
  }
}
