/**
 * @file    settings_manager.c
 * @brief   Settings manager — Flash-backed persistent config
 */
#include "include/settings_manager.h"
#include "hardware/include/sfhd.h"
#include <stdio.h>
#include <string.h>

static Settings_t g_settings;

/* ========== Init defaults ========== */
static void defaults(void) {
  memset(&g_settings, 0, sizeof(g_settings));
  g_settings.magic = SM_MAGIC;
  g_settings.disp_auto   = true;
  g_settings.disp_bright = 10;
  g_settings.disp_dir    = 0;
  strcpy(g_settings.wlan_ssid, "");
  strcpy(g_settings.wlan_pwd, "");
  strcpy(g_settings.hs_ssid, "TOS-Hotspot");
  strcpy(g_settings.hs_pwd, "12345678");
  printf("[SM] Defaults loaded\r\n");
}

/* ========== Load from Flash ========== */
static bool load(void) {
  Settings_t tmp;
  Flash_Status_t st = Flash_Rolling_Read((uint32_t *)&tmp, sizeof(tmp), NULL);
  if (st != FLASH_OK) return false;
  if (tmp.magic != SM_MAGIC) {
    printf("[SM] Bad magic 0x%08lX\r\n", tmp.magic);
    return false;
  }
  memcpy(&g_settings, &tmp, sizeof(tmp));
  printf("[SM] Loaded from Flash OK\r\n");
  return true;
}

/* ========== Public ========== */

void SM_Init(void) {
  Flash_Check_Backup(); /* recover any interrupted write */
  if (!load()) {
    printf("[SM] Load failed — erasing sector for clean start\r\n");
    Flash_Erase_Sector();
    defaults();
    SM_Save(); /* persist defaults immediately */
  }
}

void SM_Save(void) {
  g_settings.magic = SM_MAGIC;
  g_settings.crc = 0;
  Flash_Status_t st = Flash_Rolling_Write((uint32_t *)&g_settings, sizeof(g_settings));
  printf("[SM] Save (%luB): %s\r\n", sizeof(g_settings),
         st == FLASH_OK ? "OK" : "FAIL");
}

Settings_t *SM_Get(void) { return &g_settings; }

/* --- Display --- */
bool    SM_Disp_Auto(void)        { return g_settings.disp_auto; }
uint8_t SM_Disp_Bright(void)      { return g_settings.disp_bright; }
uint8_t SM_Disp_Dir(void)         { return g_settings.disp_dir; }
void    SM_Disp_SetAuto(bool v)   { g_settings.disp_auto = v; SM_Save(); }
void    SM_Disp_SetBright(uint8_t v) { g_settings.disp_bright = v; SM_Save(); }
void    SM_Disp_SetDir(uint8_t v)    { g_settings.disp_dir = v; SM_Save(); }

/* --- WLAN --- */
const char *SM_Wlan_SSID(void)    { return g_settings.wlan_ssid; }
const char *SM_Wlan_PWD(void)     { return g_settings.wlan_pwd; }
void SM_Wlan_SetSSID(const char *s) { strncpy(g_settings.wlan_ssid, s, 23); SM_Save(); }
void SM_Wlan_SetPWD(const char *s)  { strncpy(g_settings.wlan_pwd, s, 31); SM_Save(); }

/* --- Hotspot --- */
const char *SM_Hotspot_SSID(void) { return g_settings.hs_ssid; }
const char *SM_Hotspot_PWD(void)  { return g_settings.hs_pwd; }
void SM_Hotspot_SetSSID(const char *s) { strncpy(g_settings.hs_ssid, s, 23); SM_Save(); }
void SM_Hotspot_SetPWD(const char *s)  { strncpy(g_settings.hs_pwd, s, 31); SM_Save(); }
