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


bool Buzzer::isEnabled() {
  
  return HAL_GPIO_ReadPin(BUZZER_ENABLE_PORT, BUZZER_ENABLE_PIN) ==
         GPIO_PIN_RESET;
}

void Buzzer::init() {
  if (_initialized) {
    return;
  }

  
  
  //   HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  _initialized = true;

  
  if (isEnabled()) {
    beep(50);
    JPDelay(100);
    beep(50);
    JPDelay(100);
  }
}


void Buzzer::beep(uint32_t duration_ms) {
  if (!_initialized) {
    
    init();
    if (!_initialized)
      return;
  }

  
  if (!isEnabled()) {
    return;
  }

  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);   
  HAL_Delay(duration_ms);                         
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET); 
}


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


Buzzer buzzer1(BUZZER1_PORT, BUZZER1_PIN);