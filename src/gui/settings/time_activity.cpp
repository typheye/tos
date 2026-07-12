/**
 ******************************************************************************
 * @file    time_activity.cpp
 * @author  Typheye
 * @brief   Time Activity implementation.
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

#include "include/time_activity.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern TRTC boardTRTC;

static bool auto_sync = true;
static bool style_24h = true;
static char date_buf[16] = "2026-01-01";
static char time_buf[16] = "00:00:00";
static bool editing = false;
static int edit_sel = -1;

/* ==================================================================
 *  Draw
 * ================================================================== */

/* ==================================================================
 *  Sync (direct draw, no alert component during wait)
 * ================================================================== */

static bool do_sync(void) {
  /* If ESP8266 is hard-disabled, skip the sync attempt */
  if (ESP8266_IsHardDisabled()) {
    alert_show("TIME", "ESP8266 is disable!");
    return false;
  }

  /* Show "Syncing..." alert-style (frame + left-aligned msg) */
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);
    PD_DrawFrame();
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, "TIME");
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, 33, "Syncing...");
  });

  uint32_t start = HAL_GetTick();
  bool ok = SysTime_Sync();
  uint32_t elapsed = HAL_GetTick() - start;

  if (!ok && elapsed < 15000) {
    /* Retry once after short delay */
    JPDelay(1000);
    ok = SysTime_Sync();
  }
  if (ok)
    alert_show("TIME", "Synced OK!");
  else
    alert_show("TIME", "Sync failed");
  return ok;
}

/* ==================================================================
 *  Main
 * ================================================================== */

void time_activity_run(void) {

  auto_sync = SM_Time_AutoSync();
  style_24h = SM_Time_Style24h();
  editing = false;
  edit_sel = -1;

  Time_t t;
  Date_t d;
  boardTRTC.getDateTime(&t, &d);
  snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", 2000 + d.year, d.month,
           d.date);
  /* Time item always 24H */
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", t.hours, t.minutes,
           t.seconds);

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (editing) {
      if (keyManager._collisionA8.getState() == KEY_PRESSED ||
          keyManager._collisionD0.getState() == KEY_PRESSED) {
        if (edit_sel == 1) {
          auto_sync = !auto_sync;
          SM_Time_SetAutoSync(auto_sync);
        }
        if (edit_sel == 5) {
          style_24h = !style_24h;
          SM_Time_SetStyle24h(style_24h);
        }
        JPDelay(45);
      }
    } else {
      if (keyManager._collisionA8.getState() == KEY_PRESSED) {
        sel = (sel + 1) % 6;
        JPDelay(45);
      }
      if (keyManager._collisionD0.getState() == KEY_PRESSED) {
        sel = (sel - 1 + 6) % 6;
        JPDelay(45);
      }
    }

    uint8_t ce = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (editing) {
        editing = false;
        edit_sel = -1;
      } else {
        switch (sel) {
        case 0:
          return;
        case 1:
          editing = true;
          edit_sel = 1;
          break;
        case 2:
          if (auto_sync) {
            do_sync();
          }
          break;
        case 3:
          if (!auto_sync) {
            if (keyboard_open("Date yyyy-mm-dd", date_buf, 10)) {
              int y, mo, da;
              if (sscanf(date_buf, "%d-%d-%d", &y, &mo, &da) == 3) {
                int wk = (da + (mo <= 2 ? 1 : 0)) % 7;
                if (!wk)
                  wk = 7;
                boardTRTC.setDate((uint8_t)(y - 2000), (uint8_t)mo, (uint8_t)da,
                                  (uint8_t)wk);
              }
            }
          }
          break;
        case 4:
          if (!auto_sync) {
            if (keyboard_open("Time hh:mm:ss", time_buf, 8)) {
              int h, m, s;
              if (sscanf(time_buf, "%d:%d:%d", &h, &m, &s) == 3) {
                boardTRTC.setTime((uint8_t)h, (uint8_t)m, (uint8_t)s);
              }
            }
          }
          break;
        case 5:
          editing = true;
          edit_sel = 5;
          break;
        }
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      boardTRTC.getDateTime(&t, &d);
      snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", 2000 + d.year,
               d.month, d.date);
      /* Time item always 24H */
      snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", t.hours, t.minutes,
               t.seconds);

      bool sync_on = auto_sync;
      LCD_FLUSH({
        UI_DrawFrameTitle("TIME");
        PD_SetFont(FONT_ASCII_16);
        for (int i = 0; i < 6; i++) {
          int cy = 33 + i * 25;
          switch (i) {
          case 0:
            UI_DrawMenuCardEx(0, sel, cy, "00 Return", false);
            break;
          case 1: {
            char b[32];
            snprintf(b, sizeof(b), "01 Auto Sync");
            UI_DrawMenuValueEx(1, sel, cy, b, auto_sync ? "ON" : "OFF",
                               editing && edit_sel == 1, false);
            break;
          }
          case 2:
            UI_DrawMenuCardEx(2, sel, cy, "   Sync Now", !sync_on);
            break;
          case 3: {
            char b[32];
            snprintf(b, sizeof(b), "   Date");
            UI_DrawMenuValueEx(3, sel, cy, b, date_buf, false, sync_on);
            break;
          }
          case 4: {
            char b[32];
            snprintf(b, sizeof(b), "   Time");
            UI_DrawMenuValueEx(4, sel, cy, b, time_buf, false, sync_on);
            break;
          }
          case 5: {
            char b[32];
            snprintf(b, sizeof(b), "02 Style");
            UI_DrawMenuValueEx(5, sel, cy, b, style_24h ? "24H" : "12H",
                               editing && edit_sel == 5, false);
            break;
          }
          }
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(10);
  }
}
