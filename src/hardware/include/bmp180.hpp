#ifndef __BMP180_HPP
#define __BMP180_HPP

#include "main.h"
#include "stm32f4xx_hal.h"
#include <cstring>

// BMP180 I2C 地址定义
// 注意: HAL 库需要传入 8 位地址 (7位地址左移1位)
#define BMP180_ADDR_7BIT 0x77   // 7位地址
#define BMP180_ADDR (0x77 << 1) // 8位地址: 0xEE

// 寄存器地址
#define BMP180_CAL_AC1 0xAA // 校准数据 AC1 (2字节)
#define BMP180_CAL_AC2 0xAC // 校准数据 AC2 (2字节)
#define BMP180_CAL_AC3 0xAE // 校准数据 AC3 (2字节)
#define BMP180_CAL_AC4 0xB0 // 校准数据 AC4 (2字节)
#define BMP180_CAL_AC5 0xB2 // 校准数据 AC5 (2字节)
#define BMP180_CAL_AC6 0xB4 // 校准数据 AC6 (2字节)
#define BMP180_CAL_B1 0xB6  // 校准数据 B1 (2字节)
#define BMP180_CAL_B2 0xB8  // 校准数据 B2 (2字节)
#define BMP180_CAL_MB 0xBA  // 校准数据 MB (2字节)
#define BMP180_CAL_MC 0xBC  // 校准数据 MC (2字节)
#define BMP180_CAL_MD 0xBE  // 校准数据 MD (2字节)

#define BMP180_TEMP_MSB 0xF6  // 温度 MSB
#define BMP180_TEMP_LSB 0xF7  // 温度 LSB
#define BMP180_TEMP_XLSB 0xF8 // 温度 XLSB (未使用)

#define BMP180_PRESS_MSB 0xF6  // 压力 MSB
#define BMP180_PRESS_LSB 0xF7  // 压力 LSB
#define BMP180_PRESS_XLSB 0xF8 // 压力 XLSB

#define BMP180_CTRL_MEAS 0xF4  // 控制寄存器
#define BMP180_SOFT_RESET 0xE0 // 软复位寄存器

// 测量模式
#define BMP180_TEMP_CMD 0x2E    // 温度测量命令
#define BMP180_PRESS_0_CMD 0x34 // 压力测量命令 (超低功耗)
#define BMP180_PRESS_1_CMD 0x74 // 压力测量命令 (标准)
#define BMP180_PRESS_2_CMD 0xB4 // 压力测量命令 (高分辨率)
#define BMP180_PRESS_3_CMD 0xF4 // 压力测量命令 (超高分辨率)

// 测量模式枚举
typedef enum {
  BMP180_MODE_ULP = 0, // 超低功耗 (4.5ms)
  BMP180_MODE_STD = 1, // 标准 (7.5ms)
  BMP180_MODE_HR = 2,  // 高分辨率 (13.5ms)
  BMP180_MODE_UHR = 3  // 超高分辨率 (25.5ms)
} BMP180_Mode_t;

// 校准数据结构体
typedef struct {
  int16_t AC1;
  int16_t AC2;
  int16_t AC3;
  uint16_t AC4;
  uint16_t AC5;
  uint16_t AC6;
  int16_t B1;
  int16_t B2;
  int16_t MB;
  int16_t MC;
  int16_t MD;
} BMP180_Calib_t;

// 传感器数据结构体
typedef struct {
  float temperature; // 温度 (摄氏度)
  float pressure;    // 气压 (hPa)
  float altitude;    // 海拔 (米，海平面气压1013.25hPa)
} BMP180_Data_t;

class BMP180 {
public:
  BMP180();

  void init(void);
  bool checkConnection(void);
  bool isInitialized(void) { return _initialized; }

  int16_t getAC1(void) { return _calib.AC1; }
  int16_t getAC2(void) { return _calib.AC2; }
  int16_t getAC3(void) { return _calib.AC3; }
  uint16_t getAC4(void) { return _calib.AC4; }
  uint16_t getAC5(void) { return _calib.AC5; }
  uint16_t getAC6(void) { return _calib.AC6; }
  int16_t getB1(void) { return _calib.B1; }
  int16_t getB2(void) { return _calib.B2; }
  int16_t getMB(void) { return _calib.MB; }
  int16_t getMC(void) { return _calib.MC; }
  int16_t getMD(void) { return _calib.MD; }
  // 读取校准数据
  bool readCalibration(void);

  // 读取原始温度值 (未补偿)
  int16_t readRawTemp(void);

  // 读取原始压力值 (未补偿)
  uint32_t readRawPressure(BMP180_Mode_t mode);

  // 读取补偿后的温度 (摄氏度)
  float readTemperature(void);

  // 读取补偿后的压力 (hPa)
  float readPressure(BMP180_Mode_t mode = BMP180_MODE_STD);

  // 一次性读取温度和压力
  BMP180_Data_t readData(BMP180_Mode_t mode = BMP180_MODE_STD);

  // 计算海拔高度 (给定海平面气压)
  float calcAltitude(float pressure, float seaLevelPressure = 1013.25f);

  // 软复位
  void softReset(void);

  // 设置测量模式 (可选)
  void setMode(BMP180_Mode_t mode);

  // 获取等待时间 (根据模式)
  uint8_t getMeasurementDelay(BMP180_Mode_t mode);

  void debugCalibration(void); // 添加这行

private:
  I2C_HandleTypeDef *_hi2c;
  uint16_t _addr; // 8位 I2C 地址
  bool _initialized;
  BMP180_Calib_t _calib; // 校准数据
  BMP180_Mode_t _mode;   // 当前模式

  // 读取单个寄存器
  bool readReg(uint8_t reg, uint8_t *value);

  // 写入单个寄存器
  bool writeReg(uint8_t reg, uint8_t value);

  // 读取16位寄存器
  int16_t readReg16(uint8_t reg);

  // 读取无符号16位寄存器
  uint16_t readReg16U(uint8_t reg);

  // 批量读取
  void readRegs(uint8_t reg, uint8_t *buffer, uint8_t len);

  // 补偿计算
  int32_t computeB5(int32_t UT);
};

extern BMP180 boardBMP180;

#endif // __BMP180_HPP