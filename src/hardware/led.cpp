#include "include/led.hpp"

// 构造函数
LED::LED(GPIO_TypeDef *port, uint16_t pin) : _port(port), _pin(pin) {

  // 初始化GPIO
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 使能 GPIO 时钟（需要根据端口判断）
  if (_port == GPIOA) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  } else if (_port == GPIOB) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  } else if (_port == GPIOC) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  } else if (_port == GPIOD) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  } // 根据需要添加其他端口

  // 配置为推挽输出
  GPIO_InitStruct.Pin = _pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(_port, &GPIO_InitStruct);

  // 默认关闭LED（假设低电平点亮）
  off();
}

void LED::on() {
  // 假设低电平点亮
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET);
}

void LED::off() {
  // 假设高电平熄灭
  HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET);
}

void LED::toggle() { HAL_GPIO_TogglePin(_port, _pin); }

void LED::blink(uint32_t delay_ms) {
  on();
  HAL_Delay(delay_ms);
  off();
  HAL_Delay(delay_ms);
}

// 定义板载LED（PC13）
LED boardLed(GPIOC, GPIO_PIN_13);