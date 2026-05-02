#ifndef __JY901S_HPP
#define __JY901S_HPP

#include "main.h"
#include "stm32f4xx_hal.h"
#include <cstring>

// JY901S I2C 地址定义
// 注意: HAL 库需要传入 8 位地址 (7位地址左移1位)
#define JY901S_ADDR_7BIT 0x50   // 7位地址
#define JY901S_ADDR (0x50 << 1) // 8位地址: 0xA0

// 寄存器地址
#define JY901S_ACC_X 0x34   // 加速度 X 轴 (2字节)
#define JY901S_ACC_Y 0x36   // 加速度 Y 轴
#define JY901S_ACC_Z 0x38   // 加速度 Z 轴
#define JY901S_GYRO_X 0x3A  // 角速度 X 轴 (2字节)
#define JY901S_GYRO_Y 0x3C  // 角速度 Y 轴
#define JY901S_GYRO_Z 0x3E  // 角速度 Z 轴
#define JY901S_ANGLE_X 0x40 // 欧拉角 X (2字节)
#define JY901S_ANGLE_Y 0x42 // 欧拉角 Y
#define JY901S_ANGLE_Z 0x44 // 欧拉角 Z
#define JY901S_MAG_X 0x46   // 磁场 X 轴 (2字节)
#define JY901S_MAG_Y 0x48   // 磁场 Y 轴
#define JY901S_MAG_Z 0x4A   // 磁场 Z 轴
#define JY901S_TEMP 0x52    // 温度 (2字节)
#define JY901S_VERSION 0x5C // 版本号

// 计算常量 (根据 JY901S 数据手册)
// 加速度: ±16g 量程, 1g = 2048 LSB
// 角速度: ±2000°/s 量程, 1°/s = 16.4 LSB
// 角度: ±180° 量程, 1° = 182.044 LSB
// 温度: 1°C = 100 LSB
#define ACC_SCALE 2048.0f
#define GYRO_SCALE 16.4f
#define ANGLE_SCALE 182.044f
#define TEMP_SCALE 100.0f

// 姿态数据结构体
typedef struct {
  float acc_x;       // 加速度 X (g)
  float acc_y;       // 加速度 Y (g)
  float acc_z;       // 加速度 Z (g)
  float gyro_x;      // 角速度 X (度/秒)
  float gyro_y;      // 角速度 Y (度/秒)
  float gyro_z;      // 角速度 Z (度/秒)
  float roll;        // 横滚角 (度)
  float pitch;       // 俯仰角 (度)
  float yaw;         // 航向角 (度)
  float mag_x;       // 磁场 X
  float mag_y;       // 磁场 Y
  float mag_z;       // 磁场 Z
  float temperature; // 温度 (度)
} JY901S_Data_t;

// 原始数据结构体
typedef struct {
  int16_t acc_x;
  int16_t acc_y;
  int16_t acc_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t roll;
  int16_t pitch;
  int16_t yaw;
  int16_t mag_x;
  int16_t mag_y;
  int16_t mag_z;
  int16_t temperature;
} JY901S_Raw_t;

class JY901S {
public:
  JY901S();

  void init(void);
  bool checkConnection(void);
  bool isInitialized(void) { return _initialized; }

  // 读取原始数据
  JY901S_Raw_t readRaw(void);

  // 读取转换后的数据
  JY901S_Data_t readData(void);

  // 分别读取各轴数据
  void readAccel(float *x, float *y, float *z);
  void readGyro(float *x, float *y, float *z);
  void readAngle(float *roll, float *pitch, float *yaw);
  void readMag(float *x, float *y, float *z);
  float readTemp(void);
  uint8_t readVersion(void);

  // 低级读取函数
  int16_t readReg16(uint8_t reg);
  void readRegs(uint8_t reg, uint8_t *buffer, uint8_t len);

private:
  I2C_HandleTypeDef *_hi2c;
  uint16_t _addr; // 8位 I2C 地址
  bool _initialized;

  bool readReg(uint8_t reg, uint8_t *value);
  bool writeReg(uint8_t reg, uint8_t value);
};

extern JY901S boardJY901S;

#endif // __JY901S_HPP