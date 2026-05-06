#include "include/tos.hpp"
#include "demo/include/bmp_activity.hpp"
#include "demo/include/display_activity.hpp"
#include "demo/include/i2c_activity.hpp"
#include "demo/include/jyro_activity.hpp"
#include "demo/include/key_activity.hpp"
#include "demo/include/sd_activity.hpp"
#include "include/lcd.h"

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
extern BMP180 boardBMP180; // 添加 BMP180 外部声明
extern I2C_HandleTypeDef hi2c1;
extern LED boardLed;
extern LED warnLed;
extern LED errorLed;
extern Buzzer buzzer1;
extern ESP8266 esp8266;
extern TCS3472 boardTCS3472;
extern SN74HC00N boardHC00N;
extern Potentiometer boardPot;

TOS::TOS() : initialized_(false), tick_count_(0) {}

// 在 tos.cpp 的 TOS::init() 函数末尾添加

void TOS::init() {
  boardSerial.init();

  // LED 初始化
  boardLed.init();
  warnLed.init();
  errorLed.init();

  buzzer1.init();
  boardLCD.init();
  HAL_Delay(50);

  PD_ShowSplashFadeStart(500);

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

  initialized_ = true;
  boardLed.off();

  boardLed.on();
  HAL_Delay(50);

  warnLed.on();
  HAL_Delay(50);
  errorLed.on();

  printf("TOS initialized!\r\n");

  ESP8266_Init();
  SysUI::init();
}

void TOS::start() {
  if (!initialized_)
    init();

  warnLed.off();
  HAL_Delay(50);
  errorLed.off();
  HAL_Delay(50);

  boardLed.off();
  HAL_Delay(50);

  PD_SplashFinish(100);

  boardLCD.fillScreen(LCD_COLOR_BLACK);

  // 设置默认界面为启动器
  SysUI::setActivity(UI_LAUNCHER);

  // 主循环
  while (1) {
    SysUI::loop();
    HAL_Delay(10); // 10ms 轮询间隔
  }
}

TOS::~TOS() {}

extern "C" void start_tos(void) {
  TOS os;
  os.init();
  os.start();
}