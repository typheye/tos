/**
 ******************************************************************************
 * @file    syslog.h
 * @author  Typheye
 * @brief   Syslog interface.
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

#ifndef SYSLOG_H
#define SYSLOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Log levels (matching syslog severity) ========== */
typedef enum {
  SYSLOG_FATAL = 0,   /* system is unusable */
  SYSLOG_ERROR = 1,   /* error conditions */
  SYSLOG_WARN  = 2,   /* warning conditions */
  SYSLOG_INFO  = 3,   /* informational */
  SYSLOG_DEBUG = 4,   /* debug-level messages */
} SysLog_Level_t;

/* ========== Compile-time max level (override in project config) ========== */
#ifndef SYSLOG_MAX_LEVEL
#define SYSLOG_MAX_LEVEL  SYSLOG_DEBUG
#endif

#ifndef SYSLOG_FILE_MAX_LEVEL
#define SYSLOG_FILE_MAX_LEVEL SYSLOG_INFO
#endif

/* ========== Recommended module tags (5 chars max, passed as string literal) ==========
 * MAIN  – main / TOS init           FLASH – sfhd Flash driver
 * SMGR  – settings manager          RTC   – real-time clock
 * SDIO  – SD card driver            ESP   – ESP8266 WiFi
 * LCD   – display driver            TCS   – TCS3472 color sensor
 * BMP   – BMP180 pressure           JYRO  – JY901S gyro
 * POT   – potentiometer             HC00  – SN74HC00N logic
 * KEY   – key manager               LED   – LED driver
 * BUZZ  – buzzer                    WLAN  – WiFi settings
 * HOTS  – hotspot settings          DSPL  – display settings
 * SND   – sound settings            STOR  – storage settings
 * PET   – launcher / pet            EHW   – expression hardware
 * PD    – libpd display library     KB    – keyboard
 * SYS   – syscalls / system         FS    – file system
 * DEMO  – demo activities           I2C   – I2C scanner
 * JACT  – JY901S activity           TACT  – TCS3472 activity
 * BACT  – BMP180 activity           HACT  – HC00N activity
 * DACT  – display demo              PACT  – potentiometer activity
 * SDAC  – SD card activity          KACT  – key test activity
 * EACT  – ESP8266 activity          3DOX  – 3D demo
 */

/* ========== ANSI color codes ========== */
#define SYSLOG_CLR_NONE   ""
#define SYSLOG_CLR_RED    "\x1b[31m"
#define SYSLOG_CLR_GREEN  "\x1b[32m"
#define SYSLOG_CLR_YELLOW "\x1b[33m"
#define SYSLOG_CLR_MAG    "\x1b[35m"
#define SYSLOG_CLR_CYAN   "\x1b[36m"
#define SYSLOG_CLR_RESET  "\x1b[0m"

/* ========== Core output function ========== */

/**
 * @brief  Format and emit a log line
 * @param  level  Severity
 * @param  mod    Module tag (5 chars max, will be right-padded)
 * @param  task   Task / context name (9 chars max, may be "")
 * @param  file   Source filename (or "" to omit location)
 * @param  line   Source line number (0 to omit)
 * @param  fmt    printf-style format string
 * @param  ...    Format arguments
 */
void SysLog_Write(SysLog_Level_t level, const char *mod, const char *task,
                  const char *file, int line, const char *fmt, ...);
void SysLog_Print(SysLog_Level_t level, const char *mod, const char *fmt, ...);
void SysLog_WriteFatalDump(uint32_t code, const char *name);
void SysLog_DisableFileOutput(void);
bool SysLog_IsFileOutputDisabled(void);

/**
 * @brief  Get the millisecond tick used for timestamps
 * @note   Default: HAL_GetTick(). Override by redefining the weak symbol.
 */
uint32_t SysLog_GetTick(void);

/* ========== Timestamp helper ========== */
const char *syslog_ts(void);  /* fixed 8-digit timestamp from HAL_GetTick() */

/* ========== Per-level macros — printf DIRECT ========== */

#define LOG_FATAL(mod, task, fmt, ...) \
  SysLog_Print(SYSLOG_FATAL, mod, fmt, ##__VA_ARGS__)

#define LOG_ERROR(mod, task, fmt, ...) \
  SysLog_Print(SYSLOG_ERROR, mod, fmt, ##__VA_ARGS__)

#define LOG_WARN(mod, task, fmt, ...) \
  SysLog_Print(SYSLOG_WARN, mod, fmt, ##__VA_ARGS__)

#if SYSLOG_MAX_LEVEL >= 3
#define LOG_INFO(mod, task, fmt, ...) \
  SysLog_Print(SYSLOG_INFO, mod, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(...)  ((void)0)
#endif

#if SYSLOG_MAX_LEVEL >= 4
#define LOG_DEBUG(mod, task, fmt, ...) \
  SysLog_Print(SYSLOG_DEBUG, mod, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

/* ========== Shortcut macros ========== */
#define LOG_F(mod, fmt, ...)  SysLog_Print(SYSLOG_FATAL, mod, fmt, ##__VA_ARGS__)
#define LOG_E(mod, fmt, ...)  SysLog_Print(SYSLOG_ERROR, mod, fmt, ##__VA_ARGS__)
#define LOG_W(mod, fmt, ...)  SysLog_Print(SYSLOG_WARN, mod, fmt, ##__VA_ARGS__)
#define LOG_I(mod, fmt, ...)  SysLog_Print(SYSLOG_INFO, mod, fmt, ##__VA_ARGS__)
#define LOG_D(mod, fmt, ...)  SysLog_Print(SYSLOG_DEBUG, mod, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* SYSLOG_H */
