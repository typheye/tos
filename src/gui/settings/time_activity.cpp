/**
 ******************************************************************************
 * @file    time_activity.cpp
 * @author  Typheye
 * @brief   Time Activity implementation.
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

#include "include/time_activity.hpp"
#include "components/include/alert.hpp"
#include "components/include/keyboard.hpp"
#include "core/manager/include/settings_manager.h"
#include "core/sys/include/systime.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "syslog.h"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;
extern TRTC boardTRTC;

static bool auto_sync = true;
static bool style_24h = true;
static char date_buf[16] = "2026-01-01";
static char time_buf[16] = "00:00:00";
static bool editing = false;
static int  edit_sel = -1;

/* ==================================================================
 *  Draw
 * ================================================================== */

static void draw_frame(const char *title) {
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
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text, bool grey) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(grey ? TOS_GREY : (s ? TOS_TEXT : TOS_TEXT_SEC));
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        const char *value, bool editing_this, bool grey) {
  bool s = (idx == sel);
  uint32_t cc = s ? TOS_ACCENT : TOS_CARD_BG;
  if (editing_this && (HAL_GetTick() / 200) % 2) cc = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, cc);
  PD_SetColor(grey ? TOS_GREY : (s ? TOS_TEXT : TOS_TEXT_SEC));
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

/* ==================================================================
 *  Sync (direct draw, no alert component during wait)
 * ================================================================== */

static bool do_sync(void) {
  /* If ESP8266 is hard-disabled, skip the sync attempt */
  if (ESP8266_IsHardDisabled()) {
    alert_show("TIME", "ESP8266 is disable!");
    return false;
  }

  /* Show "Syncing..." — alert-style (frame + left-aligned msg) */
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  LCD_FLUSH({
    PD_Init(); PD_FillScreen(TOS_BG);
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
    HAL_Delay(1000);
    ok = SysTime_Sync();
  }

  boardLCD.fillScreen(LCD_COLOR_BLACK);
  if (ok) alert_show("TIME", "Synced OK!");
  else    alert_show("TIME", "Sync failed");
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  return ok;
}

/* ==================================================================
 *  Main
 * ================================================================== */

void time_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  auto_sync = SM_Time_AutoSync();
  style_24h = SM_Time_Style24h();
  editing = false; edit_sel = -1;

  Time_t t; Date_t d;
  boardTRTC.getDateTime(&t, &d);
  snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
           2000 + d.year, d.month, d.date);
  /* Time item always 24H */
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
           t.hours, t.minutes, t.seconds);

  int sel = 0;
  uint8_t le = 0; uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (editing) {
      if (keyManager.collision_A8.getState() == KEY_PRESSED ||
          keyManager.collision_D0.getState() == KEY_PRESSED) {
        if (edit_sel == 1)  { auto_sync = !auto_sync; SM_Time_SetAutoSync(auto_sync); }
        if (edit_sel == 5)  { style_24h = !style_24h; SM_Time_SetStyle24h(style_24h); }
        HAL_Delay(150);
      }
    } else {
      if (keyManager.collision_A8.getState() == KEY_PRESSED)
      { sel = (sel + 1) % 6; HAL_Delay(100); }
      if (keyManager.collision_D0.getState() == KEY_PRESSED)
      { sel = (sel - 1 + 6) % 6; HAL_Delay(100); }
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (editing) {
        editing = false; edit_sel = -1;
      } else {
        switch (sel) {
        case 0: return;
        case 1: editing = true; edit_sel = 1; break;
        case 2: if (auto_sync) { do_sync(); } break;
        case 3:
          if (!auto_sync) {
            if (keyboard_open("Date yyyy-mm-dd", date_buf, 10)) {
              int y, mo, da;
              if (sscanf(date_buf, "%d-%d-%d", &y, &mo, &da) == 3) {
                int wk = (da + (mo <= 2 ? 1 : 0)) % 7; if (!wk) wk = 7;
                boardTRTC.setDate((uint8_t)(y - 2000), (uint8_t)mo, (uint8_t)da, (uint8_t)wk);
              }
            }
            boardLCD.fillScreen(LCD_COLOR_BLACK);
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
            boardLCD.fillScreen(LCD_COLOR_BLACK);
          }
          break;
        case 5: editing = true; edit_sel = 5; break;
        }
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();
      boardTRTC.getDateTime(&t, &d);
      snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
               2000 + d.year, d.month, d.date);
      /* Time item always 24H */
      snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
               t.hours, t.minutes, t.seconds);

      bool sync_on = auto_sync;
      LCD_FLUSH({
        draw_frame("TIME");
        PD_SetFont(FONT_ASCII_16);
        for (int i = 0; i < 6; i++) {
          int cy = 33 + i * 25;
          switch (i) {
          case 0: draw_card(0, sel, cy, "00 Return", false); break;
          case 1: { char b[32]; snprintf(b,sizeof(b),"01 Auto Sync");
            draw_card_r(1,sel,cy,b,auto_sync?"ON":"OFF",editing&&edit_sel==1,false); break; }
          case 2: draw_card(2,sel,cy,"   Sync Now",!sync_on); break;
          case 3: { char b[32]; snprintf(b,sizeof(b),"   Date");
            draw_card_r(3,sel,cy,b,date_buf,false,sync_on); break; }
          case 4: { char b[32]; snprintf(b,sizeof(b),"   Time");
            draw_card_r(4,sel,cy,b,time_buf,false,sync_on); break; }
          case 5: { char b[32]; snprintf(b,sizeof(b),"02 Style");
            draw_card_r(5,sel,cy,b,style_24h?"24H":"12H",editing&&edit_sel==5,false); break; }
          }
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(10);
  }
}
