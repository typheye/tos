/**
 ******************************************************************************
 * @file    tos.hpp
 * @author  Typheye
 * @brief   Tos interface.
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

#ifndef CORE_TOS_HPP
#define CORE_TOS_HPP

// ── Hardware headers ──────────────────────────────────────────
#include "include/bmp180.hpp"
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
#include "include/pot.hpp"
#include "include/sn74hc00n.hpp"
#include "include/sysui.hpp"
#include "include/tcs3472.hpp"
#include "include/thid.hpp"
#include "include/trtc.hpp"
#include "include/tsdio.hpp"

extern "C" {
#include "include/lib3dox.h"
}

#include "main.h"
#include "stm32f4xx_hal.h"

#include <cstdint>
#include <math.h>
#include <stdio.h>
#include <string.h>

// ── Demo type ─────────────────────────────────────────────────
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

// ── TOS class ─────────────────────────────────────────────────
class TOS {
public:
  TOS();
  ~TOS();

  void init();  // 初始化系统 (implemented in core/init.cpp)
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

#endif // CORE_TOS_HPP
