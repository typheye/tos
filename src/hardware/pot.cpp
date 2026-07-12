/**
 ******************************************************************************
 * @file    pot.cpp
 * @author  Typheye
 * @brief   Pot implementation.
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

#include "include/pot.hpp"
#include "library/include/libdly.h"



extern ADC_HandleTypeDef hadc1;


Potentiometer boardPot;


Potentiometer::Potentiometer() {
  _hadc = &hadc1;
  _initialized = false;
  _maxResistance = POT_MAX_RESISTANCE;
  _minResistance = 0.0f;
  _minRaw = 0;
  _maxRaw = POT_ADC_MAX_VALUE;
}


void Potentiometer::init(void) {
  if (_initialized)
    return;


  LOG_I("POT", "Potentiometer Driver Initialized");
  LOG_I("POT", "ADC Channel: PC0 (ADC123_IN10)");
  LOG_I("POT", "Reference Voltage: %.2fV", POT_VREF);
  LOG_I("POT", "Max Resistance: %.1fkΩ", _maxResistance);

  _initialized = true;


  LOG_I("POT", "Test reading: %d (%.2fV)", (int)readRaw(), (double)readVoltage());
}


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


uint16_t Potentiometer::readAverage(uint8_t samples) {
  if (!_initialized || samples == 0)
    return 0;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += readRaw();
    JPDelay(5);
  }
  return (uint16_t)(sum / samples);
}


float Potentiometer::readVoltage(void) {
  uint16_t raw = readRaw();
  return (float)raw * POT_VREF / POT_ADC_MAX_VALUE;
}


float Potentiometer::readResistance(void) {
  uint16_t raw = readRaw();


  if (raw <= _minRaw)
    return _minResistance;
  if (raw >= _maxRaw)
    return _maxResistance;

  float ratio = (float)(raw - _minRaw) / (_maxRaw - _minRaw);
  return _minResistance + ratio * (_maxResistance - _minResistance);
}


float Potentiometer::readPercentage(void) {
  uint16_t raw = readRaw();

  if (raw <= _minRaw)
    return 0.0f;
  if (raw >= _maxRaw)
    return 100.0f;

  return (float)(raw - _minRaw) * 100.0f / (_maxRaw - _minRaw);
}


Pot_Data_t Potentiometer::readAll(void) {
  Pot_Data_t data;
  data.adc_raw = readRaw();
  data.voltage = (float)data.adc_raw * POT_VREF / POT_ADC_MAX_VALUE;
  data.resistance = readResistance();
  data.percentage = readPercentage();
  return data;
}


void Potentiometer::setMaxResistance(float maxRes_kΩ) {
  _maxResistance = maxRes_kΩ;
  LOG_I("POT", "Max resistance set to %.1fkΩ", _maxResistance);
}


void Potentiometer::calibrateMax(void) {
  _maxRaw = readAverage(10);
  LOG_I("POT", "Calibrated MAX: raw=%d (%.2fV)", _maxRaw,
         (float)_maxRaw * POT_VREF / POT_ADC_MAX_VALUE);
}


void Potentiometer::calibrateMin(void) {
  _minRaw = readAverage(10);
  LOG_I("POT", "Calibrated MIN: raw=%d (%.2fV)", _minRaw,
         (float)_minRaw * POT_VREF / POT_ADC_MAX_VALUE);
}