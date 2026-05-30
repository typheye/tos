/**
 * @file    settings_manager.h
 * @brief   Unified settings manager — all persistent config in one struct
 *          Uses sfhd Flash storage for persistence across reboots
 */
#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SM_MAGIC 0x544F5302u  /* "TOS\2" */

/* ========== Unified settings struct ========== */
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t crc;

  /* --- Display --- */
  bool     disp_auto;        /* auto brightness on/off */
  uint8_t  disp_bright;      /* manual brightness 1-10 */
  uint8_t  disp_dir;         /* rotation 0-1 */

  /* --- WLAN --- */
  char     wlan_ssid[24];
  char     wlan_pwd[32];

  /* --- Hotspot --- */
  char     hs_ssid[24];
  char     hs_pwd[32];

  /* --- Alignment fix: ensures struct size is multiple of 4 for Flash word writes --- */
  uint8_t  _align8;

  /* --- Reserved --- */
  uint32_t _pad[4];
} Settings_t;

/* ========== API ========== */

/**
 * @brief  Initialize: load from Flash, or init defaults if first boot
 */
void SM_Init(void);

/**
 * @brief  Save current settings to Flash (rolling write)
 */
void SM_Save(void);

/**
 * @brief  Get pointer to settings struct (read/write directly)
 */
Settings_t *SM_Get(void);

/* Convenience getters */
bool    SM_Disp_Auto(void);
uint8_t SM_Disp_Bright(void);
uint8_t SM_Disp_Dir(void);
void    SM_Disp_SetAuto(bool v);
void    SM_Disp_SetBright(uint8_t v);
void    SM_Disp_SetDir(uint8_t v);

const char *SM_Wlan_SSID(void);
const char *SM_Wlan_PWD(void);
void SM_Wlan_SetSSID(const char *s);
void SM_Wlan_SetPWD(const char *s);

const char *SM_Hotspot_SSID(void);
const char *SM_Hotspot_PWD(void);
void SM_Hotspot_SetSSID(const char *s);
void SM_Hotspot_SetPWD(const char *s);

#ifdef __cplusplus
}
#endif

#endif
