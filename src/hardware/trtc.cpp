#include "include/trtc.hpp"

extern RTC_HandleTypeDef hrtc;

TRTC boardTRTC; // ✅ 改名

// 构造函数
TRTC::TRTC() { // ✅ 改名
  initialized = false;
  memset(&sTime, 0, sizeof(sTime));
  memset(&sDate, 0, sizeof(sDate));
}

void TRTC::syncFromHAL() { // ✅ 改名
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

void TRTC::syncToHAL() { // ✅ 改名
  HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

void TRTC::init() { // ✅ 改名
  if (initialized)
    return;

  HAL_Delay(100);

  syncFromHAL();
  initialized = true;
}

void TRTC::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds) { // ✅ 改名
  if (!initialized)
    init();
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
  sTime.Hours = hours;
  sTime.Minutes = minutes;
  sTime.Seconds = seconds;
  sTime.TimeFormat = RTC_HOURFORMAT_24;
  syncToHAL();
  syncFromHAL();
}

void TRTC::setDate(uint8_t year, uint8_t month, uint8_t date,
                   uint8_t weekday) { // ✅ 改名
  if (!initialized)
    init();
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  sDate.Year = year;
  sDate.Month = month;
  sDate.Date = date;
  sDate.WeekDay = weekday;
  syncToHAL();
  syncFromHAL();
}

void TRTC::getTime(Time_t *time) { // ✅ 改名
  if (!initialized)
    init();
  syncFromHAL();
  time->hours = sTime.Hours;
  time->minutes = sTime.Minutes;
  time->seconds = sTime.Seconds;
}

void TRTC::getDate(Date_t *date) { // ✅ 改名
  if (!initialized)
    init();
  syncFromHAL();
  date->year = sDate.Year;
  date->month = sDate.Month;
  date->date = sDate.Date;
  date->weekday = sDate.WeekDay;
}

void TRTC::getDateTime(Time_t *time, Date_t *date) { // ✅ 改名
  getTime(time);
  getDate(date);
}

uint32_t TRTC::getTimestamp(void) { // ✅ 改名
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

void TRTC::print(void) { // ✅ 改名
  if (!initialized)
    return;

  syncFromHAL();
  printf("RTC: %04d-%02d-%02d %02d:%02d:%02d WeekDay:%d\r\n", 2000 + sDate.Year,
         sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes, sTime.Seconds,
         sDate.WeekDay);
}