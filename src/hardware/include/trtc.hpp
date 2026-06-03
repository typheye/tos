/**
 ******************************************************************************
 * @file    trtc.hpp
 * @author  Typheye
 * @brief   Trtc interface.
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

#ifndef __RTC_HPP
#define __RTC_HPP

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

#endif
