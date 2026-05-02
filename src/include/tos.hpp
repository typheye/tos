#ifndef TOS_HPP
#define TOS_HPP

#include "include/buzzer.hpp"
#include "include/esp8266.hpp"
#include "include/jy901s.hpp"
#include "include/key.hpp"
#include "include/lcd.hpp"
#include "include/led.hpp"
#include "include/lib3dgyro.h"
#include "include/libfs.h"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/sysui.hpp"
#include "include/trtc.hpp"
#include "include/tsdio.hpp"
#include "include/usart.hpp"
#include <cstdint>

extern "C" {
#include "include/lib3dox.h"
}

#include "main.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Demo 类型枚举
enum class DemoType {
  NONE = 0,
  KEY_TEST,
  SD_CARD_TEST,
  I2C_SCAN,
  JY901S_TEST,
  DISPLAY_TEST,
  GYRO_CUBE,
  BMP180_TEST
};

class TOS {
public:
  TOS();
  ~TOS();

  void init();  // 初始化系统
  void start(); // 启动主循环

private:
  bool initialized_;
  uint32_t tick_count_;
};

#ifdef __cplusplus
extern "C" {
#endif
void start_tos(void);
#ifdef __cplusplus
}
#endif

#endif // TOS_HPP