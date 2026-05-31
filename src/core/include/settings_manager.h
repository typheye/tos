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

#define SM_MAGIC        0x544F5303u  /* "TOS\3" — bump on struct change */
#define SM_SAVED_MAX    10           /* max saved WiFi networks */

/* ========== Saved WiFi network entry ========== */
typedef struct __attribute__((packed)) {
  char     ssid[24];
  char     pwd[32];
} SM_SavedNet_t;

/* ========== Unified settings struct ========== */
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t crc;

  /* --- Display --- */
  bool     disp_auto;        /* auto brightness on/off */
  uint8_t  disp_bright;      /* manual brightness 1-10 */
  uint8_t  disp_dir;         /* rotation 0-1 */

  /* --- WLAN (current connection) --- */
  char     wlan_ssid[24];
  char     wlan_pwd[32];

  /* --- Hotspot --- */
  char     hs_ssid[24];
  char     hs_pwd[32];

  /* --- WLAN settings --- */
  bool     wlan_on;          /* WiFi enabled */
  bool     wlan_auto_conn;   /* auto-connect on enable */
  uint8_t  _pad1[3];         /* alignment */

  /* --- Saved networks --- */
  uint8_t  saved_count;                    /* 0 .. SM_SAVED_MAX */
  SM_SavedNet_t saved[SM_SAVED_MAX];       /* 10 * 56 = 560 bytes */

  /* --- Time settings --- */
  bool     time_auto_sync;   /* auto NTP sync on boot */
  bool     time_style_24h;   /* 24h (true) or 12h (false) */

  /* --- Alignment tail --- */
  uint8_t  _pad2[1];         /* ensures sizeof % 4 == 0 */
} Settings_t;

/* ========== API ========== */

void SM_Init(void);
void SM_Save(void);
Settings_t *SM_Get(void);

/* --- Display --- */
bool    SM_Disp_Auto(void);
uint8_t SM_Disp_Bright(void);
uint8_t SM_Disp_Dir(void);
void    SM_Disp_SetAuto(bool v);
void    SM_Disp_SetBright(uint8_t v);
void    SM_Disp_SetDir(uint8_t v);

/* --- WLAN current --- */
const char *SM_Wlan_SSID(void);
const char *SM_Wlan_PWD(void);
void SM_Wlan_SetSSID(const char *s);
void SM_Wlan_SetPWD(const char *s);

/* --- WLAN settings --- */
bool SM_Wlan_On(void);
void SM_Wlan_SetOn(bool v);
bool SM_Wlan_AutoConn(void);
void SM_Wlan_SetAutoConn(bool v);

/* --- Saved networks --- */
uint8_t SM_Saved_Count(void);
const SM_SavedNet_t *SM_Saved_Get(uint8_t idx);
bool SM_Saved_Add(const char *ssid, const char *pwd);
void SM_Saved_Del(uint8_t idx);
bool SM_Saved_Find(const char *ssid);

/* --- Time --- */
bool SM_Time_AutoSync(void);
void SM_Time_SetAutoSync(bool v);
bool SM_Time_Style24h(void);
void SM_Time_SetStyle24h(bool v);

/* --- Hotspot --- */
const char *SM_Hotspot_SSID(void);
const char *SM_Hotspot_PWD(void);
void SM_Hotspot_SetSSID(const char *s);
void SM_Hotspot_SetPWD(const char *s);

#ifdef __cplusplus
}
#endif

#endif
