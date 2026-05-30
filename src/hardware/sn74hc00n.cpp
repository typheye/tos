#include "include/sn74hc00n.hpp"
#include "syslog.h"
#include <stdio.h>

// 全局实例
SN74HC00N boardHC00N;

// 构造函数
SN74HC00N::SN74HC00N() {
  _initialized = false;
  _lastSwitches = 0;
  _lastOutputs = {0, 0, 0, 0};
}

// NAND 门逻辑: 输出 = NOT (A AND B) - 静态方法
// 修改为 AND 门逻辑: 输出 = A AND B
uint8_t SN74HC00N::nandGate(uint8_t a, uint8_t b) {
  // 两个输入都为高电平时，输出高电平
  // 否则输出低电平
  if (a && b) {
    return HC00N_HIGH; // 1 AND 1 = 1
  } else {
    return HC00N_LOW; // 其他情况 = 0
  }
}

// 根据开关输入组合计算理论输出 - 静态方法
// 注意：这里的 switches 参数，bit 代表开关闭合(1)/断开(0)
uint8_t SN74HC00N::calculateNANDOutput(uint8_t switches) {
  // 提取每个通道的两个开关状态 (原始输入：1=闭合, 0=断开)
  uint8_t raw1A = (switches >> 0) & 0x01;
  uint8_t raw1B = (switches >> 1) & 0x01;
  uint8_t raw2A = (switches >> 2) & 0x01;
  uint8_t raw2B = (switches >> 3) & 0x01;
  uint8_t raw3A = (switches >> 4) & 0x01;
  uint8_t raw3B = (switches >> 5) & 0x01;
  uint8_t raw4A = (switches >> 6) & 0x01;
  uint8_t raw4B = (switches >> 7) & 0x01;

  // ========== 关键修改：翻转输入逻辑 ==========
  // 因为你的硬件：开关闭合 = 低电平，开关断开 = 高电平
  // 所以需要翻转：原始1表示闭合(低电平) -> 转为0(低电平)
  //           原始0表示断开(高电平) -> 转为1(高电平)
  uint8_t sw1A = raw1A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw1B = raw1B ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw2A = raw2A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw2B = raw2B ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw3A = raw3A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw3B = raw3B ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw4A = raw4A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw4B = raw4B ? HC00N_LOW : HC00N_HIGH;

  // 计算每个通道的输出 - 直接调用静态方法
  uint8_t out = 0;
  if (nandGate(sw1A, sw1B))
    out |= 0x01; // bit0 = 1Y (输出高电平)
  if (nandGate(sw2A, sw2B))
    out |= 0x02; // bit1 = 2Y
  if (nandGate(sw3A, sw3B))
    out |= 0x04; // bit2 = 3Y
  if (nandGate(sw4A, sw4B))
    out |= 0x08; // bit3 = 4Y

  return out;
}

// 读取实际输出（从PE10-PE13）
// 输出的是实际电平：1=3.3V, 0=0V
uint8_t SN74HC00N::readOutputByte(void) {
  if (!_initialized)
    return 0;

  uint8_t out = 0;

  if (HAL_GPIO_ReadPin(HC00N_OUT1_PORT, HC00N_OUT1_PIN) == GPIO_PIN_SET) {
    out |= 0x01;
  }
  if (HAL_GPIO_ReadPin(HC00N_OUT2_PORT, HC00N_OUT2_PIN) == GPIO_PIN_SET) {
    out |= 0x02;
  }
  if (HAL_GPIO_ReadPin(HC00N_OUT3_PORT, HC00N_OUT3_PIN) == GPIO_PIN_SET) {
    out |= 0x04;
  }
  if (HAL_GPIO_ReadPin(HC00N_OUT4_PORT, HC00N_OUT4_PIN) == GPIO_PIN_SET) {
    out |= 0x08;
  }

  return out;
}

// 读取所有输出状态 (结构体)
HC00N_Outputs_t SN74HC00N::readOutputs(void) {
  HC00N_Outputs_t outs;
  uint8_t data = readOutputByte();

  outs.output1 = (data >> 0) & 0x01;
  outs.output2 = (data >> 1) & 0x01;
  outs.output3 = (data >> 2) & 0x01;
  outs.output4 = (data >> 3) & 0x01;

  return outs;
}

