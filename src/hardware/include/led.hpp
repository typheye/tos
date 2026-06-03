/**
 ******************************************************************************
 * @file    led.hpp
 * @author  Typheye
 * @brief   Led interface.
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

#ifndef __LED_HPP
#define __LED_HPP

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
void LED_WarnOn(void);
void LED_WarnOff(void);
/* Non-blocking 300 ms warning pulse. The actual off edge is serviced from
 * SysWatchdog_Tick(), so network/UI paths never block for LED feedback. */
void LED_WarnBlink300ms(void);
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