/**
 * @file    settings_manager.c
 * @brief   Settings manager — Flash-backed persistent config
 */
#include "settings_manager.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/sfhd.h"
#include "syslog.h"
#include <stdio.h>
#include <string.h>

#define CCMRAM __attribute__((section(".ccmram")))
static CCMRAM Settings_t g_settings;

static void copy_str(char *dst, const char *src, size_t cap) {
  if (!dst || cap == 0) return;
  if (!src) src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
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
  g_settings.saved_count    = 0;
  g_settings.time_auto_sync     = true;
  g_settings.time_style_24h     = true;
  g_settings.hotspot_auto_close = true;
  copy_str(g_settings.hotspot_ip, "192.168.4.1", sizeof(g_settings.hotspot_ip));
  LOG_D("SMGR", "Defaults loaded");
}

/* ========== Load from Flash ========== */
static bool load(void) {
  Settings_t tmp;
  Flash_Status_t st = Flash_Rolling_Read((uint32_t *)&tmp, sizeof(tmp), NULL);
  if (st != FLASH_OK) return false;
  if (tmp.magic != SM_MAGIC) {
    LOG_W("SMGR", "Bad magic 0x%08lX", (unsigned long)tmp.magic);
    return false;
  }
  memcpy(&g_settings, &tmp, sizeof(tmp));
  sanitize();
  LOG_I("SMGR", "Loaded from Flash OK");
  return true;
}

/* ========== Public ========== */

void SM_Init(void) {
  Flash_Check_Backup();
  if (!load()) {
    LOG_W("SMGR", "Load failed — erasing sector for clean start");
    Flash_Erase_Sector();
    defaults();
    SM_Save();
  }
}

void SM_Save(void) {
  g_settings.magic = SM_MAGIC;
  g_settings.crc = 0;
  sanitize();
  Flash_Status_t st = Flash_Rolling_Write((uint32_t *)&g_settings, sizeof(g_settings));
  LOG_I("SMGR", "Save (%luB): %s", (unsigned long)sizeof(g_settings),
        st == FLASH_OK ? "OK" : "FAIL");
  (void)st;
}

Settings_t *SM_Get(void) { return &g_settings; }

/* --- Display --- */
bool    SM_Disp_Auto(void)          { return g_settings.disp_auto; }
uint8_t SM_Disp_Bright(void)        { return g_settings.disp_bright; }
uint8_t SM_Disp_Dir(void)           { return g_settings.disp_dir; }
void    SM_Disp_SetAuto(bool v)     { g_settings.disp_auto = v; SM_Save(); }
void    SM_Disp_SetBright(uint8_t v){ g_settings.disp_bright = v; SM_Save(); }
void    SM_Disp_SetDir(uint8_t v)   { g_settings.disp_dir = v; SM_Save(); }

/* --- WLAN current --- */
const char *SM_Wlan_SSID(void)       { return g_settings.wlan_ssid; }
const char *SM_Wlan_PWD(void)        { return g_settings.wlan_pwd; }
void SM_Wlan_SetSSID(const char *s)  { copy_str(g_settings.wlan_ssid, s, sizeof(g_settings.wlan_ssid)); SM_Save(); }
void SM_Wlan_SetPWD(const char *s)   { copy_str(g_settings.wlan_pwd, s, sizeof(g_settings.wlan_pwd)); SM_Save(); }

/* --- WLAN settings --- */
bool SM_Wlan_On(void)               { return g_settings.wlan_on; }
void SM_Wlan_SetOn(bool v)          { g_settings.wlan_on = v; SM_Save(); }
bool SM_Wlan_AutoConn(void)         { return g_settings.wlan_auto_conn; }
void SM_Wlan_SetAutoConn(bool v)    { g_settings.wlan_auto_conn = v; SM_Save(); }

/* --- Saved networks --- */
uint8_t SM_Saved_Count(void) { return g_settings.saved_count; }

const SM_SavedNet_t *SM_Saved_Get(uint8_t idx) {
  if (idx >= g_settings.saved_count) return NULL;
  return &g_settings.saved[idx];
}

bool SM_Saved_Find(const char *ssid) {
  for (uint8_t i = 0; i < g_settings.saved_count; i++) {
    if (strcmp(g_settings.saved[i].ssid, ssid) == 0) return true;
  }
  return false;
}

bool SM_Saved_Add(const char *ssid, const char *pwd) {
  /* Update existing entry if found */
  for (uint8_t i = 0; i < g_settings.saved_count; i++) {
    if (strcmp(g_settings.saved[i].ssid, ssid) == 0) {
      copy_str(g_settings.saved[i].pwd, pwd, sizeof(g_settings.saved[i].pwd));
      SM_Save();
      return true;
    }
  }
  /* Add new entry */
  if (g_settings.saved_count >= SM_SAVED_MAX) {
    /* Shift oldest out (index 0) */
    memmove(&g_settings.saved[0], &g_settings.saved[1],
            (SM_SAVED_MAX - 1) * sizeof(SM_SavedNet_t));
    g_settings.saved_count = SM_SAVED_MAX - 1;
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
  uint8_t tail = g_settings.saved_count - idx - 1;
  if (tail > 0)
    memmove(&g_settings.saved[idx], &g_settings.saved[idx + 1],
            tail * sizeof(SM_SavedNet_t));
  g_settings.saved_count--;
  SM_Save();
}

/* --- Time --- */
bool SM_Time_AutoSync(void)        { return g_settings.time_auto_sync; }
void SM_Time_SetAutoSync(bool v)   { g_settings.time_auto_sync = v; SM_Save(); }
bool SM_Time_Style24h(void)        { return g_settings.time_style_24h; }
void SM_Time_SetStyle24h(bool v)   { g_settings.time_style_24h = v; SM_Save(); }

/* --- Hotspot --- */
bool SM_Hotspot_AutoClose(void)        { return g_settings.hotspot_auto_close; }
void SM_Hotspot_SetAutoClose(bool v)   { g_settings.hotspot_auto_close = v; SM_Save(); }
const char *SM_Hotspot_IP(void)        { return g_settings.hotspot_ip; }
void SM_Hotspot_SetIP(const char *s)   { copy_str(g_settings.hotspot_ip, s, sizeof(g_settings.hotspot_ip)); SM_Save(); }
const char *SM_Hotspot_SSID(void) { return g_settings.hs_ssid; }
const char *SM_Hotspot_PWD(void)  { return g_settings.hs_pwd; }
void SM_Hotspot_SetSSID(const char *s) { copy_str(g_settings.hs_ssid, s, sizeof(g_settings.hs_ssid)); SM_Save(); }
void SM_Hotspot_SetPWD(const char *s)  { copy_str(g_settings.hs_pwd, s, sizeof(g_settings.hs_pwd)); SM_Save(); }

/* --- Status icon helpers (C-callable) --- */
bool esp_wlan_is_on(void) {
  /* If ESP8266 is hard-disabled, report WLAN as OFF regardless of Flash
   * setting — the icon in the status bar should show grey. */
  if (ESP8266_IsHardDisabled()) return false;
  return SM_Wlan_On();
}
