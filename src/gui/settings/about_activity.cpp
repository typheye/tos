/**
 ******************************************************************************
 * @file    about_activity.cpp
 * @author  Typheye
 * @brief   About Activity implementation.
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

#include "include/about_activity.hpp"
#include "components/include/alert.hpp"
#include "components/include/confirm.hpp"
#include "core/include/config.h"
#include "core/manager/include/settings_manager.h"
#include "core/sys/include/systime.h"
#include "core/sdk/include/tos_api.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/sfhd.h"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

#define AM_N 12

static void draw_frame_title(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  extern TRTC boardTRTC;
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 1000) {
    last_tm = HAL_GetTick();
    Time_t t;
    Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8];
    time_fmt(ts, sizeof(ts), t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        const char *value) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

static void sysinfo_page(uint32_t boot_tick) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  uint32_t lu = 0;
  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      return;
    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();
      uint32_t el = (HAL_GetTick() - boot_tick) / 1000;
      LCD_FLUSH({
        draw_frame_title("SysInfo");
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_TEXT);
        char buf[48];
        snprintf(buf, sizeof(buf), "Elapsed Time:");
        PD_DrawString(16, 33, buf);
        snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", (unsigned long)(el / 3600),
                 (unsigned long)((el / 60) % 60), (unsigned long)(el % 60));
        PD_DrawString(16, 58, buf);
        PD_DrawFooterCenter("ENTER", NULL, NULL);
      });
    }
    HAL_Delay(1);
  }
}

void about_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  uint32_t boot_tick = HAL_GetTick();

  struct {
    const char *l, *v;
  } items[AM_N] = {
      {"00 Return", ""},
      {"01 Model", CFG_MODEL},
      {"   MCU", CFG_MCU},
      {"   RAM", CFG_RAM},
      {"   ROM", CFG_ROM},
      {"02 TOS Version", CFG_TOS_VERSION},
      {"   Build", CFG_BUILD},
      {"   Patch", CFG_PATCH},
      {"03 System Information", ""},
      {"   Update System", ""},
      {"   Reboot Device", ""},
      {"04 Restore to Default", ""},
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % AM_N;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + AM_N) % AM_N;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 8: /* System Information */
        sysinfo_page(boot_tick);
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 9: /* Update System */ {
        /* Loading screen */
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        LCD_FLUSH({
          draw_frame_title("UPD");
          PD_SetColor(TOS_TEXT);
          PD_DrawString(26, 33, "Checking...");
        });

        TosUpgradeInfo info;
        if (TosApi_CheckUpgrade(&info)) {
          if (info.has_update) {
            char msg[200];
            const char *ver = info.latest_version[0] ? info.latest_version : "-";
            const char *build = info.latest_build[0] ? info.latest_build : "-";
            const char *patch = info.latest_patch[0] ? info.latest_patch : "-";
            snprintf(msg, sizeof(msg),
                     "New version available!\n\n"
                     "Version: %s\nBuild: %s\nPatch: %s\nSize: %d B\n\n"
                     "Download from PC.",
                     ver, build, patch, info.latest_size);
            alert_show("UPD", msg);
          } else {
            alert_show("UPD", "Already up to date!");
          }
        } else {
          alert_show("UPD", "Check failed.\nCheck WiFi connection.");
        }
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      }
      case 10: /* Reboot Device */
        if (confirm_show("REB", "Reboot the device now?")) {
          NVIC_SystemReset();
        }
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 11: /* Restore to Default */
        if (confirm_show("RST", "Erase all settings?\nDevice will reboot.")) {
          /* Loading screen */
          boardLCD.fillScreen(LCD_COLOR_BLACK);
          LCD_FLUSH({
            draw_frame_title("RST");
            PD_SetColor(TOS_TEXT);
            PD_DrawString(26, 33, "Resetting...");
          });
          HAL_Delay(2000);
          /* Erase flash sector and reboot */
          Flash_Erase_Sector();
          NVIC_SystemReset();
        }
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        draw_frame_title("ABOUT");
        PD_SetFont(FONT_ASCII_16);

        int vis = AM_N < 7 ? AM_N : 7;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > AM_N)
          start = AM_N - vis;

        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= AM_N)
            break;
          int cy = 33 + i * 25;
          if (items[idx].v[0])
            draw_card_r(idx, sel, cy, items[idx].l, items[idx].v);
          else
            draw_card(idx, sel, cy, items[idx].l);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}
