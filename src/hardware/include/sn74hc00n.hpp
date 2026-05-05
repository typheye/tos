#ifndef __SN74HC00N_HPP
#define __SN74HC00N_HPP

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 引脚定义 ====================
// HC00N 的 4 个输出引脚 (1Y, 2Y, 3Y, 4Y) 连接到 MCU
#define HC00N_OUT1_PORT GPIOE
#define HC00N_OUT1_PIN GPIO_PIN_13 // 1Y

#define HC00N_OUT2_PORT GPIOE
#define HC00N_OUT2_PIN GPIO_PIN_12 // 2Y

#define HC00N_OUT3_PORT GPIOE
#define HC00N_OUT3_PIN GPIO_PIN_11 // 3Y

#define HC00N_OUT4_PORT GPIOE
#define HC00N_OUT4_PIN GPIO_PIN_10 // 4Y

// ==================== 逻辑电平定义 ====================
#define HC00N_HIGH 1
#define HC00N_LOW 0

// ==================== 开关输入组合 ====================
#define SW1A_MASK 0x01
#define SW1B_MASK 0x02
#define SW2A_MASK 0x04
#define SW2B_MASK 0x08
#define SW3A_MASK 0x10
#define SW3B_MASK 0x20
#define SW4A_MASK 0x40
#define SW4B_MASK 0x80

// ==================== 数据结构 ====================
typedef struct {
  uint8_t sw1A;
  uint8_t sw1B;
  uint8_t sw2A;
  uint8_t sw2B;
  uint8_t sw3A;
  uint8_t sw3B;
  uint8_t sw4A;
  uint8_t sw4B;
} HC00N_Switches_t;

typedef struct {
  uint8_t output1;
  uint8_t output2;
  uint8_t output3;
  uint8_t output4;
} HC00N_Outputs_t;

// ==================== 类接口 ====================
class SN74HC00N {
public:
  SN74HC00N();

  // 初始化
  void init(void);
  bool isInitialized(void) { return _initialized; }

  // 读取所有 4 个输出引脚状态
  uint8_t readOutputByte(void);
  HC00N_Outputs_t readOutputs(void);
  uint8_t readOutput(uint8_t channel);

  // 静态 NAND 逻辑函数 (可以在外部直接调用)
  static uint8_t nandGate(uint8_t a, uint8_t b);
  static uint8_t calculateNANDOutput(uint8_t switches);

  // 更新状态
  void updateFromSwitches(uint8_t switchStates);

  // 获取状态
  HC00N_Switches_t getSwitchStates(void);
  HC00N_Outputs_t getActualOutputs(void);

  // 验证
  bool verifyOutputs(uint8_t switches);

  // 调试
  void debugPrint(void);

private:
  bool _initialized;
  uint8_t _lastSwitches;
  HC00N_Outputs_t _lastOutputs;
};

extern SN74HC00N boardHC00N;

#ifdef __cplusplus
}
#endif

#endif /* __SN74HC00N_HPP */