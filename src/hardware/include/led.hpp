#ifndef LED_HPP
#define LED_HPP

#include "main.h" // 需要 HAL 库定义

class LED {
public:
  // 构造函数：指定端口和引脚
  LED(GPIO_TypeDef *port, uint16_t pin);

  // 禁止拷贝
  LED(const LED &) = delete;
  LED &operator=(const LED &) = delete;

  // 基本操作
  void on();
  void off();
  void toggle();

  // 带延时的闪烁
  void blink(uint32_t delay_ms);

private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
};

// 为了方便，可以预定义一些常用的LED
// 例如板载LED通常在 PC13
extern LED boardLed;

#endif // LED_HPP