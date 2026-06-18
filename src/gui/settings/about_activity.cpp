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
#include "hardware/include/sfhd.h"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern "C" void SysUI_DebugOverlaySetEnabled(uint8_t enabled);

#define AM_N 12
#define BUILD_DEBUG_CLICKS 5U
#define BUILD_DEBUG_WINDOW_MS 1400U

static void format_uptime(char *buf, size_t len) {
  uint32_t sec = HAL_GetTick() / 1000U;
  snprintf(buf, len, "%lu:%02lu:%02lu", (unsigned long)(sec / 3600U),
           (unsigned long)((sec / 60U) % 60U), (unsigned long)(sec % 60U));
}

static void debug_page(void) {
  uint32_t wait_start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - wait_start) < 600U) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() != KEY_PRESSED) {
      break;
    }
    JPDelay(5);
  }

  const char *items[6] = {"00 Return", "01 Dashboard", "02 Logd on COM",
                          "03 Enter FASTBOOT", "   Enter Recovery",
                          "04 Upgrade from REC"};
  int sel = 0;
  bool editing = false;
  bool pending_dashboard = SM_Debug_Dashboard();
  bool pending_log_com = SM_Debug_LogCom();
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (editing && sel == 1) {
        pending_dashboard = !pending_dashboard;
      } else if (editing && sel == 2) {
        pending_log_com = !pending_log_com;
      } else {
        sel = (sel + 1) % 6;
      }
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (editing && sel == 1) {
        pending_dashboard = !pending_dashboard;
      } else if (editing && sel == 2) {
        pending_log_com = !pending_log_com;
      } else {
        sel = (sel - 1 + 6) % 6;
      }
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0) {
        return;
      }
      if (sel == 3 || sel == 4 || sel == 5) {
        const bool to_fastboot = (sel == 3);
        const bool to_upgrade = (sel == 5);
        if (confirm_show(to_fastboot ? "FB" : (to_upgrade ? "UPG" : "REC"),
                         to_fastboot ? "Enter FASTBOOT now?"
                                     : (to_upgrade ? "Upgrade from REC?"
                                                   : "Enter Recovery now?"))) {
          uint32_t target = to_fastboot ? FLASH_BL_BOOT_FASTBOOT :
                            to_upgrade ? FLASH_BL_BOOT_RECOVERY_UPGRADE :
                                         FLASH_BL_BOOT_RECOVERY;
          if (Flash_BL_SetBootTarget(target) == FLASH_OK) {
            NVIC_SystemReset();
          } else {
            alert_show("BOOT", "Failed to set boot target.");
          }
        }
        lu = 0;
        editing = false;
      } else
      if (!editing) {
        pending_dashboard = SM_Debug_Dashboard();
        pending_log_com = SM_Debug_LogCom();
        editing = true;
      } else {
        if (sel == 1) {
          SM_Debug_SetDashboard(pending_dashboard);
        } else if (sel == 2) {
          SM_Debug_SetLogCom(pending_log_com);
        }
        SysUI_DebugOverlaySetEnabled(pending_dashboard ? 1U : 0U);
        editing = false;
      }
      lu = 0;
    }
    le = ce;

    if (HAL_GetTick() - lu > 16U) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        UI_DrawFrameTitle("DEBUG");
        PD_SetFont(FONT_ASCII_16);
        for (int i = 0; i < 6; i++) {
          int cy = 33 + i * 25;
          if (i == 1) {
            UI_DrawMenuValue(i, sel, cy, items[i],
                           pending_dashboard ? "ON" : "OFF",
                           editing && sel == i);
          } else if (i == 2) {
            UI_DrawMenuValue(i, sel, cy, items[i],
                           pending_log_com ? "ON" : "OFF",
                           editing && sel == i);
          } else {
            UI_DrawMenuCardEx(i, sel, cy, items[i], false);
          }
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

static bool handle_build_debug_click(uint32_t now, bool on_build) {
  static uint8_t count = 0;
  static uint32_t last_ms = 0;

  if (!on_build) {
    count = 0;
    last_ms = 0;
    return false;
  }

  if (last_ms == 0U || (uint32_t)(now - last_ms) > BUILD_DEBUG_WINDOW_MS) {
    count = 0;
  }
  last_ms = now;
  count++;
  if (count >= BUILD_DEBUG_CLICKS) {
    count = 0;
    last_ms = 0;
    return true;
  }
  return false;
}

void about_activity_run(void) {

  char running_value[16];
  format_uptime(running_value, sizeof(running_value));

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
      {"   Running", running_value},
      {"03 Update System", ""},
      {"   Reboot Device", ""},
      {"   Restore to Default", ""},
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t last_draw = 0;
  uint32_t last_running_sec = 0xFFFFFFFFU;
  bool dirty = true;

  while (1) {
    uint32_t now = HAL_GetTick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % AM_N;
      if (sel != 6) {
        handle_build_debug_click(now, false);
      }
      dirty = true;
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + AM_N) % AM_N;
      if (sel != 6) {
        handle_build_debug_click(now, false);
      }
      dirty = true;
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 6: /* Build: hidden DEBUG page */
        if (handle_build_debug_click(now, true)) {
          if (confirm_show("DEBUG", "Whether to enter debugging settings?")) {
            debug_page();
          }
          dirty = true;
          last_draw = 0;
        }
        break;
      case 9: /* Update System */ {
        /* Loading screen */
        LCD_FLUSH({
          UI_DrawFrameTitle("UPD");
          PD_SetColor(TOS_TEXT);
          PD_DrawString(26, 33, "Checking...");
        });

        TosUpgradeInfo info;
        if (TosApi_CheckUpgrade(&info)) {
          if (info.has_update) {
            char msg[200];
            const char *ver =
                info.latest_version[0] ? info.latest_version : "-";
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
        break;
      }
      case 10: /* Reboot Device */
        if (confirm_show("REB", "Reboot the device now?")) {
          NVIC_SystemReset();
        }
        break;
      case 11: /* Restore to Default */
        if (confirm_show("RST", "Erase all settings?\nDevice will reboot.")) {
          /* Destructive USERDATA erase belongs to the independent REC image,
           * not to SYSTEM.  Persist the one-shot target in TEE and reset; REC
           * will erase and verify sector 11, then reboot into clean defaults. */
          if (!Flash_BL_RecoveryAvailable()) {
            alert_show("RST", "Recovery is unavailable.");
          } else if (Flash_BL_SetBootTarget(
                         FLASH_BL_BOOT_RECOVERY_FORMAT) == FLASH_OK) {
            NVIC_SystemReset();
          } else {
            alert_show("RST", "Failed to enter recovery.");
          }
        }
        break;
      }
      if (sel != 6) {
        handle_build_debug_click(now, false);
      }
    }
    le = ce;

    uint32_t running_sec = HAL_GetTick() / 1000U;
    if (running_sec != last_running_sec) {
      last_running_sec = running_sec;
      format_uptime(running_value, sizeof(running_value));
      dirty = true;
    }

    now = HAL_GetTick();
    if (dirty || (uint32_t)(now - last_draw) >= 1000U) {
      last_draw = now;
      dirty = false;
      LCD_FLUSH({
        UI_DrawFrameTitle("ABOUT");
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
            UI_DrawMenuValue(idx, sel, cy, items[idx].l, items[idx].v, false);
          else
            UI_DrawMenuCard(idx, sel, cy, items[idx].l);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}