// 读取单个输出通道
uint8_t SN74HC00N::readOutput(uint8_t channel) {
  if (channel >= 4)
    return 0;

  switch (channel) {
  case 0:
    return HAL_GPIO_ReadPin(HC00N_OUT1_PORT, HC00N_OUT1_PIN) == GPIO_PIN_SET;
  case 1:
    return HAL_GPIO_ReadPin(HC00N_OUT2_PORT, HC00N_OUT2_PIN) == GPIO_PIN_SET;
  case 2:
    return HAL_GPIO_ReadPin(HC00N_OUT3_PORT, HC00N_OUT3_PIN) == GPIO_PIN_SET;
  case 3:
    return HAL_GPIO_ReadPin(HC00N_OUT4_PORT, HC00N_OUT4_PIN) == GPIO_PIN_SET;
  default:
    return 0;
  }
}

// 初始化
void SN74HC00N::init(void) {
  if (_initialized)
    return;

  _initialized = true;

  LOG_I("HC00", "SN74HC00N Driver Initialized");
  LOG_I("HC00", "Inputs: 8 switches (1A1B-4A4B)");
  LOG_I("HC00", "Outputs: PE10=1Y, PE11=2Y, PE12=3Y, PE13=4Y");
  LOG_I("HC00", "Logic: Y = NOT (A AND B)");
  LOG_I("HC00", "Note: Input logic flipped for your hardware");

  // 读取初始输出状态
  _lastOutputs = readOutputs();
}

// 根据开关输入更新
void SN74HC00N::updateFromSwitches(uint8_t switchStates) {
  _lastSwitches = switchStates;

  // 理论输出
  uint8_t expected = calculateNANDOutput(switchStates);

  // 实际输出
  uint8_t actual = readOutputByte();

  _lastOutputs.output1 = (actual >> 0) & 0x01;
  _lastOutputs.output2 = (actual >> 1) & 0x01;
  _lastOutputs.output3 = (actual >> 2) & 0x01;
  _lastOutputs.output4 = (actual >> 3) & 0x01;

  // 可选: 验证并打印不匹配
  if (expected != actual) {
    LOG_W("HC00", "Mismatch! Expected: 0x%02X, Actual: 0x%02X", expected,
           actual);
  }
}

// 获取最后更新的开关状态（原始值）
HC00N_Switches_t SN74HC00N::getSwitchStates(void) {
  HC00N_Switches_t sw;

  sw.sw1A = (_lastSwitches >> 0) & 0x01;
  sw.sw1B = (_lastSwitches >> 1) & 0x01;
  sw.sw2A = (_lastSwitches >> 2) & 0x01;
  sw.sw2B = (_lastSwitches >> 3) & 0x01;
  sw.sw3A = (_lastSwitches >> 4) & 0x01;
  sw.sw3B = (_lastSwitches >> 5) & 0x01;
  sw.sw4A = (_lastSwitches >> 6) & 0x01;
  sw.sw4B = (_lastSwitches >> 7) & 0x01;

  return sw;
}

// 获取实际输出状态
HC00N_Outputs_t SN74HC00N::getActualOutputs(void) { return _lastOutputs; }

// 验证输出
bool SN74HC00N::verifyOutputs(uint8_t switches) {
  uint8_t expected = calculateNANDOutput(switches);
  uint8_t actual = readOutputByte();
  return expected == actual;
}

// 调试打印
void SN74HC00N::debugPrint(void) {
  HC00N_Switches_t sw = getSwitchStates();
  HC00N_Outputs_t outs = getActualOutputs();
  uint8_t raw = readOutputByte();
  (void)sw; (void)outs; (void)raw;

  LOG_D("HC00", "========== HC00N Status ==========");
  LOG_D("HC00", "Input Switches (raw: 1=closed, 0=open):");
  LOG_D("HC00", "  CH1: A=%d, B=%d", sw.sw1A, sw.sw1B);
  LOG_D("HC00", "  CH2: A=%d, B=%d", sw.sw2A, sw.sw2B);
  LOG_D("HC00", "  CH3: A=%d, B=%d", sw.sw3A, sw.sw3B);
  LOG_D("HC00", "  CH4: A=%d, B=%d", sw.sw4A, sw.sw4B);
  LOG_D("HC00", "Outputs (read from PE10-PE13):");
  LOG_D("HC00", "  1Y=%d, 2Y=%d, 3Y=%d, 4Y=%d", outs.output1, outs.output2,
         outs.output3, outs.output4);
  LOG_D("HC00", "Raw output byte: 0x%02X", raw);
  LOG_D("HC00", "Logic: Y = NOT (A AND B)");
  LOG_D("HC00", "==================================");
}