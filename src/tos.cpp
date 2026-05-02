#include "include/tos.hpp"
#include "demo/include/bmp_activity.hpp"
#include "demo/include/display_activity.hpp"
#include "demo/include/i2c_activity.hpp"
#include "demo/include/jyro_activity.hpp"
#include "demo/include/key_activity.hpp"
#include "demo/include/sd_activity.hpp"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/esp8266.hpp"
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
extern Buzzer buzzer1;
extern ESP8266 esp8266;

TOS::TOS() : initialized_(false), tick_count_(0) {}

// 在 tos.cpp 的 TOS::init() 函数末尾添加

void TOS::init() {
  boardSerial.init();
  buzzer1.init();
  boardTRTC.init();
  boardSDIO.init();
  keyManager.init();
  boardJY901S.init();
  boardBMP180.init();

  // 初始化 GUI 系统
  initialized_ = true;
  boardLed.off();

  printf("TOS initialized!\r\n");

  ESP8266_Init();

  boardLCD.init();

  SysUI::init();
}

void TOS::start() {
  if (!initialized_)
    init();

  boardLed.on();
  HAL_Delay(50);
  boardLed.off();
  HAL_Delay(50);
  boardLed.on();

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