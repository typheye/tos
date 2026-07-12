/**
 ******************************************************************************
 * @file    confirm.cpp
 * @author  Typheye
 * @brief   Confirm implementation.
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

#include "include/confirm.hpp"
#include "library/include/libdly.h"


extern KeyManager keyManager;
extern LCD boardLCD;

static void draw_wrapped_message(int16_t x, int16_t y, int16_t max_w,
                                 int16_t max_y, const char *msg) {
  if (!msg) {
    return;
  }

  uint16_t step = PD_GetCharWidth() + 1U;
  uint16_t line_h = PD_GetCharHeight() + 3U;
  uint16_t max_chars = step > 0U ? (uint16_t)(max_w / step) : 1U;
  if (max_chars < 1U) {
    max_chars = 1U;
  }
  if (max_chars > 63U) {
    max_chars = 63U;
  }

  const char *p = msg;
  while (*p && y <= max_y) {
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      y += line_h;
      p++;
      continue;
    }

    while (*p == ' ' || *p == '\t') {
      p++;
    }

    const char *eol = p;
    while (*eol && *eol != '\n' && *eol != '\r') {
      eol++;
    }

    while (p < eol && y <= max_y) {
      uint16_t remain = (uint16_t)(eol - p);
      uint16_t len = remain > max_chars ? max_chars : remain;

      if (remain > max_chars) {
        uint16_t brk = len;
        while (brk > 0U && p[brk] != ' ' && p[brk] != '\t') {
          brk--;
        }
        if (brk > 0U) {
          len = brk;
        }
      }

      while (len > 0U && (p[len - 1U] == ' ' || p[len - 1U] == '\t')) {
        len--;
      }
      if (len == 0U) {
        len = 1U;
      }

      char line[64];
      memcpy(line, p, len);
      line[len] = '\0';
      PD_DrawString(x, y, line);
      y += line_h;
      p += len;

      while (*p == ' ' || *p == '\t') {
        p++;
      }
    }
  }
}

bool confirm_show(const char *title, const char *msg) {
  int sel = 0; /* 0=Yes, 1=No */
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    SysWatchdog_Tick();
    TosApi_Tick();
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (keyManager._collisionA8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 2;
      JPDelay(45);
    }
    if (keyManager._collisionD0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 2) % 2;
      JPDelay(45);
    }

    uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (ce && !le) {
      le = ce;
      return (sel == 0);
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
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
        SysTime_Fmt(ts, sizeof(ts), t.hours, t.minutes);
        PD_SetHeaderTime(ts);

        PD_DrawFrame();
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_ACCENT);
        PD_DrawString(22, 5, title);

        /* Message */
        PD_SetColor(TOS_TEXT);
        draw_wrapped_message(16, 33, 208, 145, msg);

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
