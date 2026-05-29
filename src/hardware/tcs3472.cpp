#include "include/tcs3472.hpp"
#include <math.h>
#include <stdio.h>

// 外部 I2C 句柄
extern I2C_HandleTypeDef hi2c1;

// 全局实例
TCS3472 boardTCS3472;

// LED 补光灯引脚 (PC3)
#define TCS_LED_LOW HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET)
#define TCS_LED_HIGH HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET)

// 构造函数
TCS3472::TCS3472() {
  _hi2c = &hi2c1;
  _addr = TCS3472_ADDR_8BIT;
  _initialized = false;
  _gain = TCS3472_CONTROL_AGAIN_16X;
  _atime = 0x00; // 最短积分时间 (~2.4ms)
}

// 读取单个寄存器
uint8_t TCS3472::readReg(uint8_t reg) {
  uint8_t value = 0;
  uint8_t cmd = TCS3472_COMMAND_BIT | reg;

  HAL_I2C_Mem_Read(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
  return value;
}

// 写入单个寄存器
void TCS3472::writeReg(uint8_t reg, uint8_t value) {
  uint8_t cmd = TCS3472_COMMAND_BIT | reg;
  HAL_I2C_Mem_Write(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}

// 读取16位寄存器
uint16_t TCS3472::readReg16(uint8_t reg) {
  uint8_t buffer[2];
  uint8_t cmd = TCS3472_COMMAND_BIT | reg;

  HAL_I2C_Mem_Read(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, buffer, 2, 100);
  return (uint16_t)(buffer[0] | (buffer[1] << 8));
}

// 设置增益
void TCS3472::setGain(uint8_t gain) {
  _gain = gain;
  writeReg(TCS3472_CONTROL, _gain);
}

// 设置积分时间 (ATIME)
// atime = 256 - 积分时间 (每步 2.4ms)
// 积分时间 = (256 - atime) * 2.4ms
void TCS3472::setIntegrationTime(uint8_t atime) {
  _atime = atime;
  writeReg(TCS3472_ATIME, _atime);
}

// 初始化
void TCS3472::init(void) {
  if (_initialized)
    return;

  HAL_Delay(100);

  // 读取 ID 验证连接
  uint8_t id = readID();
  if (id == 0x44 || id == 0x4D) { // TCS34725: 0x44, TCS34721: 0x4D
    _initialized = true;
  }

  if (!_initialized) {
    printf("[TCS3472] Init failed! ID=0x%02X\r\n", id);
    return;
  }

  // 配置传感器
  setIntegrationTime(_atime);
  setGain(_gain);

  // 上电并启动 RGBC
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON);
  HAL_Delay(3);
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);

  // 默认关闭补光灯
  ledOff();

  printf("[TCS3472] Initialized, ID=0x%02X\r\n", id);
  printf("[TCS3472] Gain=%dX, Integration=%.1fms\r\n", (int)pow(4, _gain),
         (256 - _atime) * 2.4f);
}

// 等待数据有效
bool TCS3472::waitForData(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    uint8_t status = readReg(TCS3472_STATUS);
    if (status & TCS3472_STATUS_AVALID) {
      return true;
    }
    HAL_Delay(1);
  }
  return false;
}

// 读取原始数据
TCS3472_RawData_t TCS3472::readRaw(void) {
  TCS3472_RawData_t data = {0, 0, 0, 0, 0};

  if (!_initialized)
    return data;

  // 等待数据有效
  if (!waitForData(100)) {
    return data;
  }

  // 读取所有颜色数据 (连续读取)
  uint8_t buffer[8];
  uint8_t cmd = TCS3472_COMMAND_BIT | TCS3472_CDATAL;
  HAL_I2C_Mem_Read(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, buffer, 8, 100);

  data.clear = (uint16_t)(buffer[1] << 8) | buffer[0];
  data.red = (uint16_t)(buffer[3] << 8) | buffer[2];
  data.green = (uint16_t)(buffer[5] << 8) | buffer[4];
  data.blue = (uint16_t)(buffer[7] << 8) | buffer[6];

  return data;
}

// 计算色温 (基于 Taos 公式)
float TCS3472::calculateColorTemperature(uint16_t r, uint16_t g, uint16_t b) {
  // 防止除零
  if (r == 0 || g == 0 || b == 0)
    return 0;

  // 归一化到绿色通道
  float r_norm = (float)r / g;
  float b_norm = (float)b / g;

  // Taos 色温公式 (适用于 TCS34725)
  // CT = 3810 * (R/G) + 1392 * (B/G) + 1084
  float color_temp = 3810.0f * r_norm + 1392.0f * b_norm + 1084.0f;

  if (color_temp < 2000)
    color_temp = 2000;
  if (color_temp > 10000)
    color_temp = 10000;

  return color_temp;
}

// 计算照度 (Lux)
float TCS3472::calculateLux(uint16_t c, uint16_t r, uint16_t g, uint16_t b) {
  // 使用 TCS34725 的 Lux 公式
  float ir = (float)(c - r - g - b);
  if (ir < 0)
    ir = 0;

  float g_norm = (float)g / c;

  // 根据不同增益和积分时间需要调整
  // 这里使用经验公式
  float lux = (0.136f * r + 0.542f * g + 0.035f * b) * (1.0f / g_norm);

  // 根据增益调整
  float gain_factor = pow(4, _gain);
  lux = lux * (16.0f / gain_factor);

  // 根据积分时间调整
  float time_factor = (256 - _atime) * 2.4f / 50.0f;
  lux = lux / time_factor;

  return lux;
}

// 读取颜色数据（处理后台）
TCS3472_ColorData_t TCS3472::readColor(void) {
  TCS3472_ColorData_t result = {0, 0, 0, 0, 0};

  TCS3472_RawData_t raw = readRaw();
  if (raw.clear == 0)
    return result;

  // 归一化 (除以 clear 通道)
  result.red = (float)raw.red / raw.clear;
  result.green = (float)raw.green / raw.clear;
  result.blue = (float)raw.blue / raw.clear;

  // 计算色温
  result.color_temp = calculateColorTemperature(raw.red, raw.green, raw.blue);

  // 计算照度
  result.lux = calculateLux(raw.clear, raw.red, raw.green, raw.blue);

  return result;
}

// LED 控制
void TCS3472::ledOn(void) { TCS_LED_HIGH; }

void TCS3472::ledOff(void) { TCS_LED_LOW; }

void TCS3472::ledSet(bool on) {
  if (on) {
    ledOn();
  } else {
    ledOff();
  }
}

// 读取芯片 ID
uint8_t TCS3472::readID(void) { return readReg(TCS3472_ID); }

// 软复位
void TCS3472::softReset(void) {
  writeReg(TCS3472_ENABLE, 0x00);
  HAL_Delay(10);
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON);
  HAL_Delay(3);
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);
}

// 开启传感器
void TCS3472::enable(void) {
  if (!_initialized)
    return;
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);
}

// 关闭传感器（省电）
void TCS3472::disable(void) {
  if (!_initialized)
    return;
  writeReg(TCS3472_ENABLE, 0x00);
  ledOff();
}

float TCS3472::getLux(void) {
  TCS3472_ColorData_t c = readColor();
  return c.lux;
}