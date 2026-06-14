/**
 ******************************************************************************
 * @file    trtc.cpp
 * @author  Typheye
 * @brief   Trtc implementation.
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

#include "include/trtc.hpp"
#include "library/include/libdly.h"


extern RTC_HandleTypeDef hrtc;

TRTC boardTRTC;


TRTC::TRTC() {
  initialized = false;
  memset(&sTime, 0, sizeof(sTime));
  memset(&sDate, 0, sizeof(sDate));
}

void TRTC::syncFromHAL() {
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

void TRTC::syncToHAL() {
  HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}


void TRTC::init() {
  if (initialized)
    return;

  LOG_I("RTC", "Initializing...");

  
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  
  uint32_t start = HAL_GetTick();
  uint8_t lse_ready = 0;

  LOG_I("RTC", "Waiting for LSE oscillator...");
  while ((HAL_GetTick() - start) < 3000) {
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY)) {
      lse_ready = 1;
      LOG_I("RTC", "LSE ready after %lu ms", (unsigned long)(HAL_GetTick() - start));
      break;
    }
    JPDelay(50);
  }

  if (!lse_ready) {
    LOG_W("RTC", "LSE not ready! RTC may not work correctly");
  }

  
  HAL_StatusTypeDef ret;
  int retry = 5;

  do {
    
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    ret = HAL_RTC_Init(&hrtc);

    if (ret != HAL_OK) {
      LOG_E("RTC", "Init failed (ret=%d), retry %d...", ret, 6 - retry);
      JPDelay(100);
    }
    retry--;
  } while (ret != HAL_OK && retry > 0);

  if (ret != HAL_OK) {
    LOG_F("RTC", "Cannot initialize RTC!");
    initialized = true; 
    return;
  }

  
  syncFromHAL();

  
  if (sTime.Hours > 23 || sTime.Minutes > 59 || sTime.Seconds > 59 ||
      sDate.Year > 99 || sDate.Month > 12 || sDate.Date > 31) {
    LOG_W("RTC", "Invalid time detected, setting default...");

    
    sTime.Hours = 0;
    sTime.Minutes = 0;
    sTime.Seconds = 0;
    sTime.TimeFormat = RTC_HOURFORMAT_24;
    sDate.Year = 25; 
    sDate.Month = 1;
    sDate.Date = 1;
    sDate.WeekDay = 4; 

    syncToHAL();
    syncFromHAL();
  }

  initialized = true;
  LOG_I("RTC", "Initialized successfully: %04d-%02d-%02d %02d:%02d:%02d",
         2000 + sDate.Year, sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes,
         sTime.Seconds);
}

void TRTC::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
  if (!initialized)
    init();

  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
  sTime.Hours = hours;
  sTime.Minutes = minutes;
  sTime.Seconds = seconds;
  sTime.TimeFormat = RTC_HOURFORMAT_24;
  syncToHAL();
  syncFromHAL();

  LOG_I("RTC", "Time set to %02d:%02d:%02d", hours, minutes, seconds);
}

void TRTC::setDate(uint8_t year, uint8_t month, uint8_t date, uint8_t weekday) {
  if (!initialized)
    init();

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  sDate.Year = year;
  sDate.Month = month;
  sDate.Date = date;
  sDate.WeekDay = weekday;
  syncToHAL();
  syncFromHAL();

  LOG_I("RTC", "Date set to %04d-%02d-%02d (weekday=%d)", 2000 + year,
         month, date, weekday);
}

void TRTC::setDateTime(uint8_t year, uint8_t month, uint8_t date,
                       uint8_t weekday, uint8_t hours, uint8_t minutes,
                       uint8_t seconds) {
  if (!initialized)
    init();

  sTime.Hours = hours;
  sTime.Minutes = minutes;
  sTime.Seconds = seconds;
  sTime.TimeFormat = RTC_HOURFORMAT_24;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sDate.Year = year;
  sDate.Month = month;
  sDate.Date = date;
  sDate.WeekDay = weekday;

  syncToHAL();
  syncFromHAL();

  LOG_I("RTC", "DateTime set to %04d-%02d-%02d %02d:%02d:%02d (weekday=%d)",
         2000 + year, month, date, hours, minutes, seconds, weekday);
}

void TRTC::getTime(Time_t *time) {
  if (!initialized)
    init();
  syncFromHAL();
  time->hours = sTime.Hours;
  time->minutes = sTime.Minutes;
  time->seconds = sTime.Seconds;
}

void TRTC::getDate(Date_t *date) {
  if (!initialized)
    init();
  syncFromHAL();
  date->year = sDate.Year;
  date->month = sDate.Month;
  date->date = sDate.Date;
  date->weekday = sDate.WeekDay;
}

void TRTC::getDateTime(Time_t *time, Date_t *date) {
  getTime(time);
  getDate(date);
}

uint32_t TRTC::getTimestamp(void) {
  if (!initialized)
    init();
  syncFromHAL();

  uint32_t days = 0;
  for (uint16_t y = 2000; y < 2000 + sDate.Year; y++) {
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
      days += 366;
    } else {
      days += 365;
    }
  }

  const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (uint8_t m = 1; m < sDate.Month; m++) {
    days += monthDays[m - 1];
    if (m == 2 &&
        (((2000 + sDate.Year) % 4 == 0 && (2000 + sDate.Year) % 100 != 0) ||
         ((2000 + sDate.Year) % 400 == 0))) {
      days += 1;
    }
  }

  days += sDate.Date - 1;

  uint32_t timestamp = days * 86400;
  timestamp += sTime.Hours * 3600;
  timestamp += sTime.Minutes * 60;
  timestamp += sTime.Seconds;

  return timestamp;
}

void TRTC::print(void) {
  if (!initialized)
    return;

  syncFromHAL();
  LOG_I("RTC", "%04d-%02d-%02d %02d:%02d:%02d WeekDay:%d", 2000 + sDate.Year,
         sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes, sTime.Seconds,
         sDate.WeekDay);
}
