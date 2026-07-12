/**
 ******************************************************************************
 * @file    pot.hpp
 * @author  Typheye
 * @brief   Pot interface.
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

#ifndef POT_HPP
#define POT_HPP

#include "main.h"
#include <stdint.h>
#include "core/sys/include/syslog.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif



#define POT_ADC_HANDLE hadc1
#define POT_ADC_CHANNEL ADC_CHANNEL_10
#define POT_ADC_GPIO_PORT GPIOC
#define POT_ADC_GPIO_PIN GPIO_PIN_0


#define POT_ADC_MAX_VALUE 4095.0f
#define POT_VREF 3.30f
#define POT_MAX_RESISTANCE 10.0f


typedef struct {
  uint16_t adc_raw;
  float voltage;
  float resistance;
  float percentage;
} Pot_Data_t;


class Potentiometer {
public:
  Potentiometer();


  void init(void);
  bool isInitialized(void) { return _initialized; }


  uint16_t readRaw(void);


  float readVoltage(void);


  float readResistance(void);


  float readPercentage(void);


  Pot_Data_t readAll(void);


  uint16_t readAverage(uint8_t samples);


  void setMaxResistance(float maxRes_kΩ);


  void calibrateMax(void);


  void calibrateMin(void);


  float getMinResistance(void) { return _minResistance; }
  float getMaxResistance(void) { return _maxResistance; }
  uint16_t getMinRaw(void) { return _minRaw; }
  uint16_t getMaxRaw(void) { return _maxRaw; }

private:
  ADC_HandleTypeDef *_hadc;
  bool _initialized;
  float _maxResistance;
  float _minResistance;
  uint16_t _minRaw;
  uint16_t _maxRaw;
};


extern Potentiometer boardPot;

#ifdef __cplusplus
}
#endif

#endif /* POT_HPP */