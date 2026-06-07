/**
 ******************************************************************************
 * @file    init.cpp
 * @author  Typheye
 * @brief   Core startup orchestration implementation.
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

#include "include/init.hpp"
#include "core/manager/include/file_manager.h"


extern "C" {
}


extern USART boardSerial;
extern TRTC boardTRTC;
extern LCD boardLCD;
extern TSDIO boardSDIO;
extern KeyManager keyManager;
extern JY901S boardJY901S;
extern BMP180 boardBMP180;
extern I2C_HandleTypeDef hi2c1;
extern LED boardLed;
extern LED warnLed;
extern LED errorLed;
extern Buzzer buzzer1;
extern ESP8266 esp8266;
extern TCS3472 boardTCS3472;
extern SN74HC00N boardHC00N;
extern Potentiometer boardPot;
extern THID boardHID;

static bool sd_root_name_eq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return *a == '\0' && *b == '\0';
}

static bool sd_root_entry_allowed(const FMCore_Entry *entry) {
  if (!entry || !entry->name[0]) return true;
  if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0) return true;
#ifdef AM_VOL
  if ((entry->attr & AM_VOL) != 0U) return true;
#endif

  if (sd_root_name_eq(entry->name, "init")) {
    return entry->is_dir == 0U;
  }

  static const char *kAllowedDirs[] = {
      "data", "oem", "dev", "storage", "system"};
  for (unsigned i = 0; i < sizeof(kAllowedDirs) / sizeof(kAllowedDirs[0]); ++i) {
    if (sd_root_name_eq(entry->name, kAllowedDirs[i])) {
      return entry->is_dir != 0U;
    }
  }
  return false;
}

static FRESULT sd_root_find_extra(char *path, size_t path_size,
                                  bool *out_found) {
  DIR dir;
  FILINFO fno;
  FMCore_Entry entry;
  FRESULT res;

  if (out_found) *out_found = false;
  if (!path || path_size == 0U || !out_found) return FR_INVALID_PARAMETER;
  path[0] = '\0';

  res = f_opendir(&dir, "0:");
  if (res != FR_OK) return res;

  while (1) {
    SysWatchdog_Tick();
    res = f_readdir(&dir, &fno);
    if (res != FR_OK) {
      (void)f_closedir(&dir);
      return res;
    }
    if (fno.fname[0] == '\0') break;

    strncpy(entry.name, fno.fname, FMCORE_NAME_MAX - 1U);
    entry.name[FMCORE_NAME_MAX - 1U] = '\0';
    entry.is_dir = (fno.fattrib & AM_DIR) ? 1U : 0U;
    entry.size = fno.fsize;
    entry.attr = fno.fattrib;
    if (sd_root_entry_allowed(&entry)) continue;

    if (!FMCore_JoinPath("0:", entry.name, path, path_size)) {
      (void)f_closedir(&dir);
      return FR_INVALID_NAME;
    }
    *out_found = true;
    break;
  }

  FRESULT close_res = f_closedir(&dir);
  return close_res == FR_OK ? res : close_res;
}

static FRESULT sd_delete_recursive_quiet(const char *path) {
  FILINFO info;
  FRESULT res;

  if (!path || !path[0]) return FR_INVALID_PARAMETER;
  SysWatchdog_Tick();

  res = f_stat(path, &info);
  if (res != FR_OK) return res;

#if _USE_CHMOD
  (void)f_chmod(path, 0, AM_RDO);
#endif
  if ((info.fattrib & AM_DIR) == 0U) {
    return f_unlink(path);
  }

  while (1) {
    DIR dir;
    FILINFO fno;
    char child[FMCORE_PATH_MAX];
    bool found = false;

    res = f_opendir(&dir, path);
    if (res != FR_OK) return res;
    while (1) {
      res = f_readdir(&dir, &fno);
      if (res != FR_OK || fno.fname[0] == '\0') break;
      if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0) continue;
      if (!FMCore_JoinPath(path, fno.fname, child, sizeof(child))) {
        res = FR_INVALID_NAME;
        break;
      }
      found = true;
      break;
    }
    (void)f_closedir(&dir);

    if (res != FR_OK) return res;
    if (!found) break;

    res = sd_delete_recursive_quiet(child);
    if (res != FR_OK && res != FR_NO_FILE && res != FR_NO_PATH) return res;
    SysWatchdog_Tick();
  }

#if _USE_CHMOD
  (void)f_chmod(path, 0, AM_RDO);
#endif
  return f_unlink(path);
}

static void sd_cleanup_disable_for_boot(const char *reason, FRESULT res) {
  SysLog_DisableFileOutput();
  LOG_W("MAIN", "SD root cleanup disabled SD for this boot: %s => %s(%d)",
        reason ? reason : "?", FMCore_FResultName(res), (int)res);
  FMCore_Unmount();
  TSDIO_MarkHardDisabled();
}

static void cleanup_sd_root_whitelist(void) {
  if (TSDIO_IsHardDisabled() || !TSDIO_IsInitialized() ||
      !FMCore_IsInitialized()) {
    return;
  }

  FRESULT mount_res = FMCore_Mount(NULL, false);
  if (mount_res != FR_OK) {
    sd_cleanup_disable_for_boot("mount root", mount_res);
    return;
  }

  for (uint16_t pass = 0; pass < 128U; ++pass) {
    char path[FMCORE_PATH_MAX];
    bool found = false;
    FRESULT list_res = sd_root_find_extra(path, sizeof(path), &found);
    if (list_res != FR_OK) {
      sd_cleanup_disable_for_boot("list root", list_res);
      return;
    }
    if (!found) return;

    SysWatchdog_FeedNow();
    FRESULT del_res = sd_delete_recursive_quiet(path);
    SysWatchdog_FeedNow();
    if (del_res != FR_OK && del_res != FR_NO_FILE && del_res != FR_NO_PATH) {
      sd_cleanup_disable_for_boot(path, del_res);
      return;
    }

    LOG_I("MAIN", "Removed SD root extra: %s", path);
  }

  LOG_W("MAIN", "SD root cleanup pass limit reached");
}

void TOS::init() {
  boardSerial.init();

  
  boardLed.init();
  warnLed.init();
  errorLed.init();

  buzzer1.init();
  boardLCD.init();
  SysWatchdog_Init();
  SysWatchdog_FeedNow();
  HAL_Delay(50);

  PD_ShowSplashFadeStart(300);

  
  
  boardTRTC.init(); 

  boardSDIO.init();
  if (TSDIO_IsHardDisabled()) {
    LOG_W("MAIN", "SD card is hard-disabled - SD features unavailable");
  } else {
    cleanup_sd_root_whitelist();
  }
  /* Defer the previous-IWDG error screen until SD/logging is available.
   * Otherwise the controlled reboot happens before a persistent dump can be
   * written, leaving an abruptly truncated boot log with no reset reason. */
  SysWatchdog_ShowBootReasonIfAny();

  keyManager.init();
  boardJY901S.init();
  boardBMP180.init();
  boardTCS3472.init();
  boardHC00N.init();
  boardPot.init();
  boardHID.init();

  SM_Init();
  boardLCD.setAutoBrightness(SM_Disp_Auto());
  boardLCD.setRotation(SM_Disp_Dir());
  if (!SM_Disp_Auto())
    boardLCD.setBrightness((uint16_t)SM_Disp_Bright() * 100);

  initialized_ = true;

  LOG_I("MAIN", "System initialized, CPU:168MHz");

  SysWatchdog_FeedNow();
  ESP8266_Init();
  SysWatchdog_FeedNow();
  if (!ESP8266_IsHardDisabled()) {
    Net_ConfigureStationCompatibility();
    SysWatchdog_FeedNow();
  }

  /* ── WLAN Auto-Connect ──
   * Skip entirely if ESP8266 is hard-disabled (module not responding). */
  if (!ESP8266_IsHardDisabled() && SM_Wlan_On() && SM_Wlan_AutoConn()) {
    int saved = SM_Saved_Count();
    bool has_saved = false;
    for (int i = 0; i < saved; i++) {
      const SM_SavedNet_t *net = SM_Saved_Get(i);
      if (net && net->ssid[0]) { has_saved = true; break; }
    }

    if (has_saved) {
      LOG_I("MAIN", "Auto-connect: starting...");
      ESP8266_SendCommand("AT+CWMODE=1", "OK", 3000);
      HAL_Delay(300);
      SysWatchdog_FeedNow();

      const char *preferred_ssid = SM_Wlan_SSID();
      int preferred_idx = -1;
      if (preferred_ssid && preferred_ssid[0]) {
        for (int i = 0; i < saved; i++) {
          const SM_SavedNet_t *net = SM_Saved_Get(i);
          if (net && strcmp(net->ssid, preferred_ssid) == 0) {
            preferred_idx = i;
            break;
          }
        }
      }

      bool ok = false;
      auto try_saved = [&](int idx, int round) -> bool {
        const SM_SavedNet_t *net = SM_Saved_Get(idx);
        if (!net || !net->ssid[0]) return false;
        LOG_I("MAIN", "Auto-connect: trying %s (round %d/2)...",
              net->ssid, round + 1);
        if (ESP8266_ConnectWiFi(net->ssid, net->pwd)) {
          HAL_Delay(500);
          SysWatchdog_FeedNow();
          if (ESP8266_IsConnected()) {
            SM_Wlan_SetSSID(net->ssid);
            SM_Wlan_SetPWD(net->pwd);
            LOG_I("MAIN", "Auto-connect: connected to %s!", net->ssid);
            return true;
          }
        }

        SysWatchdog_FeedNow();
        if (round == 0) {
          LOG_W("MAIN", "Auto-connect: %s failed, light cleanup before retry",
                net->ssid);
          esp8266.resetRxBuffer();
          ESP8266_SendCommand("AT+CIPCLOSE", "OK", 800);
          ESP8266_SendCommand("AT+CWQAP", "OK", 1200);
          esp8266.resetRxBuffer();
          HAL_Delay(400);
        } else {
          HAL_Delay(300);
        }
        SysWatchdog_FeedNow();
        return false;
      };

      for (int round = 0; round < 2 && !ok; round++) {
        if (preferred_idx >= 0) ok = try_saved(preferred_idx, round);
        for (int i = 0; i < saved && !ok; i++) {
          if (i == preferred_idx) continue;
          ok = try_saved(i, round);
        }
      }

      if (!ok) {
        LOG_W("MAIN", "Auto-connect: no saved WLAN joined; offline for this boot");
      }
    } else {
      LOG_I("MAIN", "Auto-connect skipped: no saved WLAN");
    }
  }

  /* Do not run blocking time sync during boot.  TosApi_Tick schedules a
   * deferred auto sync after UI startup and a proven cloud connection. */
  if (!ESP8266_IsHardDisabled() && SM_Wlan_On() &&
      ESP8266_IsConnected() && SM_Time_AutoSync()) {
    LOG_I("MAIN", "Auto time sync deferred until after UI startup");
  }

  EmotionManager_Init();
  HidManager_Init();
  TosApi_Init();

  SysUI::init();

  HAL_Delay(50);

  PD_SplashFinish(100);

  
  SysUI::setActivity(UI_PET);
}
