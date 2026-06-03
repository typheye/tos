/**
 ******************************************************************************
 * @file    pot.cpp
 * @author  Typheye
 * @brief   Pot implementation.
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

#include "include/pot.hpp"
#include "syslog.h"
#include <stdio.h>

// 外部 ADC 句柄
extern ADC_HandleTypeDef hadc1;

// 全局实例
Potentiometer boardPot;

// 构造函数
Potentiometer::Potentiometer() {
  _hadc = &hadc1;
  _initialized = false;
  _maxResistance = POT_MAX_RESISTANCE;
  _minResistance = 0.0f;
  _minRaw = 0;
  _maxRaw = POT_ADC_MAX_VALUE;
}

// 初始化
void Potentiometer::init(void) {
  if (_initialized)
    return;

  // ADC 已在 CubeMX 中配置，这里只需验证
  LOG_I("POT", "Potentiometer Driver Initialized");
  LOG_I("POT", "ADC Channel: PC0 (ADC123_IN10)");
  LOG_I("POT", "Reference Voltage: %.2fV", POT_VREF);
  LOG_I("POT", "Max Resistance: %.1fkΩ", _maxResistance);

  _initialized = true;

  // 测试读取
  LOG_I("POT", "Test reading: %d (%.2fV)", (int)readRaw(), (double)readVoltage());
}

// 读取 ADC 原始值 (单次)
uint16_t Potentiometer::readRaw(void) {
  if (!_initialized)
    return 0;

  uint16_t adc_value = 0;
  HAL_ADC_Start(_hadc);
  if (HAL_ADC_PollForConversion(_hadc, 100) == HAL_OK) {
    adc_value = HAL_ADC_GetValue(_hadc);
  }
  HAL_ADC_Stop(_hadc);

  return adc_value;
}

// 多次采样取平均值
uint16_t Potentiometer::readAverage(uint8_t samples) {
  if (!_initialized || samples == 0)
    return 0;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += readRaw();
    HAL_Delay(5);
  }
  return (uint16_t)(sum / samples);
}

// 读取电压
float Potentiometer::readVoltage(void) {
  uint16_t raw = readRaw();
  return (float)raw * POT_VREF / POT_ADC_MAX_VALUE;
}

// 读取阻值 (线性映射)
float Potentiometer::readResistance(void) {
  uint16_t raw = readRaw();

  // 使用校准后的范围
  if (raw <= _minRaw)
    return _minResistance;
  if (raw >= _maxRaw)
    return _maxResistance;

  float ratio = (float)(raw - _minRaw) / (_maxRaw - _minRaw);
  return _minResistance + ratio * (_maxResistance - _minResistance);
}

// 读取百分比
float Potentiometer::readPercentage(void) {
  uint16_t raw = readRaw();

  if (raw <= _minRaw)
    return 0.0f;
  if (raw >= _maxRaw)
    return 100.0f;

  return (float)(raw - _minRaw) * 100.0f / (_maxRaw - _minRaw);
}

// 读取所有数据
Pot_Data_t Potentiometer::readAll(void) {
  Pot_Data_t data;
  data.adc_raw = readRaw();
  data.voltage = (float)data.adc_raw * POT_VREF / POT_ADC_MAX_VALUE;
  data.resistance = readResistance();
  data.percentage = readPercentage();
  return data;
}

// 设置最大阻值
void Potentiometer::setMaxResistance(float maxRes_kΩ) {
  _maxResistance = maxRes_kΩ;
  LOG_I("POT", "Max resistance set to %.1fkΩ", _maxResistance);
}

// 校准最大值 (将当前 ADC 值设为最大值)
void Potentiometer::calibrateMax(void) {
  _maxRaw = readAverage(10);
  LOG_I("POT", "Calibrated MAX: raw=%d (%.2fV)", _maxRaw,
         (float)_maxRaw * POT_VREF / POT_ADC_MAX_VALUE);
}

// 校准最小值
void Potentiometer::calibrateMin(void) {
  _minRaw = readAverage(10);
  LOG_I("POT", "Calibrated MIN: raw=%d (%.2fV)", _minRaw,
         (float)_minRaw * POT_VREF / POT_ADC_MAX_VALUE);
}