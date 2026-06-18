/**
 ******************************************************************************
 * @file    settings_manager.c
 * @author  Typheye
 * @brief   Settings Manager implementation.
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

#include "include/settings_manager.h"
#include <stddef.h>


#define CCMRAM __attribute__((section(".ccmram")))

static CCMRAM Settings_t g_settings;

static void copy_str(char *dst, const char *src, size_t cap) {
  if (!dst || cap == 0) return;
  if (!src) src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

static bool str_same(const char *a, const char *b, size_t cap) {
  if (!a) a = "";
  if (!b) b = "";
  return strncmp(a, b, cap) == 0;
}

static void sanitize(void) {
  g_settings.wlan_ssid[sizeof(g_settings.wlan_ssid) - 1] = '\0';
  g_settings.wlan_pwd[sizeof(g_settings.wlan_pwd) - 1] = '\0';
  g_settings.hs_ssid[sizeof(g_settings.hs_ssid) - 1] = '\0';
  g_settings.hs_pwd[sizeof(g_settings.hs_pwd) - 1] = '\0';
  g_settings.hotspot_ip[sizeof(g_settings.hotspot_ip) - 1] = '\0';

  if (g_settings.saved_count > SM_SAVED_MAX) {
    g_settings.saved_count = SM_SAVED_MAX;
  }
  g_settings.debug_dashboard = g_settings.debug_dashboard ? 1U : 0U;
  g_settings.debug_log_com = g_settings.debug_log_com ? 1U : 0U;
  g_settings.boot_gfx = g_settings.boot_gfx ? true : false;
  g_settings.disp_auto = g_settings.disp_auto ? true : false;
  g_settings.wlan_on = g_settings.wlan_on ? true : false;
  g_settings.wlan_auto_conn = g_settings.wlan_auto_conn ? true : false;
  g_settings.time_auto_sync = g_settings.time_auto_sync ? true : false;
  g_settings.time_style_24h = g_settings.time_style_24h ? true : false;
  g_settings.hotspot_auto_close =
      g_settings.hotspot_auto_close ? true : false;
  if (g_settings.disp_bright < 1U || g_settings.disp_bright > 10U) {
    g_settings.disp_bright = 10U;
  }
  if (g_settings.disp_dir > 1U) {
    g_settings.disp_dir = 0U;
  }
  for (uint8_t i = 0; i < SM_SAVED_MAX; ++i) {
    g_settings.saved[i].ssid[sizeof(g_settings.saved[i].ssid) - 1] = '\0';
    g_settings.saved[i].pwd[sizeof(g_settings.saved[i].pwd) - 1] = '\0';
  }

  if (!g_settings.hotspot_ip[0]) copy_str(g_settings.hotspot_ip, "192.168.4.1",
                                          sizeof(g_settings.hotspot_ip));
  if (!g_settings.hs_ssid[0]) copy_str(g_settings.hs_ssid, "TOS-Hotspot",
                                       sizeof(g_settings.hs_ssid));
  if (!g_settings.hs_pwd[0]) copy_str(g_settings.hs_pwd, "12345678",
                                      sizeof(g_settings.hs_pwd));
}

static bool sm_magic_compatible(uint32_t magic) {
  return (magic & 0xFFFFFF00u) == 0x544F5300u;
}

/* ========== Init defaults ========== */
static void defaults(void) {
  memset(&g_settings, 0, sizeof(g_settings));
  g_settings.magic = SM_MAGIC;
  g_settings.disp_auto   = true;
  g_settings.disp_bright = 10;
  g_settings.disp_dir    = 0;
  copy_str(g_settings.wlan_ssid, "", sizeof(g_settings.wlan_ssid));
  copy_str(g_settings.wlan_pwd, "", sizeof(g_settings.wlan_pwd));
  copy_str(g_settings.hs_ssid, "TOS-Hotspot", sizeof(g_settings.hs_ssid));
  copy_str(g_settings.hs_pwd, "12345678", sizeof(g_settings.hs_pwd));
  g_settings.wlan_on        = false;
  g_settings.wlan_auto_conn = false;
  g_settings.debug_dashboard = false;
  g_settings.debug_log_com   = false;
  g_settings.saved_count    = 0;
  g_settings.time_auto_sync     = true;
  g_settings.time_style_24h     = true;
  g_settings.hotspot_auto_close = true;
  g_settings.boot_gfx           = true;
  copy_str(g_settings.hotspot_ip, "192.168.4.1", sizeof(g_settings.hotspot_ip));
  LOG_D("SMGR", "Defaults loaded");
}

