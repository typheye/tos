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


/* Hand-rolled uint32→dec (avoids newlib-nano snprintf issues) */
static int u32dec(uint32_t v, char *b) {
  if (!v) { b[0] = '0'; return 1; }
  char t[12]; int n = 0;
  while (v) { t[n++] = '0' + (v % 10); v /= 10; }
  for (int i = 0; i < n; i++) b[i] = t[n - 1 - i];
  return n;
}

/* Return formatted timestamp "[sssss.mmm]" in a static buffer */
const char *syslog_ts(void) {
  static char buf[16];
  uint32_t t = SysLog_GetTick();
  uint32_t sec = t / 1000u;
  uint32_t ms  = t % 1000u;
  int p = 0;
  buf[p++] = '[';
  /* seconds: right-aligned in 5 */
  char ts[12]; int n = u32dec(sec, ts);
  for (int i = n; i < 5; i++) buf[p++] = ' ';
  for (int i = 0; i < n; i++) buf[p++] = ts[i];
  buf[p++] = '.';
  /* milliseconds: zero-padded 3 digits */
  buf[p++] = '0' + (ms / 100);
  buf[p++] = '0' + ((ms / 10) % 10);
  buf[p++] = '0' + (ms % 10);
  buf[p++] = ']';
  buf[p] = '\0';
  return buf;
}

__attribute__((weak))
uint32_t SysLog_GetTick(void) { return HAL_GetTick(); }
