/**
 ******************************************************************************
 * @file    key.cpp
 * @author  Typheye
 * @brief   Key implementation.
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

#include "include/key.hpp"




Switch::Switch(GPIO_TypeDef *port, uint16_t pin, bool inverted)
    : _port(port), _pin(pin), _inverted(inverted), _initialized(false) {}

void Switch::init(void) {
  if (_initialized)
    return;
  _initialized = true;
}

bool Switch::isOn(void) {
  if (!_initialized)
    return false;
  GPIO_PinState state = HAL_GPIO_ReadPin(_port, _pin);
  if (_inverted) {
    return state == GPIO_PIN_RESET;
  } else {
    return state == GPIO_PIN_SET;
  }
}

bool Switch::isOff(void) { return !isOn(); }



Key::Key(GPIO_TypeDef *port, uint16_t pin, bool inverted)
    : _port(port), _pin(pin), _inverted(inverted), _lastState(false),
      _longPressTriggered(false), _pressStartTime(0), _longPressTime(500),
      _lastSampleTime(0), _debounce(0), _event(KEY_IDLE), _initialized(false) {}

void Key::init(void) {
  if (_initialized)
    return;
  _initialized = true;
  bool raw = rawPressed();
  _lastState = raw;
  _debounce = raw ? KEY_DEBOUNCE_CNT : 0;
  _pressStartTime = HAL_GetTick();
  _lastSampleTime = _pressStartTime;
  _longPressTriggered = false;
  _event = KEY_IDLE;
}

bool Key::rawPressed(void) const {
  GPIO_PinState state = HAL_GPIO_ReadPin(_port, _pin);
  if (_inverted) {
    return state == GPIO_PIN_SET;
  } else {
    return state == GPIO_PIN_RESET;
  }
}

bool Key::isPressed(void) {
  if (!_initialized)
    return false;
  return rawPressed();
}

bool Key::isReleased(void) { return !isPressed(); }

KeyState_t Key::getState(void) {
  if (!_initialized)
    return KEY_IDLE;

  tick();
  KeyState_t latched = _event;
  _event = KEY_IDLE;
  return latched;
}

void Key::latchEvent(KeyState_t event) {
  if (event == KEY_IDLE)
    return;

  if (_event == KEY_IDLE) {
    _event = event;
  } else if (_event == KEY_PRESSED) {
    return;
  } else if (event == KEY_LONG_PRESS) {
    _event = event;
  } else if (_event == KEY_RELEASED && event == KEY_PRESSED) {
    _event = event;
  }
}

void Key::tick(void) {
  if (!_initialized)
    return;

  uint32_t now = HAL_GetTick();
  if (now - _lastSampleTime < KEY_SCAN_INTERVAL_MS) {
    if (_lastState && !_longPressTriggered &&
        now - _pressStartTime >= _longPressTime) {
      _longPressTriggered = true;
    }
    return;
  }
  _lastSampleTime = now;

  bool raw = rawPressed();
  if (raw) {
    if (_debounce < KEY_DEBOUNCE_CNT)
      _debounce++;
  } else {
    if (_debounce > 0)
      _debounce--;
  }

  bool stable = (_debounce >= KEY_DEBOUNCE_CNT);
  if (stable && !_lastState) {
    _pressStartTime = now;
    _longPressTriggered = false;
    latchEvent(KEY_PRESSED);
  } else if (!stable && _lastState) {
    if (_longPressTriggered) {
      latchEvent(KEY_LONG_PRESS);
      _longPressTriggered = false;
    } else {
      latchEvent(KEY_RELEASED);
    }
  }
  _lastState = stable;

  if (_lastState && !_longPressTriggered &&
      now - _pressStartTime >= _longPressTime) {
    _longPressTriggered = true;
  }
}

void Key::setLongPressTime(uint32_t ms) { _longPressTime = ms; }



KeyManager keyManager;


void lcd_dma_yield(void) {
  TosApi_Tick();
  keyManager._collisionA8.tick();
  keyManager._collisionD0.tick();
  keyManager._btnEnter.tick();
}

void KeyManager::init(void) {

  _sw1E0.init();
  _sw2G13.init();
  _sw3E2.init();
  _sw4E4.init();
  _sw5D6.init();
  _sw6G9.init();
  _sw7G11.init();
  _sw8G10.init();
  _sw9G15.init();
  _sw10G3.init();
  _sw11D15.init();
  _sw12B12.init();
  _sw13B14.init();
  _swSdDetect.init();
  _swMute.init();


  _collisionA8.init();
  _collisionD0.init();
  _btnEnter.init();
}

void KeyManager::tick(void) {
  _collisionA8.tick();
  _collisionD0.tick();
  _btnEnter.tick();
}

uint8_t KeyManager::getGroup1Config(void) {
  uint8_t val = 0;
  if (_sw1E0.isOn())
    val |= 0x01;
  if (_sw2G13.isOn())
    val |= 0x02;
  if (_sw3E2.isOn())
    val |= 0x04;
  if (_sw4E4.isOn())
    val |= 0x08;
  return val;
}

uint8_t KeyManager::getGroup2Config(void) {
  uint8_t val = 0;
  if (_sw5D6.isOn())
    val |= 0x01;
  if (_sw6G9.isOn())
    val |= 0x02;
  if (_sw7G11.isOn())
    val |= 0x04;
  if (_sw8G10.isOn())
    val |= 0x08;
  if (_sw9G15.isOn())
    val |= 0x10;
  return val;
}

uint8_t KeyManager::getGroup3Config(void) {
  uint8_t val = 0;
  if (_sw10G3.isOn())
    val |= 0x01;
  if (_sw11D15.isOn())
    val |= 0x02;
  if (_sw12B12.isOn())
    val |= 0x04;
  if (_sw13B14.isOn())
    val |= 0x08;
  return val;
}

bool KeyManager::isSdCardInserted(void) { return _swSdDetect.isOn(); }
bool KeyManager::isMuted(void) { return _swMute.isOn(); }
bool KeyManager::isAnyCollision(void) {
  return _collisionA8.isPressed() || _collisionD0.isPressed();
}
