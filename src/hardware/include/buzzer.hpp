/**
 ******************************************************************************
 * @file    buzzer.hpp
 * @author  Typheye
 * @brief   Buzzer interface.
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

#ifndef BUZZER_HPP
#define BUZZER_HPP

#include "include/libdly.h"
#include "main.h"

// 蜂鸣器引脚定义
#define BUZZER1_PORT GPIOG
#define BUZZER1_PIN GPIO_PIN_5

// 放音开关引脚定义
#define BUZZER_ENABLE_PORT GPIOG
#define BUZZER_ENABLE_PIN GPIO_PIN_8

class Buzzer {
public:
  Buzzer(GPIO_TypeDef *port, uint16_t pin);

  void init();

  // 检查放音开关是否打开
  bool isEnabled();

  // 简单鸣叫（会检查开关）
  void beep(uint32_t duration_ms);

  // 强制鸣叫（不检查开关，用于测试）
  void forceBeep(uint32_t duration_ms);

  // 常用预设（会自动检查开关）
  void beepShort() { beep(50); }
  void beepLong() { beep(200); }
  void beepError() {
    beep(100);
    HAL_Delay(100);
    beep(100);
  }
  void beepSuccess() {
    beep(50);
    HAL_Delay(50);
    beep(50);
    HAL_Delay(50);
    beep(50);
  }

private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _initialized;
};

extern Buzzer buzzer1;

#endif