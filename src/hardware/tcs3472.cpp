/**
 ******************************************************************************
 * @file    tcs3472.cpp
 * @author  Typheye
 * @brief   Tcs3472 implementation.
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

#include "include/tcs3472.hpp"
#include "library/include/libdly.h"



extern I2C_HandleTypeDef hi2c1;


TCS3472 boardTCS3472;


#define TCS_LED_LOW HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET)
#define TCS_LED_HIGH HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET)


TCS3472::TCS3472() {
  _hi2c = &hi2c1;
  _addr = TCS3472_ADDR_8BIT;
  _initialized = false;
  _gain = TCS3472_CONTROL_AGAIN_16X;
  _atime = 0x00; 
}


uint8_t TCS3472::readReg(uint8_t reg) {
  uint8_t value = 0;
  uint8_t cmd = TCS3472_COMMAND_BIT | reg;

  HAL_I2C_Mem_Read(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
  return value;
}


void TCS3472::writeReg(uint8_t reg, uint8_t value) {
  uint8_t cmd = TCS3472_COMMAND_BIT | reg;
  HAL_I2C_Mem_Write(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, &value, 1, 100);
}


uint16_t TCS3472::readReg16(uint8_t reg) {
  uint8_t buffer[2];
  uint8_t cmd = TCS3472_COMMAND_BIT | reg;

  HAL_I2C_Mem_Read(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, buffer, 2, 100);
  return (uint16_t)(buffer[0] | (buffer[1] << 8));
}


void TCS3472::setGain(uint8_t gain) {
  _gain = gain;
  writeReg(TCS3472_CONTROL, _gain);
}




void TCS3472::setIntegrationTime(uint8_t atime) {
  _atime = atime;
  writeReg(TCS3472_ATIME, _atime);
}


void TCS3472::init(void) {
  if (_initialized)
    return;

  JPDelay(100);

  
  uint8_t id = readID();
  if (id == 0x44 || id == 0x4D) { // TCS34725: 0x44, TCS34721: 0x4D
    _initialized = true;
  }

  if (!_initialized) {
    LOG_E("TCS", "Init failed! ID=0x%02X", id);
    return;
  }

  
  setIntegrationTime(_atime);
  setGain(_gain);

  
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON);
  JPDelay(3);
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);

  
  ledOff();

  LOG_I("TCS", "Initialized, ID=0x%02X", id);
  LOG_I("TCS", "Gain=%dX, Integration=%.1fms", (int)pow(4, _gain),
         (256 - _atime) * 2.4f);
}


bool TCS3472::waitForData(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    uint8_t status = readReg(TCS3472_STATUS);
    if (status & TCS3472_STATUS_AVALID) {
      return true;
    }
    JPDelay(1);
  }
  return false;
}


TCS3472_RawData_t TCS3472::readRaw(void) {
  TCS3472_RawData_t data = {0, 0, 0, 0, 0};

  if (!_initialized)
    return data;

  
  if (!waitForData(100)) {
    return data;
  }

  
  uint8_t buffer[8];
  uint8_t cmd = TCS3472_COMMAND_BIT | TCS3472_CDATAL;
  HAL_I2C_Mem_Read(_hi2c, _addr, cmd, I2C_MEMADD_SIZE_8BIT, buffer, 8, 100);

  data.clear = (uint16_t)(buffer[1] << 8) | buffer[0];
  data.red = (uint16_t)(buffer[3] << 8) | buffer[2];
  data.green = (uint16_t)(buffer[5] << 8) | buffer[4];
  data.blue = (uint16_t)(buffer[7] << 8) | buffer[6];

  return data;
}


float TCS3472::calculateColorTemperature(uint16_t r, uint16_t g, uint16_t b) {
  
  if (r == 0 || g == 0 || b == 0)
    return 0;

  
  float r_norm = (float)r / g;
  float b_norm = (float)b / g;

  
  // CT = 3810 * (R/G) + 1392 * (B/G) + 1084
  float color_temp = 3810.0f * r_norm + 1392.0f * b_norm + 1084.0f;

  if (color_temp < 2000)
    color_temp = 2000;
  if (color_temp > 10000)
    color_temp = 10000;

  return color_temp;
}


float TCS3472::calculateLux(uint16_t c, uint16_t r, uint16_t g, uint16_t b) {
  
  float ir = (float)(c - r - g - b);
  if (ir < 0)
    ir = 0;

  float g_norm = (float)g / c;

  
  
  float lux = (0.136f * r + 0.542f * g + 0.035f * b) * (1.0f / g_norm);

  
  float gain_factor = pow(4, _gain);
  lux = lux * (16.0f / gain_factor);

  
  float time_factor = (256 - _atime) * 2.4f / 50.0f;
  lux = lux / time_factor;

  return lux;
}


TCS3472_ColorData_t TCS3472::readColor(void) {
  TCS3472_ColorData_t result = {0, 0, 0, 0, 0};

  TCS3472_RawData_t raw = readRaw();
  if (raw.clear == 0)
    return result;

  
  result.red = (float)raw.red / raw.clear;
  result.green = (float)raw.green / raw.clear;
  result.blue = (float)raw.blue / raw.clear;

  
  result.color_temp = calculateColorTemperature(raw.red, raw.green, raw.blue);

  
  result.lux = calculateLux(raw.clear, raw.red, raw.green, raw.blue);

  return result;
}


void TCS3472::ledOn(void) { TCS_LED_HIGH; }

void TCS3472::ledOff(void) { TCS_LED_LOW; }

void TCS3472::ledSet(bool on) {
  if (on) {
    ledOn();
  } else {
    ledOff();
  }
}


uint8_t TCS3472::readID(void) { return readReg(TCS3472_ID); }


void TCS3472::softReset(void) {
  writeReg(TCS3472_ENABLE, 0x00);
  JPDelay(10);
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON);
  JPDelay(3);
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);
}


void TCS3472::enable(void) {
  if (!_initialized)
    return;
  writeReg(TCS3472_ENABLE, TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);
}


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