/**
 ******************************************************************************
 * @file    led.cpp
 * @author  Typheye
 * @brief   Led implementation.
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

#include "include/led.hpp"


static uint8_t g_warn_pulse_active = 0;
static uint32_t g_warn_pulse_until_ms = 0;

static bool tick_due(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

LED::LED(GPIO_TypeDef *port, uint16_t pin, bool polarity)
    : _port(port), _pin(pin), _polarity(polarity) {}

void LED::init(void) { off(); }

void LED::on(void) {
  if (_polarity) {
    
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  } else {
    
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  }
}

void LED::off(void) {
  if (_polarity) {
    
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  } else {
    
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  }
}

void LED::toggle(void) { HAL_GPIO_TogglePin(_port, _pin); }

void LED::blink(uint32_t delay_ms) {
  on();
  HAL_Delay(delay_ms);
  off();
  HAL_Delay(delay_ms);
}



void LED_ErrorOn(void) { errorLed.on(); }
void LED_ErrorOff(void) { errorLed.off(); }
void LED_BoardOn(void) { boardLed.on(); }
void LED_BoardOff(void) { boardLed.off(); }
void LED_BoardBlink100ms(void) {
  boardLed.on();
  HAL_Delay(100);
  boardLed.off();
}
void LED_WarnOn(void) {
  g_warn_pulse_active = 0;
  warnLed.on();
}
void LED_WarnOff(void) {
  g_warn_pulse_active = 0;
  warnLed.off();
}
void LED_WarnBlink300ms(void) {
  warnLed.on();
  g_warn_pulse_until_ms = HAL_GetTick() + 300U;
  g_warn_pulse_active = 1;
}
void LED_ServiceTick(void) {
  if (g_warn_pulse_active && tick_due(HAL_GetTick(), g_warn_pulse_until_ms)) {
    g_warn_pulse_active = 0;
    warnLed.off();
  }
}



LED boardLed(GPIOC, GPIO_PIN_13, false);
LED warnLed(GPIOD, GPIO_PIN_8, true);
LED errorLed(GPIOD, GPIO_PIN_9, true);