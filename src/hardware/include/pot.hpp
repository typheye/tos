/**
 ******************************************************************************
 * @file    pot.hpp
 * @author  Typheye
 * @brief   Pot interface.
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

#ifndef __POT_HPP
#define __POT_HPP

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 引脚定义 ====================
// 电位器连接到 PC0 (ADC123_IN10)
#define POT_ADC_HANDLE hadc1
#define POT_ADC_CHANNEL ADC_CHANNEL_10
#define POT_ADC_GPIO_PORT GPIOC
#define POT_ADC_GPIO_PIN GPIO_PIN_0

// ==================== 参数配置 ====================
#define POT_ADC_MAX_VALUE 4095.0f // 12位 ADC 最大值
#define POT_VREF 3.30f            // 参考电压 (V)
#define POT_MAX_RESISTANCE 10.0f  // 最大阻值 (kΩ)，根据实际电位器修改

// ==================== 数据结构 ====================
typedef struct {
  uint16_t adc_raw; // ADC 原始值 (0-4095)
  float voltage;    // 电压 (V)
  float resistance; // 阻值 (kΩ)
  float percentage; // 百分比 (0-100%)
} Pot_Data_t;

// ==================== 类接口 ====================
class Potentiometer {
public:
  Potentiometer();

  // 初始化 ADC
  void init(void);
  bool isInitialized(void) { return _initialized; }

  // 读取 ADC 原始值
  uint16_t readRaw(void);

  // 读取电压 (V)
  float readVoltage(void);

  // 读取阻值 (kΩ)
  float readResistance(void);

  // 读取百分比 (0-100%)
  float readPercentage(void);

  // 读取所有数据
  Pot_Data_t readAll(void);

  // 多次采样取平均值
  uint16_t readAverage(uint8_t samples);

  // 设置最大阻值 (用于校准)
  void setMaxResistance(float maxRes_kΩ);

  // 校准：设置当前值为最大值
  void calibrateMax(void);

  // 校准：设置当前值为最小值
  void calibrateMin(void);

  // 获取校准值
  float getMinResistance(void) { return _minResistance; }
  float getMaxResistance(void) { return _maxResistance; }
  uint16_t getMinRaw(void) { return _minRaw; }
  uint16_t getMaxRaw(void) { return _maxRaw; }

private:
  ADC_HandleTypeDef *_hadc;
  bool _initialized;
  float _maxResistance; // 最大阻值 (kΩ)
  float _minResistance; // 最小阻值 (kΩ) - 校准用
  uint16_t _minRaw;     // 最小原始值 - 校准用
  uint16_t _maxRaw;     // 最大原始值 - 校准用
};

// 全局实例
extern Potentiometer boardPot;

#ifdef __cplusplus
}
#endif

#endif /* __POT_HPP */