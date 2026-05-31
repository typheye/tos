#ifndef __RTC_HPP
#define __RTC_HPP

#include "main.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"
#include <cstring>
#include <stdio.h>

// 时间结构体（简化版）
typedef struct {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} Time_t;

// 日期结构体
typedef struct {
  uint8_t year;    // 相对于 2000 年的偏移，如 25 表示 2025 年
  uint8_t month;   // 1-12
  uint8_t date;    // 1-31
  uint8_t weekday; // 1-7 (周一=1, 周日=7，根据 HAL 定义)
} Date_t;

class TRTC { // ✅ 改名
public:
  TRTC(); // ✅ 改名
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

extern TRTC boardTRTC; // ✅ 改名

#endif
