/**
 ******************************************************************************
 * @file    led.cpp
 * @author  Typheye
 * @brief   Led implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "include/led.hpp"
#include "library/include/libdly.h"


static uint8_t g_board_pulse_active = 0;
static uint32_t g_board_pulse_until_ms = 0;
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
  JPDelay(delay_ms);
  off();
  JPDelay(delay_ms);
}



void LED_ErrorOn(void) { errorLed.on(); }
void LED_ErrorOff(void) { errorLed.off(); }
void LED_BoardOn(void) {
  LED_BoardBlink50ms();
}
void LED_BoardOff(void) {
  g_board_pulse_active = 0;
  boardLed.off();
}
void LED_BoardBlink100ms(void) {
  LED_BoardBlink50ms();
}
void LED_BoardBlink50ms(void) {
  g_warn_pulse_active = 0;
  warnLed.off();
  boardLed.on();
  g_board_pulse_until_ms = HAL_GetTick() + 50U;
  g_board_pulse_active = 1;
}
void LED_WarnOn(void) {
  g_warn_pulse_active = 0;
  g_board_pulse_active = 0;
  boardLed.off();
  warnLed.on();
}
void LED_WarnOff(void) {
  g_warn_pulse_active = 0;
  warnLed.off();
}
void LED_WarnBlink300ms(void) {
  g_board_pulse_active = 0;
  boardLed.off();
  warnLed.on();
  g_warn_pulse_until_ms = HAL_GetTick() + 300U;
  g_warn_pulse_active = 1;
}
void LED_EspCommSuccess(void) {
  LED_BoardBlink50ms();
}
void LED_EspCommFailure(void) {
  LED_WarnBlink300ms();
}
void LED_ServiceTick(void) {
  if (g_board_pulse_active && tick_due(HAL_GetTick(), g_board_pulse_until_ms)) {
    g_board_pulse_active = 0;
    boardLed.off();
  }
  if (g_warn_pulse_active && tick_due(HAL_GetTick(), g_warn_pulse_until_ms)) {
    g_warn_pulse_active = 0;
    warnLed.off();
  }
}



LED boardLed(GPIOC, GPIO_PIN_13, false);
LED warnLed(GPIOD, GPIO_PIN_8, true);
LED errorLed(GPIOD, GPIO_PIN_9, true);
