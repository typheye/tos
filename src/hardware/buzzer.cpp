/**
 ******************************************************************************
 * @file    buzzer.cpp
 * @author  Typheye
 * @brief   Buzzer implementation.
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

#include "include/buzzer.hpp"
#include "core/manager/include/settings_manager.h"
#include "library/include/libdly.h"


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


  if (SM_BootGfx() && isEnabled()) {
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
  JPDelay(duration_ms);
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
}


void Buzzer::forceBeep(uint32_t duration_ms) {
  if (!_initialized) {
    init();
    if (!_initialized)
      return;
  }

  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  JPDelay(duration_ms);
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
}


Buzzer buzzer1(BUZZER1_PORT, BUZZER1_PIN);