/* ========== Load from Flash ========== */
static bool load(void) {
  Settings_t tmp;
  uint32_t out_size = 0;
  memset(&tmp, 0, sizeof(tmp));
  Flash_Status_t st = Flash_Rolling_Read((uint32_t *)&tmp, sizeof(tmp), &out_size);
  if (st != FLASH_OK) return false;

  if (!sm_magic_compatible(tmp.magic)) {
    LOG_W("SMGR", "Bad magic 0x%08lX", (unsigned long)tmp.magic);
    return false;
  }

  defaults();
  {
    uint32_t copy_size = out_size;
    if (copy_size > sizeof(g_settings)) {
      copy_size = sizeof(g_settings);
    }
    memcpy(&g_settings, &tmp, copy_size);
  }
  if (out_size < (uint32_t)(offsetof(Settings_t, boot_gfx) + sizeof(g_settings.boot_gfx))) {
    g_settings.boot_gfx = true;
  }
  if (out_size < (uint32_t)(offsetof(Settings_t, debug_log_com) + sizeof(g_settings.debug_log_com))) {
    g_settings.debug_log_com = 0U;
  }
  g_settings.magic = SM_MAGIC;
  sanitize();
  LOG_I("SMGR", "Loaded from Flash OK (%luB)", (unsigned long)out_size);
  return true;
}

/* ========== Public ========== */

void SM_Init(void) {
  Flash_Check_Backup();
  if (!load()) {
    LOG_W("SMGR", "Load failed - using defaults");
    defaults();
    SM_Save();
  }
}

void SM_Save(void) {
  Settings_t verify;
  uint32_t verify_size;
  Flash_Status_t st = FLASH_ERR_PROGRAM;

  g_settings.magic = SM_MAGIC;
  g_settings.crc = 0;
  sanitize();

  for (uint8_t attempt = 0U; attempt < 2U; ++attempt) {
    st = Flash_Rolling_Write((uint32_t *)&g_settings, sizeof(g_settings));
    if (st == FLASH_OK) {
      memset(&verify, 0, sizeof(verify));
      verify_size = 0U;
      st = Flash_Rolling_Read((uint32_t *)&verify, sizeof(verify),
                              &verify_size);
      if (st == FLASH_OK && verify_size == sizeof(verify) &&
          memcmp(&verify, &g_settings, sizeof(verify)) == 0) {
        LOG_I("SMGR", "Save verified (%luB, try %u)",
              (unsigned long)sizeof(g_settings), (unsigned)(attempt + 1U));
        return;
      }
      st = FLASH_ERR_CRC;
    }
    LOG_W("SMGR", "Save attempt %u failed (%d)",
          (unsigned)(attempt + 1U), (int)st);
  }

  LOG_E("SMGR", "Save failed after retry (%luB, status=%d)",
        (unsigned long)sizeof(g_settings), (int)st);
}

Settings_t *SM_Get(void) { return &g_settings; }

/* --- Display --- */
bool    SM_Disp_Auto(void)          { return g_settings.disp_auto; }
uint8_t SM_Disp_Bright(void)        { return g_settings.disp_bright; }
uint8_t SM_Disp_Dir(void)           { return g_settings.disp_dir; }
void SM_Disp_SetAuto(bool v) {
  v = v ? true : false;
  if (g_settings.disp_auto != v) { g_settings.disp_auto = v; SM_Save(); }
}
void SM_Disp_SetBright(uint8_t v) {
  if (g_settings.disp_bright != v) { g_settings.disp_bright = v; SM_Save(); }
}
void SM_Disp_SetDir(uint8_t v) {
  if (g_settings.disp_dir != v) { g_settings.disp_dir = v; SM_Save(); }
}

/* --- WLAN current --- */
const char *SM_Wlan_SSID(void) { return g_settings.wlan_ssid; }
const char *SM_Wlan_PWD(void)  { return g_settings.wlan_pwd; }
void SM_Wlan_SetSSID(const char *value) {
  if (!str_same(g_settings.wlan_ssid, value, sizeof(g_settings.wlan_ssid))) {
    copy_str(g_settings.wlan_ssid, value, sizeof(g_settings.wlan_ssid));
    SM_Save();
  }
}
void SM_Wlan_SetPWD(const char *value) {
  if (!str_same(g_settings.wlan_pwd, value, sizeof(g_settings.wlan_pwd))) {
    copy_str(g_settings.wlan_pwd, value, sizeof(g_settings.wlan_pwd));
    SM_Save();
  }
}

/* --- WLAN settings --- */
bool SM_Wlan_On(void) { return g_settings.wlan_on; }
void SM_Wlan_SetOn(bool v) {
  v = v ? true : false;
  if (g_settings.wlan_on != v) { g_settings.wlan_on = v; SM_Save(); }
}
bool SM_Wlan_AutoConn(void) { return g_settings.wlan_auto_conn; }
void SM_Wlan_SetAutoConn(bool v) {
  v = v ? true : false;
  if (g_settings.wlan_auto_conn != v) {
    g_settings.wlan_auto_conn = v;
    SM_Save();
  }
}

/* --- Debug --- */
bool SM_Debug_Dashboard(void) { return g_settings.debug_dashboard != 0U; }
void SM_Debug_SetDashboard(bool v) {
  uint8_t next = v ? 1U : 0U;
  if (g_settings.debug_dashboard != next) {
    g_settings.debug_dashboard = next;
    SM_Save();
  }
}
bool SM_Debug_LogCom(void) { return g_settings.debug_log_com != 0U; }
void SM_Debug_SetLogCom(bool v) {
  uint8_t next = v ? 1U : 0U;
  if (g_settings.debug_log_com != next) {
    g_settings.debug_log_com = next;
    SM_Save();
  }
}

