#include "include/bmp180.hpp"
#include "hardware/include/usart.hpp"
#include "include/libvan.h"
#include "syslog.h"
#include <math.h>
#include <stdio.h>

// 外部 I2C 句柄 (由 CubeMX 生成)
extern I2C_HandleTypeDef hi2c1;

extern USART boardSerial;

// 全局实例
BMP180 boardBMP180;

// 构造函数
BMP180::BMP180() {
  _hi2c = &hi2c1;
  _addr = BMP180_ADDR; // 0xEE (8位地址)
  _initialized = false;
  _mode = BMP180_MODE_STD;
  memset(&_calib, 0, sizeof(_calib));
}

// 初始化
void BMP180::init(void) {
  if (_initialized)
    return;

  HAL_Delay(200);

  if (checkConnection()) {
    // 读取校准数据
    if (readCalibration()) {
      _initialized = true;
    }
  }
}

// 检查连接
bool BMP180::checkConnection(void) {
  uint8_t test = 0;
  // 尝试读取校准数据第一个字节
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_CAL_AC1, I2C_MEMADD_SIZE_8BIT,
                       &test, 1, 100) == HAL_OK) {
    return true;
  }
  return false;
}

// 读取校准数据
bool BMP180::readCalibration(void) {
  uint8_t buffer[22];

  // 一次读取所有校准数据
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_CAL_AC1, I2C_MEMADD_SIZE_8BIT,
                       buffer, 22, 100) != HAL_OK) {
    return false;
  }

  // 解析校准数据 (注意字节序：高字节在前)
  _calib.AC1 = (int16_t)((buffer[0] << 8) | buffer[1]);
  _calib.AC2 = (int16_t)((buffer[2] << 8) | buffer[3]);
  _calib.AC3 = (int16_t)((buffer[4] << 8) | buffer[5]);
  _calib.AC4 = (uint16_t)((buffer[6] << 8) | buffer[7]);
  _calib.AC5 = (uint16_t)((buffer[8] << 8) | buffer[9]);
  _calib.AC6 = (uint16_t)((buffer[10] << 8) | buffer[11]);
  _calib.B1 = (int16_t)((buffer[12] << 8) | buffer[13]);
  _calib.B2 = (int16_t)((buffer[14] << 8) | buffer[15]);
  _calib.MB = (int16_t)((buffer[16] << 8) | buffer[17]);
  _calib.MC = (int16_t)((buffer[18] << 8) | buffer[19]);
  _calib.MD = (int16_t)((buffer[20] << 8) | buffer[21]);

  return true;
}

// 读取单个寄存器
bool BMP180::readReg(uint8_t reg, uint8_t *value) {
  return HAL_I2C_Mem_Read(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, value, 1,
                          100) == HAL_OK;
}

// 写入单个寄存器
bool BMP180::writeReg(uint8_t reg, uint8_t value) {
  return HAL_I2C_Mem_Write(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1,
                           100) == HAL_OK;
}

// 读取原始温度值 (未补偿)
int16_t BMP180::readRawTemp(void) {
  // 发送温度测量命令
  if (!writeReg(BMP180_CTRL_MEAS, BMP180_TEMP_CMD)) {
    return 0;
  }

  // 等待测量完成 (4.5ms)
  HAL_Delay(5);

  // 读取温度值
  uint8_t buffer[2];
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_TEMP_MSB, I2C_MEMADD_SIZE_8BIT,
                       buffer, 2, 100) != HAL_OK) {
    return 0;
  }

  return (int16_t)((buffer[0] << 8) | buffer[1]);
}

// 读取原始压力值
uint32_t BMP180::readRawPressure(BMP180_Mode_t mode) {
  uint8_t oss = mode;
  uint8_t cmd = BMP180_PRESS_0_CMD + (oss << 1);

  // 发送压力测量命令
  if (!writeReg(BMP180_CTRL_MEAS, cmd)) {
    return 0;
  }

  // 等待测量完成 (根据模式)
  HAL_Delay(getMeasurementDelay(mode));

  // 读取压力值 (3字节: MSB, LSB, XLSB)
  uint8_t buffer[3];
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_PRESS_MSB, I2C_MEMADD_SIZE_8BIT,
                       buffer, 3, 100) != HAL_OK) {
    return 0;
  }

  // 组合成20位数据
  uint32_t up =
      ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

  // 修复：右移 (8 - oss) 位
  up = up >> (8 - oss);

  return up;
}

