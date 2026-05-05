#ifndef __TCS3472_HPP
#define __TCS3472_HPP

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== I2C 地址 ====================
#define TCS3472_ADDR_7BIT 0x29
#define TCS3472_ADDR_8BIT (TCS3472_ADDR_7BIT << 1) // 0x52

// ==================== 寄存器地址 ====================
#define TCS3472_COMMAND_BIT 0x80
#define TCS3472_COMMAND_TYPE 0x00

// 寄存器
#define TCS3472_ENABLE 0x00
#define TCS3472_ATIME 0x01
#define TCS3472_WTIME 0x03
#define TCS3472_AILTL 0x04
#define TCS3472_AILTH 0x05
#define TCS3472_AIHTL 0x06
#define TCS3472_AIHTH 0x07
#define TCS3472_PERS 0x0C
#define TCS3472_CONFIG 0x0D
#define TCS3472_CONTROL 0x0F
#define TCS3472_ID 0x12
#define TCS3472_STATUS 0x13
#define TCS3472_CDATAL 0x14
#define TCS3472_CDATAH 0x15
#define TCS3472_RDATAL 0x16
#define TCS3472_RDATAH 0x17
#define TCS3472_GDATAL 0x18
#define TCS3472_GDATAH 0x19
#define TCS3472_BDATAL 0x1A
#define TCS3472_BDATAH 0x1B

// ==================== 寄存器位定义 ====================
// ENABLE 寄存器
#define TCS3472_ENABLE_AIEN 0x10 // 中断使能
#define TCS3472_ENABLE_WEN 0x08  // 等待使能
#define TCS3472_ENABLE_AEN 0x02  // RGBC 使能
#define TCS3472_ENABLE_PON 0x01  // 上电

// STATUS 寄存器
#define TCS3472_STATUS_AINT 0x10   // RGBC 中断
#define TCS3472_STATUS_AVALID 0x01 // RGBC 数据有效

// CONTROL 寄存器
#define TCS3472_CONTROL_AGAIN_1X 0x00
#define TCS3472_CONTROL_AGAIN_4X 0x01
#define TCS3472_CONTROL_AGAIN_16X 0x02
#define TCS3472_CONTROL_AGAIN_60X 0x03

// ==================== 数据结构 ====================
typedef struct {
  uint16_t clear; // 全光谱
  uint16_t red;   // 红色
  uint16_t green; // 绿色
  uint16_t blue;  // 蓝色
  uint16_t ir;    // 红外（如果支持）
} TCS3472_RawData_t;

typedef struct {
  float red;        // 归一化红色 (0-1)
  float green;      // 归一化绿色 (0-1)
  float blue;       // 归一化蓝色 (0-1)
  float color_temp; // 色温 (K)
  float lux;        // 照度 (Lux)
} TCS3472_ColorData_t;

// ==================== 类接口 ====================
class TCS3472 {
public:
  TCS3472();

  // 初始化
  void init(void);
  bool isInitialized(void) { return _initialized; }

  // 设置增益
  void setGain(uint8_t gain);

  // 设置积分时间 (ATIME: 2.4ms - 700ms)
  void setIntegrationTime(uint8_t atime);

  // 开启/关闭传感器
  void enable(void);
  void disable(void);

  // 读取原始数据
  TCS3472_RawData_t readRaw(void);

  // 读取处理后的数据（色温、照度等）
  TCS3472_ColorData_t readColor(void);

  // LED 补光灯控制
  void ledOn(void);
  void ledOff(void);
  void ledSet(bool on);

  // 读取芯片 ID
  uint8_t readID(void);

  // 软复位
  void softReset(void);

  // 等待数据有效
  bool waitForData(uint32_t timeout_ms);

private:
  I2C_HandleTypeDef *_hi2c;
  uint16_t _addr;
  bool _initialized;
  uint8_t _gain;
  uint8_t _atime;

  // 内部函数
  uint8_t readReg(uint8_t reg);
  void writeReg(uint8_t reg, uint8_t value);
  uint16_t readReg16(uint8_t reg);

  // 计算色温和照度
  float calculateColorTemperature(uint16_t r, uint16_t g, uint16_t b);
  float calculateLux(uint16_t c, uint16_t r, uint16_t g, uint16_t b);
};

// 全局实例
extern TCS3472 boardTCS3472;

#ifdef __cplusplus
}
#endif

#endif /* __TCS3472_HPP */