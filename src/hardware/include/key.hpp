#ifndef __KEY_HPP
#define __KEY_HPP

#include "main.h"
#include "stm32f4xx_hal.h"

typedef enum { SW_OFF = 0, SW_ON = 1 } SwitchState_t;
typedef enum {
  KEY_IDLE = 0,
  KEY_PRESSED = 1,
  KEY_RELEASED = 2,
  KEY_LONG_PRESS = 3
} KeyState_t;

// 单个开关类
class Switch {
public:
  Switch(GPIO_TypeDef *port, uint16_t pin, bool inverted = false);
  void init(void);
  bool isOn(void);  // 返回 true = 开关打开
  bool isOff(void); // 返回 true = 开关关闭
private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _inverted; // true: RESET=ON, false: SET=ON
  bool _initialized;
};

// 按键类（碰撞开关、普通按键都用这个）
class Key {
public:
  Key(GPIO_TypeDef *port, uint16_t pin, bool inverted = false);
  void init(void);
  bool isPressed(void);
  bool isReleased(void);
  KeyState_t getState(void);
  void tick(void); // 需要在主循环中调用
  void setLongPressTime(uint32_t ms);

private:
  GPIO_TypeDef *_port;
  uint16_t _pin;
  bool _inverted; // true: SET=按下, false: RESET=按下
  bool _last_state;
  bool _long_press_triggered;
  uint32_t _press_start_time;
  uint32_t _long_press_time;
  bool _initialized;
};

// 按键管理器（直接暴露所有开关和按键）
class KeyManager {
public:
  void init(void);
  void tick(void);

  // 组1 (正常逻辑)
  Switch sw1_E0{GPIOE, GPIO_PIN_0, false};
  Switch sw2_G13{GPIOG, GPIO_PIN_13, false};
  Switch sw3_E2{GPIOE, GPIO_PIN_2, false};
  Switch sw4_E4{GPIOE, GPIO_PIN_4, false};

  // 组2 (逻辑相反 - 开关接地)
  Switch sw5_D6{GPIOD, GPIO_PIN_6, true};
  Switch sw6_G9{GPIOG, GPIO_PIN_9, true};
  Switch sw7_G11{GPIOG, GPIO_PIN_11, true};
  Switch sw8_G10{GPIOG, GPIO_PIN_10, true};
  Switch sw9_G15{GPIOG, GPIO_PIN_15, true};

  // 组3 (正常逻辑)
  Switch sw10_G3{GPIOG, GPIO_PIN_3, false};
  Switch sw11_D15{GPIOD, GPIO_PIN_15, false};
  Switch sw12_B12{GPIOB, GPIO_PIN_12, false};
  Switch sw13_B14{GPIOB, GPIO_PIN_14, false};

  // 组4 (逻辑相反)
  Switch sw_sd_detect{GPIOG, GPIO_PIN_6, true};   // G6  = SD card detect
  Switch sw_mute{GPIOG, GPIO_PIN_8, true};        // G8  = mute switch

  // 碰撞开关 (逻辑相反)
  Key collision_A8{GPIOA, GPIO_PIN_8, true};
  Key collision_D0{GPIOD, GPIO_PIN_0, true};

  // 标准按键 (正常逻辑)
  Key btn_enter{GPIOD, GPIO_PIN_10, false};

  // 便捷方法
  uint8_t getGroup1Config(void);
  uint8_t getGroup2Config(void);
  uint8_t getGroup3Config(void);
  bool isSdCardInserted(void);
  bool isMuted(void);
  bool isAnyCollision(void);
};

extern KeyManager keyManager;

#endif