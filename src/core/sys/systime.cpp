/**
 * @file    systime.cpp
 * @brief   RTC time sync — SNTP via ESP8266 AT commands,
 *          HTTP fallback via network_manager.
 *
 * LED rules (consistent with network_manager):
 *   Success → boardLed blink 100 ms, warnLed off
 *   Failure → warnLed ON
 */

#include "systime.h"
#include "../manager/include/network_manager.h"
#include "../manager/include/settings_manager.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/trtc.hpp"
#include "syslog.h"
#include <cstdio>
#include <cstring>

extern TRTC boardTRTC;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t  esp8266_global_buffer[512];
extern uint16_t esp8266_global_index;
extern uint8_t  esp8266_data_ready;
}

struct SysDateTime {
  int year, month, day, hour, minute, second;
};

struct TimeSample {
  SysDateTime dt;
  uint32_t tick_ms;
  bool precise_tick;
};

static const size_t   AT_RX_SIZE               = 512;
static const uint32_t RTC_SET_ADVANCE_MS       = 2;
static const uint32_t TIME_SYNC_LATENCY_COMP_MS = 2300U;
static const uint32_t TIME_SYNC_FIXED_OFFSET_S  = 1U;

/* ── SNTP raw-AT helpers (kept for AT+CIPSNTPTIME? queries) ──── */

static char *at_rx_buf(void) {
  return (char *)esp8266_global_buffer;
}

static void at_reset_shared_rx(void) {
  esp8266_global_index = 0;
  esp8266_data_ready   = 0;
  memset(esp8266_global_buffer, 0, AT_RX_SIZE);
}

static void uart2_discard_pending(uint32_t idle_ms) {
  uint32_t last_rx = HAL_GetTick();
  while (HAL_GetTick() - last_rx < idle_ms) {
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      (void)(uint8_t)(huart2.Instance->DR & 0xFF);
      last_rx = HAL_GetTick();
    }
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
      __HAL_UART_CLEAR_OREFLAG(&huart2);
      last_rx = HAL_GetTick();
    }
  }
}

static void raw_at_begin(void) {
  HAL_NVIC_DisableIRQ(USART2_IRQn);
  at_reset_shared_rx();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  uart2_discard_pending(20);
}

static void raw_at_end(void) {
  uart2_discard_pending(2);
  esp8266_global_index = 0;
  esp8266_data_ready   = 0;
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void raw_append(char c, char *out, size_t out_sz, size_t *len) {
  if (*len + 1 < out_sz) {
    out[*len] = c;
    ++(*len);
    out[*len] = '\0';
  }
}

static bool raw_collect(char *out, size_t out_sz, const char *expected,
                        uint32_t timeout_ms, uint32_t settle_ms,
                        bool closed_is_done, uint32_t *last_rx_tick) {
  uint32_t start   = HAL_GetTick();
  uint32_t last_rx = start;
  size_t   len     = strlen(out);
  bool matched     = false;
  bool failed      = false;

  while (HAL_GetTick() - start < timeout_ms) {
    bool got = false;
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      char c = (char)(huart2.Instance->DR & 0xFF);
      raw_append(c, out, out_sz, &len);
      got     = true;
      last_rx = HAL_GetTick();
      if (last_rx_tick) *last_rx_tick = last_rx;
    }
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
      __HAL_UART_CLEAR_OREFLAG(&huart2);
      last_rx = HAL_GetTick();
      if (last_rx_tick) *last_rx_tick = last_rx;
    }
    if (got) {
      if (expected && strstr(out, expected)) matched = true;
      if (closed_is_done && strstr(out, "CLOSED")) matched = true;
      if (strstr(out, "ERROR") || strstr(out, "FAIL") ||
          strstr(out, "DNS Fail"))
        failed = true;
    }
    if ((matched || failed) && (HAL_GetTick() - last_rx >= settle_ms))
      break;
  }
  return matched && !failed;
}

