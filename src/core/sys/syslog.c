/**
 ******************************************************************************
 * @file    syslog.c
 * @author  Typheye
 * @brief   Syslog implementation.
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

#include "include/syslog.h"

extern FRESULT FMCore_AppendBootLog(const char *line, uint32_t len);
extern bool FMCore_IsBootLogFaultFatal(void);
extern FRESULT FMCore_WriteSystemDump(uint32_t code, const char *name,
                                      const char *extra);
extern int ESP8266_GetState(void);
extern bool ESP8266_IsHardDisabled(void);
extern bool TSDIO_IsHardDisabled(void);
extern bool TSDIO_IsInitialized(void);
extern uint32_t SysHandle_CodeFromFResult(FRESULT res, uint32_t fallback);
extern void SysHandle_ExceptionNoDump(uint32_t code);
extern bool SysHandle_IsInException(void);
extern RTC_HandleTypeDef hrtc;

static uint8_t g_syslog_file_guard = 0;
static uint8_t g_syslog_file_disabled = 0;
static uint8_t g_syslog_storage_faulting = 0;

#ifndef SYS_ERR_SD_LOG_FAILED
#define SYS_ERR_SD_LOG_FAILED 0x0000100BUL
#endif

static const char *syslog_level_name(SysLog_Level_t level) {
  switch (level) {
  case SYSLOG_FATAL: return "FATAL";
  case SYSLOG_ERROR: return "ERROR";
  case SYSLOG_WARN:  return "WARN ";
  case SYSLOG_INFO:  return "INFO ";
  case SYSLOG_DEBUG: return "DEBUG";
  default: return "LOG  ";
  }
}


/* Hand-rolled uint32→dec (avoids newlib-nano snprintf issues) */
static int u32dec(uint32_t v, char *b) {
  if (!v) { b[0] = '0'; return 1; }
  char t[12]; int n = 0;
  while (v) { t[n++] = '0' + (v % 10); v /= 10; }
  for (int i = 0; i < n; i++) b[i] = t[n - 1 - i];
  return n;
}

/* Return a fixed digit-budget timestamp.
 * 0..99999s  : "[sssss.mmm]"
 * 100000s+   : "[ssssss.mm]", then "[sssssss.m]"
 * The bracket field keeps 8 numeric digits; the dot moves right when uptime
 * needs more integer digits.
 */
const char *syslog_ts(void) {
  static char buf[16];
  uint32_t t = SysLog_GetTick();
  uint32_t sec = t / 1000u;
  uint32_t ms  = t % 1000u;
  int p = 0;
  buf[p++] = '[';
  char ts[12]; int n = u32dec(sec, ts);

  int frac_digits = 8 - n;
  if (frac_digits > 3) frac_digits = 3;
  if (frac_digits < 0) frac_digits = 0;

  for (int i = n; i < 5; i++) buf[p++] = ' ';
  for (int i = 0; i < n; i++) buf[p++] = ts[i];
  if (frac_digits > 0) {
    buf[p++] = '.';
    if (frac_digits >= 1) buf[p++] = '0' + (ms / 100);
    if (frac_digits >= 2) buf[p++] = '0' + ((ms / 10) % 10);
    if (frac_digits >= 3) buf[p++] = '0' + (ms % 10);
  }
  buf[p++] = ']';
  buf[p] = '\0';
  return buf;
}

__attribute__((weak))
uint32_t SysLog_GetTick(void) { return HAL_GetTick(); }

void SysLog_DisableFileOutput(void) {
  g_syslog_file_disabled = 1U;
}

bool SysLog_IsFileOutputDisabled(void) {
  return g_syslog_file_disabled != 0U;
}

static void syslog_handle_file_result(FRESULT res) {
  uint32_t code;

  if (res == FR_OK) return;
  if (!FMCore_IsBootLogFaultFatal()) return;

  g_syslog_file_disabled = 1U;
  if (g_syslog_storage_faulting || SysHandle_IsInException()) return;
  g_syslog_storage_faulting = 1U;

  code = SysHandle_CodeFromFResult(res, SYS_ERR_SD_LOG_FAILED);
  printf("%s [ERROR] [SYS  ] SD log write failed: %d, entering syshandle without dump\r\n",
         syslog_ts(), (int)res);
  SysHandle_ExceptionNoDump(code);
}

static void syslog_emit_v(SysLog_Level_t level, const char *mod,
                          const char *fmt, va_list ap) {
  char msg[224];
  char line[320];
  int n;
  size_t len;

  if (!fmt) fmt = "";
  if (!mod) mod = "SYS";

  n = vsnprintf(msg, sizeof(msg), fmt, ap);
  if (n < 0) {
    strcpy(msg, "(log format error)");
  } else {
    msg[sizeof(msg) - 1U] = '\0';
  }

  n = snprintf(line, sizeof(line) - 2U, "%s [%s] [%-5s] %s",
               syslog_ts(), syslog_level_name(level), mod, msg);
  if (n < 0) return;
  line[sizeof(line) - 3U] = '\0';
  len = strlen(line);
  if (len + 2U < sizeof(line)) {
    line[len++] = '\r';
    line[len++] = '\n';
    line[len] = '\0';
  }

  printf("%s", line);

  if (level <= SYSLOG_FILE_MAX_LEVEL &&
      !g_syslog_file_guard && !g_syslog_file_disabled) {
    FRESULT res;
    g_syslog_file_guard = 1U;
    res = FMCore_AppendBootLog(line, (uint32_t)len);
    g_syslog_file_guard = 0U;
    syslog_handle_file_result(res);
  }
}

void SysLog_Print(SysLog_Level_t level, const char *mod, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  syslog_emit_v(level, mod, fmt, ap);
  va_end(ap);
}

void SysLog_Write(SysLog_Level_t level, const char *mod, const char *task,
                  const char *file, int line_no, const char *fmt, ...) {
  (void)task;
  (void)file;
  (void)line_no;
  va_list ap;
  va_start(ap, fmt);
  syslog_emit_v(level, mod, fmt, ap);
  va_end(ap);
}

void SysLog_WriteFatalDump(uint32_t code, const char *name) {
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  char rtc_line[48];
  char extra[256];
  int esp_state = ESP8266_GetState();
  bool esp_disabled = ESP8266_IsHardDisabled();
  bool sdio_disabled = TSDIO_IsHardDisabled();
  bool sdio_ready = TSDIO_IsInitialized();

  strcpy(rtc_line, "rtc=unknown\r\n");
  if (hrtc.Instance == RTC) {
    if (HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN) == HAL_OK &&
        HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN) == HAL_OK) {
      snprintf(rtc_line, sizeof(rtc_line),
               "rtc=20%02u-%02u-%02u %02u:%02u:%02u wd=%u\r\n",
               d.Year, d.Month, d.Date,
               t.Hours, t.Minutes, t.Seconds, d.WeekDay);
    }
  }

  snprintf(extra, sizeof(extra),
           "tick_ms=%lu\r\n"
           "%s"
           "esp_state=%d\r\n"
           "esp_hard_disabled=%u\r\n"
           "sdio_initialized=%u\r\n"
           "sdio_hard_disabled=%u\r\n",
           (unsigned long)HAL_GetTick(),
           rtc_line,
           esp_state,
           esp_disabled ? 1U : 0U,
           sdio_ready ? 1U : 0U,
           sdio_disabled ? 1U : 0U);

  (void)FMCore_WriteSystemDump(code, name, extra);
}
