/**
 ******************************************************************************
 * @file    tcs3472.hpp
 * @author  Typheye
 * @brief   Tcs3472 interface.
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

#ifndef TCS3472_HPP
#define TCS3472_HPP

#include "main.h"
#include <stdint.h>
#include "core/sys/include/syslog.h"
#include <math.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


#define TCS3472_ADDR_7BIT 0x29
#define TCS3472_ADDR_8BIT (TCS3472_ADDR_7BIT << 1) // 0x52


#define TCS3472_COMMAND_BIT 0x80
#define TCS3472_COMMAND_TYPE 0x00


#define TCS3472_ENABLE 0x00
#define TCS3472_ATIME 0x01
#define TCS3472_WTIME 0x03
#define TCS3472_AILTL 0x04
#define TCS3472_AILTH 0x05
#define TCS3472_AIHTL 0x06
#define TCS3472_AIHTH 0x07
#define TCS3472_PERS 0x0C
#define TCS3472_CONFIG 0x0D
#define TCS3472_CONTROL 0x0F
#define TCS3472_ID 0x12
#define TCS3472_STATUS 0x13
#define TCS3472_CDATAL 0x14
#define TCS3472_CDATAH 0x15
#define TCS3472_RDATAL 0x16
#define TCS3472_RDATAH 0x17
#define TCS3472_GDATAL 0x18
#define TCS3472_GDATAH 0x19
#define TCS3472_BDATAL 0x1A
#define TCS3472_BDATAH 0x1B



#define TCS3472_ENABLE_AIEN 0x10
#define TCS3472_ENABLE_WEN 0x08
#define TCS3472_ENABLE_AEN 0x02
#define TCS3472_ENABLE_PON 0x01


#define TCS3472_STATUS_AINT 0x10
#define TCS3472_STATUS_AVALID 0x01


#define TCS3472_CONTROL_AGAIN_1X 0x00
#define TCS3472_CONTROL_AGAIN_4X 0x01
#define TCS3472_CONTROL_AGAIN_16X 0x02
#define TCS3472_CONTROL_AGAIN_60X 0x03


typedef struct {
  uint16_t clear;
  uint16_t red;
  uint16_t green;
  uint16_t blue;
  uint16_t ir;
} TCS3472_RawData_t;

typedef struct {
  float red;
  float green;
  float blue;
  float color_temp;
  float lux;
} TCS3472_ColorData_t;


class TCS3472 {
public:
  TCS3472();


  void init(void);
  bool isInitialized(void) { return _initialized; }


  void setGain(uint8_t gain);


  void setIntegrationTime(uint8_t atime);


  void enable(void);
  void disable(void);


  TCS3472_RawData_t readRaw(void);


  TCS3472_ColorData_t readColor(void);
  float getLux(void);  // quick ambient light reading


  void ledOn(void);
  void ledOff(void);
  void ledSet(bool on);


  uint8_t readID(void);


  void softReset(void);


  bool waitForData(uint32_t timeout_ms);

private:
  I2C_HandleTypeDef *_hi2c;
  uint16_t _addr;
  bool _initialized;
  bool _last_read_ok;
  uint8_t _gain;
  uint8_t _atime;


  uint8_t readReg(uint8_t reg);
  void writeReg(uint8_t reg, uint8_t value);
  uint16_t readReg16(uint8_t reg);
  bool readBytes(uint8_t reg, uint8_t *data, uint16_t length);


  float calculateColorTemperature(uint16_t r, uint16_t g, uint16_t b);
  float calculateLux(uint16_t c, uint16_t r, uint16_t g, uint16_t b);
};


extern TCS3472 boardTCS3472;

#ifdef __cplusplus
}
#endif

#endif /* TCS3472_HPP */