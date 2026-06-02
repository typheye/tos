#ifndef __LED_HPP
#define __LED_HPP

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C-compatible wrappers for cross-language access (e.g. from syshandle.c) */
void LED_ErrorOn(void);
void LED_ErrorOff(void);
void LED_BoardOn(void);
void LED_BoardOff(void);
void LED_BoardBlink100ms(void);

#ifdef __cplusplus
}
#endif

class LED {
public:
  // polarity: true = 高电平点亮, false = 低电平点亮
  LED(GPIO_TypeDef *port, uint16_t pin, bool polarity = true);

  void init(void);
  void on(void);
  void off(void);
  void toggle(void);
  void blink(uint32_t delay_ms);

private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _polarity; // true: 高电平点亮, false: 低电平点亮
};

extern LED boardLed;
extern LED warnLed;
extern LED errorLed;

#endif