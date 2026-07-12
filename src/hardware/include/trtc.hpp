/**
 ******************************************************************************
 * @file    trtc.hpp
 * @author  Typheye
 * @brief   Trtc interface.
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

#ifndef TRTC_HPP
#define TRTC_HPP

#include "main.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"
#include <cstring>
#include <stdio.h>
#include "core/sys/include/syslog.h"


typedef struct {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} Time_t;


typedef struct {
  uint8_t year;
  uint8_t month;   // 1-12
  uint8_t date;    // 1-31
  uint8_t weekday;
} Date_t;

class TRTC {
public:
  TRTC();
  void init();
  void setTime(uint8_t hours, uint8_t minutes, uint8_t seconds);
  void setDate(uint8_t year, uint8_t month, uint8_t date, uint8_t weekday);
  void setDateTime(uint8_t year, uint8_t month, uint8_t date, uint8_t weekday,
                   uint8_t hours, uint8_t minutes, uint8_t seconds);
  void getTime(Time_t *time);
  void getDate(Date_t *date);
  void getDateTime(Time_t *time, Date_t *date);
  uint32_t getTimestamp(void);
  void print(void);

private:
  bool initialized;
  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;

  void syncToHAL(void);
  void syncFromHAL(void);
};

extern TRTC boardTRTC;

#endif /* TRTC_HPP */
