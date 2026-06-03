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


#define BUZZER1_PORT GPIOG
#define BUZZER1_PIN GPIO_PIN_5


#define BUZZER_ENABLE_PORT GPIOG
#define BUZZER_ENABLE_PIN GPIO_PIN_8

class Buzzer {
public:
  Buzzer(GPIO_TypeDef *port, uint16_t pin);

  void init();

  
  bool isEnabled();

  
  void beep(uint32_t duration_ms);

  
  void forceBeep(uint32_t duration_ms);

  
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