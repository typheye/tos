/**
 ******************************************************************************
 * @file    key.hpp
 * @author  Typheye
 * @brief   Key interface.
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

#ifndef KEY_HPP
#define KEY_HPP

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
  bool _lastState;
  bool _longPressTriggered;
  uint32_t _pressStartTime;
  uint32_t _longPressTime;
  uint32_t _lastSampleTime;
  uint8_t _debounce;
  KeyState_t _event;
  bool _initialized;
};


class KeyManager {
public:
  void init(void);
  void tick(void);


  Switch _sw1E0{GPIOE, GPIO_PIN_0, false};
  Switch _sw2G13{GPIOG, GPIO_PIN_13, false};
  Switch _sw3E2{GPIOE, GPIO_PIN_2, false};
  Switch _sw4E4{GPIOE, GPIO_PIN_4, false};


  Switch _sw5D6{GPIOD, GPIO_PIN_6, true};
  Switch _sw6G9{GPIOG, GPIO_PIN_9, true};
  Switch _sw7G11{GPIOG, GPIO_PIN_11, true};
  Switch _sw8G10{GPIOG, GPIO_PIN_10, true};
  Switch _sw9G15{GPIOG, GPIO_PIN_15, true};


  Switch _sw10G3{GPIOG, GPIO_PIN_3, false};
  Switch _sw11D15{GPIOD, GPIO_PIN_15, false};
  Switch _sw12B12{GPIOB, GPIO_PIN_12, false};
  Switch _sw13B14{GPIOB, GPIO_PIN_14, false};


  Switch _swSdDetect{GPIOG, GPIO_PIN_6, true};   // G6  = SD card detect
  Switch _swMute{GPIOG, GPIO_PIN_8, true};        // G8  = mute switch


  Key _collisionA8{GPIOA, GPIO_PIN_8, true};
  Key _collisionD0{GPIOD, GPIO_PIN_0, true};


  Key _btnEnter{GPIOD, GPIO_PIN_10, false};


  uint8_t getGroup1Config(void);
  uint8_t getGroup2Config(void);
  uint8_t getGroup3Config(void);
  bool isSdCardInserted(void);
  bool isMuted(void);
  bool isAnyCollision(void);
};

extern KeyManager keyManager;

#endif /* KEY_HPP */