static bool raw_at_command(const char *cmd, const char *expected,
                           uint32_t timeout_ms, uint32_t settle_ms,
                           char *out, size_t out_sz,
                           uint32_t *last_rx_tick = 0) {
  char tx[128];
  int  n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  if (n <= 0 || n >= (int)sizeof(tx)) return false;
  raw_at_begin();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);
  bool ok = raw_collect(out, out_sz, expected, timeout_ms, settle_ms, false,
                        last_rx_tick);
  raw_at_end();
  return ok;
}

static void log_response_summary(const char *label, const char *resp) {
  char summary[96];
  size_t j = 0;
  if (!resp) resp = "";
  for (size_t i = 0; resp[i] && j + 1 < sizeof(summary); ++i) {
    char c = resp[i];
    if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    if ((unsigned char)c < 32 || (unsigned char)c > 126) c = '.';
    summary[j++] = c;
  }
  summary[j] = '\0';
  LOG_D("SYTM", "%s: %s", label, summary);
}

/* ── Date-time parsing (unchanged) ────────────────────────────── */

static bool is_digit(char c)               { return c >= '0' && c <= '9'; }
static void skip_spaces(const char **p)    { while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') ++(*p); }

static bool read_fixed_digits(const char **p, int count, int *value) {
  int v = 0;
  for (int i = 0; i < count; ++i) { if (!is_digit((*p)[i])) return false; v = v * 10 + ((*p)[i] - '0'); }
  *p += count; *value = v; return true;
}

static bool read_digits(const char **p, int min_count, int max_count, int *value) {
  int v = 0, count = 0;
  while (count < max_count && is_digit(**p)) { v = v * 10 + (**p - '0'); ++(*p); ++count; }
  if (count < min_count) return false;
  *value = v; return true;
}

static bool is_leap_year(int year)          { return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0); }
static int  days_in_month(int year, int m)  { static const int d[]={31,28,31,30,31,30,31,31,30,31,30,31}; if(m==2&&is_leap_year(year))return 29; if(m<1||m>12)return 0; return d[m-1]; }

static bool valid_datetime(const SysDateTime &dt) {
  return dt.year >= 2024 && dt.year <= 2099 &&
         dt.month >= 1 && dt.month <= 12 &&
         dt.day >= 1 && dt.day <= days_in_month(dt.year, dt.month) &&
         dt.hour >= 0 && dt.hour <= 23 &&
         dt.minute >= 0 && dt.minute <= 59 &&
         dt.second >= 0 && dt.second <= 59;
}

