/**
 ******************************************************************************
 * @file    bmp180.hpp
 * @author  Typheye
 * @brief   Bmp180 interface.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef BMP180_HPP
#define BMP180_HPP

#include "main.h"
#include "stm32f4xx_hal.h"
#include <cstring>
#include "hardware/include/usart.hpp"
#include "include/libvan.h"
#include "core/sys/include/syslog.h"
#include <math.h>
#include <stdio.h>



#define BMP180_ADDR_7BIT 0x77
#define BMP180_ADDR (0x77 << 1)


#define BMP180_CAL_AC1 0xAA
#define BMP180_CAL_AC2 0xAC
#define BMP180_CAL_AC3 0xAE
#define BMP180_CAL_AC4 0xB0
#define BMP180_CAL_AC5 0xB2
#define BMP180_CAL_AC6 0xB4
#define BMP180_CAL_B1 0xB6
#define BMP180_CAL_B2 0xB8
#define BMP180_CAL_MB 0xBA
#define BMP180_CAL_MC 0xBC
#define BMP180_CAL_MD 0xBE

#define BMP180_TEMP_MSB 0xF6
#define BMP180_TEMP_LSB 0xF7
#define BMP180_TEMP_XLSB 0xF8

#define BMP180_PRESS_MSB 0xF6
#define BMP180_PRESS_LSB 0xF7
#define BMP180_PRESS_XLSB 0xF8

#define BMP180_CTRL_MEAS 0xF4
#define BMP180_SOFT_RESET 0xE0


#define BMP180_TEMP_CMD 0x2E
#define BMP180_PRESS_0_CMD 0x34
#define BMP180_PRESS_1_CMD 0x74
#define BMP180_PRESS_2_CMD 0xB4
#define BMP180_PRESS_3_CMD 0xF4


typedef enum {
  BMP180_MODE_ULP = 0,
  BMP180_MODE_STD = 1,
  BMP180_MODE_HR = 2,
  BMP180_MODE_UHR = 3
} BMP180_Mode_t;


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


typedef struct {
  float temperature;
  float pressure;
  float altitude;
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

  bool readCalibration(void);


  int16_t readRawTemp(void);


  uint32_t readRawPressure(BMP180_Mode_t mode);


  float readTemperature(void);


  float readPressure(BMP180_Mode_t mode = BMP180_MODE_STD);


  BMP180_Data_t readData(BMP180_Mode_t mode = BMP180_MODE_STD);


  float calcAltitude(float pressure, float seaLevelPressure = 1013.25f);


  void softReset(void);


  void setMode(BMP180_Mode_t mode);


  uint8_t getMeasurementDelay(BMP180_Mode_t mode);

  void debugCalibration(void);

private:
  I2C_HandleTypeDef *_hi2c;
  uint16_t _addr;
  bool _initialized;
  BMP180_Calib_t _calib;
  BMP180_Mode_t _mode;


  bool readReg(uint8_t reg, uint8_t *value);


  bool writeReg(uint8_t reg, uint8_t value);


  int16_t readReg16(uint8_t reg);


  uint16_t readReg16U(uint8_t reg);


  void readRegs(uint8_t reg, uint8_t *buffer, uint8_t len);


  int32_t computeB5(int32_t UT);
};

extern BMP180 boardBMP180;

#endif // BMP180_HPP