/**
 ******************************************************************************
 * @file    led.hpp
 * @author  Typheye
 * @brief   Led interface.
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

#ifndef LED_HPP
#define LED_HPP

#include "main.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C-compatible wrappers for cross-language access (e.g. from syshandle.c) */
void LED_ErrorOn(void);
void LED_ErrorOff(void);
void LED_BoardOn(void);
void LED_BoardOff(void);
void LED_BoardBlink100ms(void);
void LED_BoardBlink50ms(void);
void LED_WarnOn(void);
void LED_WarnOff(void);
/* Non-blocking 300ms warning pulse for recoverable network/time failures. */
void LED_WarnBlink300ms(void);
void LED_EspCommSuccess(void);
void LED_EspCommFailure(void);
void LED_ServiceTick(void);

#ifdef __cplusplus
}
#endif

class LED {
public:

  LED(GPIO_TypeDef *port, uint16_t pin, bool polarity = true);

  void init(void);
  void on(void);
  void off(void);
  void toggle(void);
  void blink(uint32_t delay_ms);

private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _polarity;
};

extern LED boardLed;
extern LED warnLed;
extern LED errorLed;

#endif