static int month_from_name(const char *p) {
  static const char *names[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  for (int i = 0; i < 12; ++i) if (strncmp(p, names[i], 3) == 0) return i + 1;
  return 0;
}

static bool parse_numeric_datetime(const char *buf, SysDateTime *out) {
  for (const char *p = buf; p && *p; ++p) {
    const char *q = p; SysDateTime dt; char sep;
    if (!read_fixed_digits(&q, 4, &dt.year)) continue;
    sep = *q; if (sep != '-' && sep != '/' && sep != '.') continue; ++q;
    if (!read_fixed_digits(&q, 2, &dt.month)) continue; if (*q != sep) continue; ++q;
    if (!read_fixed_digits(&q, 2, &dt.day)) continue;
    while (*q == ' ' || *q == 'T' || *q == '"' || *q == '\'') ++q;
    if (!read_fixed_digits(&q, 2, &dt.hour)) continue; if (*q != ':') continue; ++q;
    if (!read_fixed_digits(&q, 2, &dt.minute)) continue; if (*q != ':') continue; ++q;
    if (!read_fixed_digits(&q, 2, &dt.second)) continue;
    if (valid_datetime(dt)) { *out = dt; return true; }
  }
  return false;
}

static bool parse_esp_asctime(const char *buf, SysDateTime *out) {
  for (const char *p = buf; p && *p; ++p) {
    SysDateTime dt; const char *q;
    dt.month = month_from_name(p); if (dt.month == 0) continue;
    q = p + 3; skip_spaces(&q);
    if (!read_digits(&q, 1, 2, &dt.day)) continue; skip_spaces(&q);
    if (!read_fixed_digits(&q, 2, &dt.hour)) continue; if (*q != ':') continue; ++q;
    if (!read_fixed_digits(&q, 2, &dt.minute)) continue; if (*q != ':') continue; ++q;
    if (!read_fixed_digits(&q, 2, &dt.second)) continue;
    skip_spaces(&q);
    if (!read_fixed_digits(&q, 4, &dt.year)) continue;
    if (valid_datetime(dt)) { *out = dt; return true; }
  }
  return false;
}

static void add_hours(SysDateTime *dt, int hours) {
  dt->hour += hours;
  while (dt->hour >= 24) { dt->hour -= 24; ++dt->day;
    if (dt->day > days_in_month(dt->year, dt->month)) { dt->day = 1; ++dt->month;
      if (dt->month > 12) { dt->month = 1; ++dt->year; } } }
}

static bool parse_http_date_header(const char *buf, SysDateTime *out) {
  const char *p = buf;
  while (p && (p = strstr(p, "Date:")) != 0) {
    SysDateTime dt; const char *q = p + 5, *comma = strchr(q, ',');
    if (comma) q = comma + 1;
    skip_spaces(&q);
    if (!read_digits(&q, 1, 2, &dt.day)) { p += 5; continue; } skip_spaces(&q);
    dt.month = month_from_name(q); if (dt.month == 0) { p += 5; continue; } q += 3; skip_spaces(&q);
    if (!read_fixed_digits(&q, 4, &dt.year)) { p += 5; continue; } skip_spaces(&q);
    if (!read_fixed_digits(&q, 2, &dt.hour)) { p += 5; continue; } if (*q != ':') { p += 5; continue; } ++q;
    if (!read_fixed_digits(&q, 2, &dt.minute)) { p += 5; continue; } if (*q != ':') { p += 5; continue; } ++q;
    if (!read_fixed_digits(&q, 2, &dt.second)) { p += 5; continue; }
    if (valid_datetime(dt)) { add_hours(&dt, 8); if (valid_datetime(dt)) { *out = dt; return true; } }
    p += 5;
  }
  return false;
}

static bool parse_any_datetime(const char *buf, SysDateTime *out) {
  if (!buf || !buf[0]) return false;
  return parse_numeric_datetime(buf, out) ||
         parse_esp_asctime(buf, out)    ||
         parse_http_date_header(buf, out);
}

/* ── Calendar math ────────────────────────────────────────────── */

static uint32_t datetime_to_seconds(const SysDateTime &dt) {
  uint32_t days = 0;
  for (int y = 2000; y < dt.year; ++y) days += is_leap_year(y) ? 366U : 365U;
  for (int m = 1; m < dt.month; ++m) days += (uint32_t)days_in_month(dt.year, m);
  days += (uint32_t)(dt.day - 1);
  return days * 86400U + (uint32_t)dt.hour * 3600U + (uint32_t)dt.minute * 60U + (uint32_t)dt.second;
}

static void seconds_to_datetime(uint32_t seconds, SysDateTime *dt) {
  uint32_t d = seconds / 86400U, rem = seconds % 86400U;
  int y = 2000, m = 1;
  while (1) { uint32_t yd = is_leap_year(y) ? 366U : 365U; if (d < yd) break; d -= yd; ++y; }
  while (1) { uint32_t md = (uint32_t)days_in_month(y, m); if (d < md) break; d -= md; ++m; }
  dt->year = y; dt->month = m; dt->day = (int)d + 1;
  dt->hour = (int)(rem / 3600U); rem %= 3600U;
  dt->minute = (int)(rem / 60U); dt->second = (int)(rem % 60U);
}

static void   add_seconds(SysDateTime *dt, uint32_t s) { seconds_to_datetime(datetime_to_seconds(*dt) + s, dt); }
static int32_t tick_delta(uint32_t a, uint32_t b)      { return (int32_t)(a - b); }

static uint8_t weekday_monday_1(int y, int m, int d) {
  static const int off[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) --y;
  int w = (y + y/4 - y/100 + y/400 + off[m-1] + d) % 7;
  return (uint8_t)(w == 0 ? 7 : w);
}

static void apply_datetime_to_rtc(const SysDateTime &dt) {
  uint8_t wd = weekday_monday_1(dt.year, dt.month, dt.day);
  boardTRTC.setDateTime((uint8_t)(dt.year - 2000), (uint8_t)dt.month,
                        (uint8_t)dt.day, wd, (uint8_t)dt.hour,
                        (uint8_t)dt.minute, (uint8_t)dt.second);
}

static SysDateTime align_and_apply_sample(const TimeSample &sample) {
  uint32_t now = HAL_GetTick();
  uint32_t elapsed = now - sample.tick_ms;
  uint32_t compensated_elapsed = elapsed + TIME_SYNC_LATENCY_COMP_MS;
  uint32_t target_seconds = compensated_elapsed / 1000U + 1U;
  uint32_t target_tick = sample.tick_ms + target_seconds * 1000U;
  uint32_t wait_ms = target_tick - now;
  if (wait_ms < 100U) { ++target_seconds; target_tick += 1000U; wait_ms += 1000U; }
  uint32_t apply_tick = target_tick > RTC_SET_ADVANCE_MS ? target_tick - RTC_SET_ADVANCE_MS : target_tick;
  SysDateTime target = sample.dt;
  add_seconds(&target, target_seconds + TIME_SYNC_FIXED_OFFSET_S);
  LOG_D("SYTM", "RTC align: age=%lums comp=%lums wait=%lums precise=%d fixed=%lus",
        (unsigned long)elapsed, (unsigned long)TIME_SYNC_LATENCY_COMP_MS,
        (unsigned long)wait_ms, sample.precise_tick ? 1 : 0,
        (unsigned long)TIME_SYNC_FIXED_OFFSET_S);
  while (tick_delta(apply_tick, HAL_GetTick()) > 3) HAL_Delay(1);
  while (tick_delta(apply_tick, HAL_GetTick()) > 0) {}
  apply_datetime_to_rtc(target);
  return target;
}

/* ── SNTP time query (ESP8266 AT+CIPSNTPTIME?) ────────────────── */

static bool query_sntp(TimeSample *sample) {
  char *buf = at_rx_buf();
  const char *cfg = "AT+CIPSNTPCFG=1,8,\"ntp.aliyun.com\",\"cn.ntp.org.cn\",\"pool.ntp.org\"";
  bool configured = raw_at_command(cfg, "OK", 5000, 50, buf, AT_RX_SIZE);
  if (!configured) {
    log_response_summary("SNTP cfg", buf);
    LOG_W("SYTM", "SNTP server cfg failed, trying default cfg");
    configured = raw_at_command("AT+CIPSNTPCFG=1,8", "OK", 5000, 50, buf, AT_RX_SIZE);
  }
  if (!configured) {
    log_response_summary("SNTP cfg default", buf);
    LOG_W("SYTM", "SNTP cfg unsupported");
    return false;
  }

  TimeSample prev; bool have_prev = false;
  uint32_t start = HAL_GetTick(); int attempts = 0;

  while (HAL_GetTick() - start < 9000U) {
    SysDateTime dt; TimeSample cur; uint32_t rx_tick = 0;
    bool ok = raw_at_command("AT+CIPSNTPTIME?", "OK", 1200, 25, buf, AT_RX_SIZE, &rx_tick);
    ++attempts;

    if (parse_any_datetime(buf, &dt)) {
      cur.dt = dt; cur.tick_ms = rx_tick ? rx_tick : HAL_GetTick(); cur.precise_tick = false;
      if (have_prev) {
        uint32_t prev_sec = datetime_to_seconds(prev.dt);
        uint32_t cur_sec  = datetime_to_seconds(cur.dt);
        if (cur_sec > prev_sec) {
          uint32_t gap = cur.tick_ms - prev.tick_ms;
          if (cur_sec - prev_sec == 1U && gap <= 1500U) {
            cur.tick_ms = prev.tick_ms + gap / 2U;
            cur.precise_tick = true;
          }
          *sample = cur;
          LOG_I("SYTM", "SNTP second edge captured");
          return true;
        }
      }
      prev = cur; have_prev = true;
    }

    if (!ok && buf && strstr(buf, "ERROR")) {
      log_response_summary("SNTP query", buf);
      LOG_W("SYTM", "SNTP query unsupported");
      return false;
    }

    if ((attempts % 10) == 0) {
      log_response_summary("SNTP raw", buf);
      LOG_D("SYTM", "Waiting for SNTP second edge (%d)", attempts);
    }
    HAL_Delay(have_prev ? 80U : 250U);
  }

  if (have_prev) { *sample = prev; LOG_W("SYTM", "SNTP edge not captured, using last sample"); return true; }
  LOG_W("SYTM", "SNTP did not return a valid time");
  return false;
}

/* ── HTTP fallback time query (via network_manager) ───────────── */

static bool query_http_time(TimeSample *sample) {
  struct { const char *host; const char *path; } static const endpoints[] = {
    {"quan.suning.com", "/getSysTime.do"},
    {"www.baidu.com",   "/"},
  };

  char resp[1024];

  for (unsigned i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); ++i) {
    LOG_I("SYTM", "HTTP time via %s", endpoints[i].host);

    memset(resp, 0, sizeof(resp));
    if (!Net_HttpGet(endpoints[i].host, 80, endpoints[i].path,
                     resp, sizeof(resp), 8000)) {
      LOG_W("SYTM", "HTTP GET failed: %s", endpoints[i].host);
      continue;
    }

    /* Parse Date header from the raw HTTP response */
    SysDateTime dt;
    if (parse_any_datetime(resp, &dt)) {
      sample->dt           = dt;
      sample->tick_ms      = HAL_GetTick();
      sample->precise_tick = false;
      LOG_I("SYTM", "HTTP response parsed — time from Date header");
      return true;
    }

    LOG_W("SYTM", "No valid time found in response from %s", endpoints[i].host);
  }

  return false;
}

