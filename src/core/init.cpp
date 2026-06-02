/**
 * @file    init.cpp
 * @brief   TOS system initialization — hardware, settings, WLAN, NTP, UI
 */

#include "core/include/tos.hpp"
#include "core/manager/include/settings_manager.h"
#include "core/sys/include/systime.h"
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
  HAL_Delay(50);

  PD_ShowSplashFadeStart(300);

  // ========== 关键：RTC 放在较前位置，但需要等待 LSE ==========
  // RTC 会自己等待 LSE 稳定，不需要额外延时
  boardTRTC.init(); // 已修复，内部会等待 LSE 并重试

  boardSDIO.init();
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

  ESP8266_Init();

  /* ── WLAN Auto-Connect ── */
  if (SM_Wlan_On() && SM_Wlan_AutoConn()) {
    LOG_I("MAIN", "Auto-connect: starting...");
    ESP8266_SendCommand("AT+CWMODE=1", "OK", 3000);
    HAL_Delay(300);

    int saved = SM_Saved_Count();
    for (int round = 0; round < 2; round++) {
      bool ok = false;
      for (int i = 0; i < saved; i++) {
        const SM_SavedNet_t *net = SM_Saved_Get(i);
        if (!net || !net->ssid[0])
          continue;
        LOG_I("MAIN", "Auto-connect: trying %s (round %d)...", net->ssid,
              round + 1);
        if (ESP8266_ConnectWiFi(net->ssid, net->pwd)) {
          HAL_Delay(500);
          if (ESP8266_IsConnected()) {
            SM_Wlan_SetSSID(net->ssid);
            SM_Wlan_SetPWD(net->pwd);
            LOG_I("MAIN", "Auto-connect: connected to %s!", net->ssid);
            ok = true;
            break;
          }
        }
        HAL_Delay(300);
      }
      if (ok)
        break;
    }
  }

  /* Background NTP sync — only if Auto Sync is ON */
  if (SM_Wlan_On() && ESP8266_IsConnected() && SM_Time_AutoSync()) {
    SysTime_Sync();
  }

  SysUI::init();

  HAL_Delay(50);

  PD_SplashFinish(100);

  boardLCD.fillScreen(LCD_COLOR_BLACK);
}
