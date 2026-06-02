#include "include/key.hpp"
#include "core/sdk/include/tos_api.h"

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
      _last_sample_time(0), _debounce(0), _event(KEY_IDLE), _initialized(false) {}

void Key::init(void) {
  if (_initialized)
    return;
  _initialized = true;
  bool raw = rawPressed();
  _last_state = raw;
  _debounce = raw ? KEY_DEBOUNCE_CNT : 0;
  _press_start_time = HAL_GetTick();
  _last_sample_time = _press_start_time;
  _long_press_triggered = false;
  _event = KEY_IDLE;
}

bool Key::rawPressed(void) const {
  GPIO_PinState state = HAL_GPIO_ReadPin(_port, _pin);
  if (_inverted) {
    return state == GPIO_PIN_SET; // 反转：高电平=按下
  } else {
    return state == GPIO_PIN_RESET; // 正常：低电平=按下
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
  if (now - _last_sample_time < KEY_SCAN_INTERVAL_MS) {
    if (_last_state && !_long_press_triggered &&
        now - _press_start_time >= _long_press_time) {
      _long_press_triggered = true;
    }
    return;
  }
  _last_sample_time = now;

  bool raw = rawPressed();
  if (raw) {
    if (_debounce < KEY_DEBOUNCE_CNT)
      _debounce++;
  } else {
    if (_debounce > 0)
      _debounce--;
  }

  bool stable = (_debounce >= KEY_DEBOUNCE_CNT);
  if (stable && !_last_state) {
    _press_start_time = now;
    _long_press_triggered = false;
    latchEvent(KEY_PRESSED);
  } else if (!stable && _last_state) {
    if (_long_press_triggered) {
      latchEvent(KEY_LONG_PRESS);
      _long_press_triggered = false;
    } else {
      latchEvent(KEY_RELEASED);
    }
  }
  _last_state = stable;

  if (_last_state && !_long_press_triggered &&
      now - _press_start_time >= _long_press_time) {
    _long_press_triggered = true;
  }
}

void Key::setLongPressTime(uint32_t ms) { _long_press_time = ms; }

// ==================== KeyManager 实现 ====================

KeyManager keyManager;

// 覆盖弱符号：DMA 等待期间轮询按键，消除盲窗
void lcd_dma_yield(void) {
  TosApi_Tick();
  keyManager.collision_A8.tick();
  keyManager.collision_D0.tick();
  keyManager.btn_enter.tick();
}

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
