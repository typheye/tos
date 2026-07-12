/**
 ******************************************************************************
 * @file    alert.cpp
 * @author  Typheye
 * @brief   Alert implementation.
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

#include "include/alert.hpp"
#include "library/include/libdly.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern TRTC boardTRTC;

void alert_show(const char *title, const char *msg) {
  uint32_t lu = 0;

  while (1) {
    SysWatchdog_Tick();
    TosApi_Tick();
    keyManager._btnEnter.tick();
    if (keyManager._btnEnter.getState() == KEY_PRESSED)
      return;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();

      LCD_FLUSH({
        /* Update header time */
        Time_t now;
        Date_t today;
        boardTRTC.getDateTime(&now, &today);
        char ts[8];
        SysTime_Fmt(ts, sizeof(ts), now.hours, now.minutes);
        PD_SetHeaderTime(ts);

        PD_Init();
        PD_FillScreen(TOS_BG);
        PD_DrawFrame();

        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_ACCENT);
        PD_DrawString(22, 5, title);

        // Message split on \n, draw each line
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_TEXT);
        int msg_y = 33;
        const char *p = msg;
        while (*p && msg_y < 200) {
          /* Find end of this segment (\n or \0) */
          const char *eol = p;
          while (*eol && *eol != '\n')
            eol++;
          while (*p == ' ')
            p++;

          while (p < eol && msg_y < 200) {
            int len = (int)(eol - p);
            if (len > 22) {
              /* Word-wrap: find last space before the 22-char limit */
              int brk = 22;
              while (brk > 0 && p[brk] != ' ')
                brk--;
              if (brk == 0)
                brk = 22;
              len = brk;
            }
            char line[24];
            memcpy(line, p, len);
            line[len] = '\0';
            PD_DrawString(16, msg_y, line);
            msg_y += 20;
            p += len;
            while (*p == ' ')
              p++;
          }

          if (*p == '\n')
            p++;
        }

        PD_DrawFooterCenter("ENTER", NULL, NULL);
      });
    }
    SysWatchdog_Tick();
    JPDelay(1);
  }
}
