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

static void cleanup_system_volume_information(void) {
  if (TSDIO_IsHardDisabled() || !TSDIO_IsInitialized() ||
      !FMCore_IsInitialized()) {
    return;
  }

  FRESULT mount_res = FMCore_Mount(NULL, false);
  if (mount_res != FR_OK) {
    LOG_W("MAIN", "SVI cleanup skipped: mount => %s(%d)",
          FMCore_FResultName(mount_res), (int)mount_res);
    return;
  }

  FILINFO info;
  FRESULT stat_res = FMCore_Stat("0:/System Volume Information", &info, false);
  if (stat_res == FR_NO_FILE || stat_res == FR_NO_PATH) {
    return;
  }
  if (stat_res != FR_OK) {
    LOG_W("MAIN", "SVI cleanup skipped: stat => %s(%d)",
          FMCore_FResultName(stat_res), (int)stat_res);
    return;
  }
  if ((info.fattrib & AM_DIR) == 0U) {
    LOG_W("MAIN", "SVI cleanup skipped: not a directory");
    return;
  }

  SysWatchdog_FeedNow();
  FRESULT del_res = FMCore_Delete("0:/System Volume Information", true, false);
  if (del_res == FR_OK) {
    LOG_I("MAIN", "Removed SD System Volume Information");
  } else {
    LOG_W("MAIN", "SVI cleanup failed: %s(%d)",
          FMCore_FResultName(del_res), (int)del_res);
  }
  SysWatchdog_FeedNow();
}

void TOS::init() {
  boardSerial.init();

  
  boardLed.init();
  warnLed.init();
  errorLed.init();

  buzzer1.init();
  boardLCD.init();
  SysWatchdog_Init();
  SysWatchdog_ShowBootReasonIfAny();
  SysWatchdog_FeedNow();
  HAL_Delay(50);

  PD_ShowSplashFadeStart(300);

  
  
  boardTRTC.init(); 

  boardSDIO.init();
  if (TSDIO_IsHardDisabled()) {
    LOG_W("MAIN", "SD card is hard-disabled - SD features unavailable");
  } else {
    cleanup_system_volume_information();
  }

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

      bool ok = false;
      for (int round = 0; round < 2 && !ok; round++) {
        for (int i = 0; i < saved; i++) {
          const SM_SavedNet_t *net = SM_Saved_Get(i);
          if (!net || !net->ssid[0]) continue;
          LOG_I("MAIN", "Auto-connect: trying %s (round %d/2)...",
                net->ssid, round + 1);
          if (ESP8266_ConnectWiFi(net->ssid, net->pwd)) {
            HAL_Delay(500);
            SysWatchdog_FeedNow();
            if (ESP8266_IsConnected()) {
              SM_Wlan_SetSSID(net->ssid);
              SM_Wlan_SetPWD(net->pwd);
              LOG_I("MAIN", "Auto-connect: connected to %s!", net->ssid);
              ok = true;
              break;
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
        }
      }

      if (!ok) {
        LOG_W("MAIN", "Auto-connect: no saved WLAN joined; offline for this boot");
      }
    } else {
      LOG_I("MAIN", "Auto-connect skipped: no saved WLAN");
    }
  }

  /* Do not run blocking time sync during boot.  SNTP/HTTP fallback can take
   * tens of seconds when the AP or upstream server is unstable, which made the
   * product look like it had entered a reboot state.  Time can still be synced
   * from Settings or the cloud sync_time command after the UI is responsive. */
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
