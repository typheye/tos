/**
 ******************************************************************************
 * @file    buzzer.hpp
 * @author  Typheye
 * @brief   Buzzer interface.
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
    JPDelay(100);
    beep(100);
  }
  void beepSuccess() {
    beep(50);
    JPDelay(50);
    beep(50);
    JPDelay(50);
    beep(50);
  }

private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _initialized;
};

extern Buzzer buzzer1;

#endif