/**
 ******************************************************************************
 * @file    bmp180.cpp
 * @author  Typheye
 * @brief   Bmp180 implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "include/bmp180.hpp"
#include "library/include/libdly.h"



extern I2C_HandleTypeDef hi2c1;

extern USART boardSerial;


BMP180 boardBMP180;


BMP180::BMP180() {
  _hi2c = &hi2c1;
  _addr = BMP180_ADDR; 
  _initialized = false;
  _mode = BMP180_MODE_STD;
  memset(&_calib, 0, sizeof(_calib));
}


void BMP180::init(void) {
  if (_initialized)
    return;

  JPDelay(200);

  if (checkConnection()) {
    
    if (readCalibration()) {
      _initialized = true;
    }
  }
}


bool BMP180::checkConnection(void) {
  uint8_t test = 0;
  
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_CAL_AC1, I2C_MEMADD_SIZE_8BIT,
                       &test, 1, 100) == HAL_OK) {
    return true;
  }
  return false;
}


bool BMP180::readCalibration(void) {
  uint8_t buffer[22];

  
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_CAL_AC1, I2C_MEMADD_SIZE_8BIT,
                       buffer, 22, 100) != HAL_OK) {
    return false;
  }

  
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


bool BMP180::readReg(uint8_t reg, uint8_t *value) {
  return HAL_I2C_Mem_Read(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, value, 1,
                          100) == HAL_OK;
}


bool BMP180::writeReg(uint8_t reg, uint8_t value) {
  return HAL_I2C_Mem_Write(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1,
                           100) == HAL_OK;
}


int16_t BMP180::readRawTemp(void) {
  
  if (!writeReg(BMP180_CTRL_MEAS, BMP180_TEMP_CMD)) {
    return 0;
  }

  
  JPDelay(5);

  
  uint8_t buffer[2];
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_TEMP_MSB, I2C_MEMADD_SIZE_8BIT,
                       buffer, 2, 100) != HAL_OK) {
    return 0;
  }

  return (int16_t)((buffer[0] << 8) | buffer[1]);
}


uint32_t BMP180::readRawPressure(BMP180_Mode_t mode) {
  uint8_t oss = mode;
  uint8_t cmd = BMP180_PRESS_0_CMD + (oss << 1);

  
  if (!writeReg(BMP180_CTRL_MEAS, cmd)) {
    return 0;
  }

  
  JPDelay(getMeasurementDelay(mode));

  
  uint8_t buffer[3];
  if (HAL_I2C_Mem_Read(_hi2c, _addr, BMP180_PRESS_MSB, I2C_MEMADD_SIZE_8BIT,
                       buffer, 3, 100) != HAL_OK) {
    return 0;
  }

  
  uint32_t up =
      ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

  
  up = up >> (8 - oss);

  return up;
}



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


float BMP180::readPressure(BMP180_Mode_t mode) {

  
  int16_t UT = readRawTemp();
  if (UT == 0)
    return 0;

  
  int32_t X1 = (UT - (int32_t)_calib.AC6) * ((int32_t)_calib.AC5) >> 15;
  int32_t X2 = ((int32_t)_calib.MC << 11) / (X1 + (int32_t)_calib.MD);
  int32_t B5 = X1 + X2;

  
  uint32_t UP = readRawPressure(mode);
  if (UP == 0)
    return 0;

  
  int32_t B6 = B5 - 4000;

  
  int32_t X3 = ((int32_t)_calib.B2 * ((B6 * B6) >> 12)) >> 11;
  int32_t X4 = ((int32_t)_calib.AC2 * B6) >> 11;
  int32_t X5 = X3 + X4;

  
  int32_t B3 = ((((int32_t)_calib.AC1 * 4 + X5) << mode) + 2) / 4;

  
  X1 = ((int32_t)_calib.AC3 * B6) >> 13;
  X2 = ((int32_t)_calib.B1 * ((B6 * B6) >> 12)) >> 16;
  X3 = (X1 + X2 + 2) >> 2;

  
  uint32_t B4 = ((uint32_t)_calib.AC4 * (uint32_t)(X3 + 32768)) >> 15;

  
  uint32_t B7 = ((uint32_t)UP - (uint32_t)B3) * (50000 >> mode);

  
  int32_t p;
  if (B7 < 0x80000000) {
    p = (B7 << 1) / B4;
  } else {
    p = (B7 / B4) << 1;
  }

  
  X1 = (p >> 8) * (p >> 8);
  X1 = (X1 * 3038) >> 16;
  X2 = (-7357 * p) >> 16;
  p = p + ((X1 + X2 + 3791) >> 4);

  
  float pressure = p / 100.0f;

  return pressure;
}


BMP180_Data_t BMP180::readData(BMP180_Mode_t mode) {
  BMP180_Data_t data;
  memset(&data, 0, sizeof(data));
  data.temperature = readTemperature();
  data.pressure    = readPressure(mode);
  data.altitude    = calcAltitude(data.pressure);

  return data;
}


float BMP180::calcAltitude(float pressure, float seaLevelPressure) {
  return 44330.0f * (1.0f - powf(pressure / seaLevelPressure, 0.1903f));
}


void BMP180::softReset(void) {
  writeReg(BMP180_SOFT_RESET, 0xB6);
  JPDelay(50);
}


void BMP180::setMode(BMP180_Mode_t mode) { _mode = mode; }


uint8_t BMP180::getMeasurementDelay(BMP180_Mode_t mode) {
  switch (mode) {
  case BMP180_MODE_ULP:
    return 10; 
  case BMP180_MODE_STD:
    return 20; 
  case BMP180_MODE_HR:
    return 30; 
  case BMP180_MODE_UHR:
    return 50; 
  default:
    return 20;
  }
}


void BMP180::debugCalibration(void) {
  LOG_D("BMP", "AC1=%d, AC2=%d, AC3=%d", _calib.AC1, _calib.AC2,
          _calib.AC3);
  LOG_D("BMP", "AC4=%u, AC5=%u, AC6=%u", _calib.AC4, _calib.AC5,
          _calib.AC6);
  LOG_D("BMP", "B1=%d, B2=%d", _calib.B1, _calib.B2);
  LOG_D("BMP", "MB=%d, MC=%d, MD=%d", _calib.MB, _calib.MC, _calib.MD);
}