/* ── Public API ───────────────────────────────────────────────── */

extern "C" void time_fmt(char *buf, int sz, int h24, int m) {
  if (SM_Time_Style24h()) snprintf(buf, sz, "%02d:%02d", h24, m);
  else { int h12 = h24 % 12; if (h12 == 0) h12 = 12; snprintf(buf, sz, "%02d:%02d", h12, m); }
}

bool SysTime_Sync(void) {
  TimeSample sample;
  SysDateTime synced;

  LOG_I("SYTM", "ESP8266 time sync...");

  /* ── Pre-checks ─────────────────────────────────────── */
  if (ESP8266_IsHardDisabled()) {
    LOG_W("SYTM", "ESP8266 is hard-disabled — cannot sync");
    Net_LedFailure();
    return false;
  }

  if (!ESP8266_IsConnected()) {
    LOG_W("SYTM", "WiFi is not connected");
    Net_LedFailure();
    return false;
  }

  /* ── Prepare client mode (best-effort) ──────────────── */
  Net_PrepareClient();

  if (!Net_HasStationIP()) {
    LOG_E("SYTM", "ESP8266 has no STA IP");
    Net_LedFailure();
    return false;
  }

  /* ── Try SNTP first, fall back to HTTP ──────────────── */
  bool ok = query_sntp(&sample);
  if (!ok) {
    LOG_W("SYTM", "SNTP failed, trying HTTP fallback...");
    ok = query_http_time(&sample);
  }

  if (!ok) {
    LOG_E("SYTM", "Time sync failed — SNTP and HTTP both unavailable");
    Net_LedFailure();
    return false;
  }

  /* ── Apply to RTC ───────────────────────────────────── */
  synced = align_and_apply_sample(sample);
  LOG_I("SYTM", "Synced: %04d-%02d-%02d %02d:%02d:%02d",
        synced.year, synced.month, synced.day,
        synced.hour, synced.minute, synced.second);

  Net_LedSuccess();
  return true;
}