// 读取补偿后的温度
// 修复后的 readTemperature 函数（添加调试输出）
float BMP180::readTemperature(void) {
  int16_t UT = readRawTemp();
  if (UT == 0)
    return 0;

  int32_t X1 = (UT - (int32_t)_calib.AC6) * ((int32_t)_calib.AC5) >> 15;
  int32_t X2 = ((int32_t)_calib.MC << 11) / (X1 + (int32_t)_calib.MD);
  int32_t B5 = X1 + X2;
  float temp = (B5 + 8) / 160.0f;

  return temp;
}

// 读取补偿后的压力
float BMP180::readPressure(BMP180_Mode_t mode) {

  // 先读取温度 (需要 B5)
  int16_t UT = readRawTemp();
  if (UT == 0)
    return 0;

  // 计算 B5
  int32_t X1 = (UT - (int32_t)_calib.AC6) * ((int32_t)_calib.AC5) >> 15;
  int32_t X2 = ((int32_t)_calib.MC << 11) / (X1 + (int32_t)_calib.MD);
  int32_t B5 = X1 + X2;

  // 读取原始压力
  uint32_t UP = readRawPressure(mode);
  if (UP == 0)
    return 0;

  // 根据数据手册公式计算压力
  int32_t B6 = B5 - 4000;

  // 计算 X1, X2, X3
  int32_t X3 = ((int32_t)_calib.B2 * ((B6 * B6) >> 12)) >> 11;
  int32_t X4 = ((int32_t)_calib.AC2 * B6) >> 11;
  int32_t X5 = X3 + X4;

  // 根据模式调整 B3
  int32_t B3 = ((((int32_t)_calib.AC1 * 4 + X5) << mode) + 2) / 4;

  // 计算 X1, X2 (复用变量)
  X1 = ((int32_t)_calib.AC3 * B6) >> 13;
  X2 = ((int32_t)_calib.B1 * ((B6 * B6) >> 12)) >> 16;
  X3 = (X1 + X2 + 2) >> 2;

  // 计算 B4
  uint32_t B4 = ((uint32_t)_calib.AC4 * (uint32_t)(X3 + 32768)) >> 15;

  // 计算 B7
  uint32_t B7 = ((uint32_t)UP - (uint32_t)B3) * (50000 >> mode);

  // 计算压力 p
  int32_t p;
  if (B7 < 0x80000000) {
    p = (B7 << 1) / B4;
  } else {
    p = (B7 / B4) << 1;
  }

  // 修正
  X1 = (p >> 8) * (p >> 8);
  X1 = (X1 * 3038) >> 16;
  X2 = (-7357 * p) >> 16;
  p = p + ((X1 + X2 + 3791) >> 4);

  // 转换为 hPa (帕斯卡 / 100)
  float pressure = p / 100.0f;

  return pressure;
}

// 一次性读取温度和压力
BMP180_Data_t BMP180::readData(BMP180_Mode_t mode) {
  BMP180_Data_t data;
  memset(&data, 0, sizeof(data));
  data.temperature = readTemperature();
  data.pressure    = readPressure(mode);
  data.altitude    = calcAltitude(data.pressure);

  return data;
}

// 计算海拔高度
float BMP180::calcAltitude(float pressure, float seaLevelPressure) {
  return 44330.0f * (1.0f - powf(pressure / seaLevelPressure, 0.1903f));
}

// 软复位
void BMP180::softReset(void) {
  writeReg(BMP180_SOFT_RESET, 0xB6);
  HAL_Delay(50);
}

// 设置测量模式
void BMP180::setMode(BMP180_Mode_t mode) { _mode = mode; }

// 获取等待时间 (根据模式)
uint8_t BMP180::getMeasurementDelay(BMP180_Mode_t mode) {
  switch (mode) {
  case BMP180_MODE_ULP:
    return 10; // 4.5ms -> 10ms (增加余量)
  case BMP180_MODE_STD:
    return 20; // 7.5ms -> 20ms (增加余量)
  case BMP180_MODE_HR:
    return 30; // 13.5ms -> 30ms (增加余量)
  case BMP180_MODE_UHR:
    return 50; // 25.5ms -> 50ms (增加余量)
  default:
    return 20;
  }
}

// 调试函数：打印校准数据
void BMP180::debugCalibration(void) {
  LOG_D("BMP", "AC1=%d, AC2=%d, AC3=%d", _calib.AC1, _calib.AC2,
          _calib.AC3);
  LOG_D("BMP", "AC4=%u, AC5=%u, AC6=%u", _calib.AC4, _calib.AC5,
          _calib.AC6);
  LOG_D("BMP", "B1=%d, B2=%d", _calib.B1, _calib.B2);
  LOG_D("BMP", "MB=%d, MC=%d, MD=%d", _calib.MB, _calib.MC, _calib.MD);
}