/* --- Saved networks --- */
uint8_t SM_Saved_Count(void) { return g_settings.saved_count; }

const SM_SavedNet_t *SM_Saved_Get(uint8_t idx) {
  if (idx >= g_settings.saved_count) return NULL;
  return &g_settings.saved[idx];
}

bool SM_Saved_Find(const char *ssid) {
  if (!ssid) return false;
  for (uint8_t i = 0; i < g_settings.saved_count; i++) {
    if (strcmp(g_settings.saved[i].ssid, ssid) == 0) return true;
  }
  return false;
}

bool SM_Saved_Add(const char *ssid, const char *pwd) {
  if (!ssid || !ssid[0]) return false;
  /* Update existing entry only when the password actually changed. */
  for (uint8_t i = 0; i < g_settings.saved_count; i++) {
    if (strcmp(g_settings.saved[i].ssid, ssid) == 0) {
      if (!str_same(g_settings.saved[i].pwd, pwd,
                    sizeof(g_settings.saved[i].pwd))) {
        copy_str(g_settings.saved[i].pwd, pwd,
                 sizeof(g_settings.saved[i].pwd));
        SM_Save();
      }
      return true;
    }
  }
  if (g_settings.saved_count >= SM_SAVED_MAX) {
    memmove(&g_settings.saved[0], &g_settings.saved[1],
            (SM_SAVED_MAX - 1U) * sizeof(SM_SavedNet_t));
    g_settings.saved_count = SM_SAVED_MAX - 1U;
  }
  copy_str(g_settings.saved[g_settings.saved_count].ssid, ssid,
           sizeof(g_settings.saved[g_settings.saved_count].ssid));
  copy_str(g_settings.saved[g_settings.saved_count].pwd, pwd,
           sizeof(g_settings.saved[g_settings.saved_count].pwd));
  g_settings.saved_count++;
  SM_Save();
  return true;
}

void SM_Saved_Del(uint8_t idx) {
  if (idx >= g_settings.saved_count) return;
  uint8_t tail = g_settings.saved_count - idx - 1U;
  if (tail > 0U) {
    memmove(&g_settings.saved[idx], &g_settings.saved[idx + 1U],
            tail * sizeof(SM_SavedNet_t));
  }
  g_settings.saved_count--;
  memset(&g_settings.saved[g_settings.saved_count], 0,
         sizeof(SM_SavedNet_t));
  SM_Save();
}

/* --- Time --- */
bool SM_Time_AutoSync(void) { return g_settings.time_auto_sync; }
void SM_Time_SetAutoSync(bool v) {
  v = v ? true : false;
  if (g_settings.time_auto_sync != v) {
    g_settings.time_auto_sync = v;
    SM_Save();
  }
}
bool SM_Time_Style24h(void) { return g_settings.time_style_24h; }
void SM_Time_SetStyle24h(bool v) {
  v = v ? true : false;
  if (g_settings.time_style_24h != v) {
    g_settings.time_style_24h = v;
    SM_Save();
  }
}

/* --- Hotspot --- */
bool SM_Hotspot_AutoClose(void) { return g_settings.hotspot_auto_close; }
void SM_Hotspot_SetAutoClose(bool v) {
  v = v ? true : false;
  if (g_settings.hotspot_auto_close != v) {
    g_settings.hotspot_auto_close = v;
    SM_Save();
  }
}
const char *SM_Hotspot_IP(void) { return g_settings.hotspot_ip; }
void SM_Hotspot_SetIP(const char *value) {
  if (!str_same(g_settings.hotspot_ip, value,
                sizeof(g_settings.hotspot_ip))) {
    copy_str(g_settings.hotspot_ip, value, sizeof(g_settings.hotspot_ip));
    SM_Save();
  }
}
const char *SM_Hotspot_SSID(void) { return g_settings.hs_ssid; }
const char *SM_Hotspot_PWD(void)  { return g_settings.hs_pwd; }
void SM_Hotspot_SetSSID(const char *value) {
  if (!str_same(g_settings.hs_ssid, value, sizeof(g_settings.hs_ssid))) {
    copy_str(g_settings.hs_ssid, value, sizeof(g_settings.hs_ssid));
    SM_Save();
  }
}
void SM_Hotspot_SetPWD(const char *value) {
  if (!str_same(g_settings.hs_pwd, value, sizeof(g_settings.hs_pwd))) {
    copy_str(g_settings.hs_pwd, value, sizeof(g_settings.hs_pwd));
    SM_Save();
  }
}

/* --- Sound & GFX --- */
bool SM_BootGfx(void) { return g_settings.boot_gfx; }
void SM_SetBootGfx(bool v) {
  v = v ? true : false;
  if (g_settings.boot_gfx != v) { g_settings.boot_gfx = v; SM_Save(); }
}

/* --- Status icon helpers (C-callable) --- */
bool esp_wlan_is_on(void) {
  /* If ESP8266 is hard-disabled, report WLAN as OFF regardless of Flash
   * setting — the icon in the status bar should show grey. */
  if (ESP8266_IsHardDisabled()) return false;
  return SM_Wlan_On();
}
