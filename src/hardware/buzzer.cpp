/**
 ******************************************************************************
 * @file    buzzer.cpp
 * @author  Typheye
 * @brief   Buzzer implementation.
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

#include "include/buzzer.hpp"

Buzzer::Buzzer(GPIO_TypeDef *port, uint16_t pin)
    : _port(port), _pin(pin), _initialized(false) {}

// 检查放音开关是否打开
bool Buzzer::isEnabled() {
  // PG8 已配置为输入+上拉，开关闭合时读到低电平
  return HAL_GPIO_ReadPin(BUZZER_ENABLE_PORT, BUZZER_ENABLE_PIN) ==
         GPIO_PIN_RESET;
}

void Buzzer::init() {
  if (_initialized) {
    return;
  }

  // ⚠️ 关键：不要重新初始化 GPIO！CubeMX 已经做好了
  // 只关闭蜂鸣器，不修改引脚配置
  //   HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  _initialized = true;

  // 测试两声短鸣（只有开关打开时才响）
  if (isEnabled()) {
    beep(50);
    JPDelay(100);
    beep(50);
    JPDelay(100);
  }
}

// 简单的蜂鸣器鸣叫（会检查开关）
void Buzzer::beep(uint32_t duration_ms) {
  if (!_initialized) {
    // 如果未初始化，尝试初始化
    init();
    if (!_initialized)
      return;
  }

  // 检查放音开关是否打开
  if (!isEnabled()) {
    return;
  }

  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);   // 打开
  HAL_Delay(duration_ms);                         // 保持
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET); // 关闭
}

// 强制鸣叫（不检查开关，用于测试）
void Buzzer::forceBeep(uint32_t duration_ms) {
  if (!_initialized) {
    init();
    if (!_initialized)
      return;
  }

  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  HAL_Delay(duration_ms);
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
}

// 定义 buzzer1 变量（原来已经有这行就不要重复）
Buzzer buzzer1(BUZZER1_PORT, BUZZER1_PIN);