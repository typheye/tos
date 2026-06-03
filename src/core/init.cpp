/**
 * @file    init.cpp
 * @brief   TOS system initialization — hardware, settings, WLAN, NTP, UI
 */

#include "core/include/tos.hpp"
#include "core/manager/include/emotion_manager.h"
#include "core/manager/include/settings_manager.h"
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#include "core/sys/include/syswatchdog.h"
#include "demo/include/bmp_activity.hpp"
#include "demo/include/i2c_activity.hpp"
#include "demo/include/jyro_activity.hpp"
#include "demo/include/key_activity.hpp"
#include "demo/include/sd_activity.hpp"
#include "include/lcd.h"
#include "syslog.h"

extern "C" {
#include "include/lib3dox.h"
}

#include "main.h"
#include <stdio.h>

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

void TOS::init() {
  boardSerial.init();

  // LED 初始化
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

  // ========== 关键：RTC 放在较前位置，但需要等待 LSE ==========
  // RTC 会自己等待 LSE 稳定，不需要额外延时
  boardTRTC.init(); // 已修复，内部会等待 LSE 并重试

  boardSDIO.init();
  if (TSDIO_IsHardDisabled()) {
    LOG_W("MAIN", "SD card is hard-disabled — SD features unavailable");
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
  TosApi_Init();

  SysUI::init();

  HAL_Delay(50);

  PD_SplashFinish(100);

  // 设置默认界面为启动器
  SysUI::setActivity(UI_PET);
}
