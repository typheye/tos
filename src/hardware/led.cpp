#include "include/led.hpp"
#include <stdio.h>

LED::LED(GPIO_TypeDef *port, uint16_t pin, bool polarity)
    : _port(port), _pin(pin), _polarity(polarity) {}

void LED::init(void) { off(); }

void LED::on(void) {
  if (_polarity) {
    // 高电平点亮
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  } else {
    // 低电平点亮
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  }
}

void LED::off(void) {
  if (_polarity) {
    // 高电平点亮 → 熄灭需要低电平
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
  } else {
    // 低电平点亮 → 熄灭需要高电平
    HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
  }
}

void LED::toggle(void) { HAL_GPIO_TogglePin(_port, _pin); }

void LED::blink(uint32_t delay_ms) {
  on();
  HAL_Delay(delay_ms);
  off();
  HAL_Delay(delay_ms);
}

// PC13: 低电平点亮（polarity = false）
// PD8/PD9: 高电平点亮（polarity = true）
LED boardLed(GPIOC, GPIO_PIN_13, false);
LED warnLed(GPIOD, GPIO_PIN_8, true);
LED errorLed(GPIOD, GPIO_PIN_9, true);