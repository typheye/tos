/**
 ******************************************************************************
 * @file    key.hpp
 * @author  Typheye
 * @brief   Key interface.
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

#ifndef __KEY_HPP
#define __KEY_HPP

#include "main.h"
#include "stm32f4xx_hal.h"
#include "core/sdk/include/tos_api.h"

typedef enum { SW_OFF = 0, SW_ON = 1 } SwitchState_t;
typedef enum {
  KEY_IDLE = 0,
  KEY_PRESSED = 1,
  KEY_RELEASED = 2,
  KEY_LONG_PRESS = 3
} KeyState_t;


class Switch {
public:
  Switch(GPIO_TypeDef *port, uint16_t pin, bool inverted = false);
  void init(void);
  bool isOn(void);  
  bool isOff(void); 
private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _inverted; // true: RESET=ON, false: SET=ON
  bool _initialized;
};

#define KEY_DEBOUNCE_CNT 8


#define KEY_SCAN_INTERVAL_MS 1U

class Key {
public:
  Key(GPIO_TypeDef *port, uint16_t pin, bool inverted = false);
  void init(void);
  bool isPressed(void);
  bool isReleased(void);
  KeyState_t getState(void);
  void tick(void); 
  void setLongPressTime(uint32_t ms);

private:
  bool rawPressed(void) const;
  void latchEvent(KeyState_t event);

  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _inverted; 
  bool _last_state;
  bool _long_press_triggered;
  uint32_t _press_start_time;
  uint32_t _long_press_time;
  uint32_t _last_sample_time;
  uint8_t _debounce;     
  KeyState_t _event;
  bool _initialized;
};


class KeyManager {
public:
  void init(void);
  void tick(void);

  
  Switch sw1_E0{GPIOE, GPIO_PIN_0, false};
  Switch sw2_G13{GPIOG, GPIO_PIN_13, false};
  Switch sw3_E2{GPIOE, GPIO_PIN_2, false};
  Switch sw4_E4{GPIOE, GPIO_PIN_4, false};

  
  Switch sw5_D6{GPIOD, GPIO_PIN_6, true};
  Switch sw6_G9{GPIOG, GPIO_PIN_9, true};
  Switch sw7_G11{GPIOG, GPIO_PIN_11, true};
  Switch sw8_G10{GPIOG, GPIO_PIN_10, true};
  Switch sw9_G15{GPIOG, GPIO_PIN_15, true};

  
  Switch sw10_G3{GPIOG, GPIO_PIN_3, false};
  Switch sw11_D15{GPIOD, GPIO_PIN_15, false};
  Switch sw12_B12{GPIOB, GPIO_PIN_12, false};
  Switch sw13_B14{GPIOB, GPIO_PIN_14, false};

  
  Switch sw_sd_detect{GPIOG, GPIO_PIN_6, true};   // G6  = SD card detect
  Switch sw_mute{GPIOG, GPIO_PIN_8, true};        // G8  = mute switch

  
  Key collision_A8{GPIOA, GPIO_PIN_8, true};
  Key collision_D0{GPIOD, GPIO_PIN_0, true};

  
  Key btn_enter{GPIOD, GPIO_PIN_10, false};

  
  uint8_t getGroup1Config(void);
  uint8_t getGroup2Config(void);
  uint8_t getGroup3Config(void);
  bool isSdCardInserted(void);
  bool isMuted(void);
  bool isAnyCollision(void);
};

extern KeyManager keyManager;

#endif
