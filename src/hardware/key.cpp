#include "include/key.hpp"

// ==================== Switch 实现 ====================

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
    return state == GPIO_PIN_RESET; // 反转：低电平=ON
  } else {
    return state == GPIO_PIN_SET; // 正常：高电平=ON
  }
}

bool Switch::isOff(void) { return !isOn(); }

// ==================== Key 实现 ====================

Key::Key(GPIO_TypeDef *port, uint16_t pin, bool inverted)
    : _port(port), _pin(pin), _inverted(inverted), _last_state(false),
      _long_press_triggered(false), _press_start_time(0), _long_press_time(500),
      _initialized(false) {}

void Key::init(void) {
  if (_initialized)
    return;
  _last_state = isPressed();
  _initialized = true;
}

bool Key::isPressed(void) {
  if (!_initialized)
    return false;
  GPIO_PinState state = HAL_GPIO_ReadPin(_port, _pin);
  if (_inverted) {
    return state == GPIO_PIN_SET; // 反转：高电平=按下
  } else {
    return state == GPIO_PIN_RESET; // 正常：低电平=按下
  }
}

bool Key::isReleased(void) { return !isPressed(); }

KeyState_t Key::getState(void) {
  if (!_initialized)
    return KEY_IDLE;

  bool current = isPressed();
  KeyState_t result = KEY_IDLE;

  if (current && !_last_state) {
    _press_start_time = HAL_GetTick();
    _long_press_triggered = false;
    result = KEY_PRESSED;
  } else if (!current && _last_state) {
    if (_long_press_triggered) {
      result = KEY_LONG_PRESS;
      _long_press_triggered = false;
    } else {
      result = KEY_RELEASED;
    }
  }

  _last_state = current;
  return result;
}

void Key::tick(void) {
  if (!_initialized)
    return;

  if (isPressed() && !_long_press_triggered) {
    if (HAL_GetTick() - _press_start_time >= _long_press_time) {
      _long_press_triggered = true;
    }
  }
}

void Key::setLongPressTime(uint32_t ms) { _long_press_time = ms; }

// ==================== KeyManager 实现 ====================

KeyManager keyManager;

void KeyManager::init(void) {
  // 初始化所有开关
  sw1_E0.init();
  sw2_G13.init();
  sw3_E2.init();
  sw4_E4.init();
  sw5_D6.init();
  sw6_G9.init();
  sw7_G11.init();
  sw8_G10.init();
  sw9_G15.init();
  sw10_G3.init();
  sw11_D15.init();
  sw12_B12.init();
  sw13_B14.init();
  sw_sd_detect.init();
  sw_mute.init();

  // 初始化按键
  collision_A8.init();
  collision_D0.init();
  btn_enter.init();
}

void KeyManager::tick(void) {
  collision_A8.tick();
  collision_D0.tick();
  btn_enter.tick();
}

uint8_t KeyManager::getGroup1Config(void) {
  uint8_t val = 0;
  if (sw1_E0.isOn())
    val |= 0x01;
  if (sw2_G13.isOn())
    val |= 0x02;
  if (sw3_E2.isOn())
    val |= 0x04;
  if (sw4_E4.isOn())
    val |= 0x08;
  return val;
}

uint8_t KeyManager::getGroup2Config(void) {
  uint8_t val = 0;
  if (sw5_D6.isOn())
    val |= 0x01;
  if (sw6_G9.isOn())
    val |= 0x02;
  if (sw7_G11.isOn())
    val |= 0x04;
  if (sw8_G10.isOn())
    val |= 0x08;
  if (sw9_G15.isOn())
    val |= 0x10;
  return val;
}

uint8_t KeyManager::getGroup3Config(void) {
  uint8_t val = 0;
  if (sw10_G3.isOn())
    val |= 0x01;
  if (sw11_D15.isOn())
    val |= 0x02;
  if (sw12_B12.isOn())
    val |= 0x04;
  if (sw13_B14.isOn())
    val |= 0x08;
  return val;
}

bool KeyManager::isSdCardInserted(void) { return sw_sd_detect.isOn(); }
bool KeyManager::isMuted(void) { return sw_mute.isOn(); }
bool KeyManager::isAnyCollision(void) {
  return collision_A8.isPressed() || collision_D0.isPressed();
}