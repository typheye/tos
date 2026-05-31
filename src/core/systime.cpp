/**
 * @file    systime.cpp
 * @brief   RTC time sync through ESP8266 AT commands.
 */

#include "include/systime.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/trtc.hpp"
#include "syslog.h"
#include <cstdio>
#include <cstring>

extern TRTC boardTRTC;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t esp8266_global_buffer[512];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
}

struct SysDateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

struct TimeSample {
  SysDateTime dt;
  uint32_t tick_ms;
  bool precise_tick;
};

static const size_t AT_RX_SIZE = 512;
static const uint32_t RTC_SET_ADVANCE_MS = 2;

static bool parse_any_datetime(const char *buf, SysDateTime *out);

static char *at_rx_buf(void) {
  return (char *)esp8266_global_buffer;
}

static void at_reset_shared_rx(void) {
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
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
  esp8266_data_ready = 0;
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
  uint32_t start = HAL_GetTick();
  uint32_t last_rx = start;
  size_t len = strlen(out);
  bool matched = false;
  bool failed = false;

  while (HAL_GetTick() - start < timeout_ms) {
    bool got = false;

    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      char c = (char)(huart2.Instance->DR & 0xFF);
      raw_append(c, out, out_sz, &len);
      got = true;
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
          strstr(out, "DNS Fail")) {
        failed = true;
      }
    }

    if ((matched || failed) && (HAL_GetTick() - last_rx >= settle_ms)) break;
  }

  return matched && !failed;
}

static bool raw_at_command(const char *cmd, const char *expected,
                           uint32_t timeout_ms, uint32_t settle_ms,
                           char *out, size_t out_sz,
                           uint32_t *last_rx_tick = 0) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);

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

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static void skip_spaces(const char **p) {
  while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') {
    ++(*p);
  }
}

static bool read_fixed_digits(const char **p, int count, int *value) {
  int v = 0;
  const char *q = *p;

  for (int i = 0; i < count; ++i) {
    if (!is_digit(*q)) return false;
    v = v * 10 + (*q - '0');
    ++q;
  }

  *p = q;
  *value = v;
  return true;
}

static bool read_digits(const char **p, int min_count, int max_count,
                        int *value) {
  int v = 0;
  int count = 0;
  const char *q = *p;

  while (count < max_count && is_digit(*q)) {
    v = v * 10 + (*q - '0');
    ++q;
    ++count;
  }

  if (count < min_count) return false;
  *p = q;
  *value = v;
  return true;
}

static bool is_leap_year(int year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int days_in_month(int year, int month) {
  static const int days[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (month == 2 && is_leap_year(year)) return 29;
  if (month < 1 || month > 12) return 0;
  return days[month - 1];
}

static bool valid_datetime(const SysDateTime &dt) {
  if (dt.year < 2024 || dt.year > 2099) return false;
  if (dt.month < 1 || dt.month > 12) return false;
  if (dt.day < 1 || dt.day > days_in_month(dt.year, dt.month)) return false;
  if (dt.hour < 0 || dt.hour > 23) return false;
  if (dt.minute < 0 || dt.minute > 59) return false;
  if (dt.second < 0 || dt.second > 59) return false;
  return true;
}

static int month_from_name(const char *p) {
  static const char *names[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  for (int i = 0; i < 12; ++i) {
    if (strncmp(p, names[i], 3) == 0) return i + 1;
  }
  return 0;
}

static bool parse_numeric_datetime(const char *buf, SysDateTime *out) {
  for (const char *p = buf; p && *p; ++p) {
    const char *q = p;
    SysDateTime dt;
    char sep;

    if (!read_fixed_digits(&q, 4, &dt.year)) continue;
    sep = *q;
    if (sep != '-' && sep != '/' && sep != '.') continue;
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.month)) continue;
    if (*q != sep) continue;
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.day)) continue;

    while (*q == ' ' || *q == 'T' || *q == '"' || *q == '\'') ++q;

    if (!read_fixed_digits(&q, 2, &dt.hour)) continue;
    if (*q != ':') continue;
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.minute)) continue;
    if (*q != ':') continue;
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.second)) continue;

    if (valid_datetime(dt)) {
      *out = dt;
      return true;
    }
  }

  return false;
}

static bool parse_esp_asctime(const char *buf, SysDateTime *out) {
  for (const char *p = buf; p && *p; ++p) {
    SysDateTime dt;
    const char *q;

    dt.month = month_from_name(p);
    if (dt.month == 0) continue;

    q = p + 3;
    skip_spaces(&q);
    if (!read_digits(&q, 1, 2, &dt.day)) continue;
    skip_spaces(&q);
    if (!read_fixed_digits(&q, 2, &dt.hour)) continue;
    if (*q != ':') continue;
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.minute)) continue;
    if (*q != ':') continue;
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.second)) continue;
    skip_spaces(&q);
    if (!read_fixed_digits(&q, 4, &dt.year)) continue;

    if (valid_datetime(dt)) {
      *out = dt;
      return true;
    }
  }

  return false;
}

static void add_hours(SysDateTime *dt, int hours) {
  dt->hour += hours;

  while (dt->hour >= 24) {
    dt->hour -= 24;
    ++dt->day;
    if (dt->day > days_in_month(dt->year, dt->month)) {
      dt->day = 1;
      ++dt->month;
      if (dt->month > 12) {
        dt->month = 1;
        ++dt->year;
      }
    }
  }
}

static bool parse_http_date_header(const char *buf, SysDateTime *out) {
  const char *p = buf;

  while (p && (p = strstr(p, "Date:")) != 0) {
    SysDateTime dt;
    const char *q = p + 5;
    const char *comma = strchr(q, ',');

    if (comma) q = comma + 1;
    skip_spaces(&q);
    if (!read_digits(&q, 1, 2, &dt.day)) {
      p += 5;
      continue;
    }
    skip_spaces(&q);
    dt.month = month_from_name(q);
    if (dt.month == 0) {
      p += 5;
      continue;
    }
    q += 3;
    skip_spaces(&q);
    if (!read_fixed_digits(&q, 4, &dt.year)) {
      p += 5;
      continue;
    }
    skip_spaces(&q);
    if (!read_fixed_digits(&q, 2, &dt.hour)) {
      p += 5;
      continue;
    }
    if (*q != ':') {
      p += 5;
      continue;
    }
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.minute)) {
      p += 5;
      continue;
    }
    if (*q != ':') {
      p += 5;
      continue;
    }
    ++q;
    if (!read_fixed_digits(&q, 2, &dt.second)) {
      p += 5;
      continue;
    }

    if (valid_datetime(dt)) {
      add_hours(&dt, 8);
      if (valid_datetime(dt)) {
        *out = dt;
        return true;
      }
    }

    p += 5;
  }

  return false;
}

static bool parse_any_datetime(const char *buf, SysDateTime *out) {
  if (!buf || !buf[0]) return false;
  if (parse_numeric_datetime(buf, out)) return true;
  if (parse_esp_asctime(buf, out)) return true;
  if (parse_http_date_header(buf, out)) return true;
  return false;
}

static uint32_t datetime_to_seconds(const SysDateTime &dt) {
  uint32_t days = 0;

  for (int y = 2000; y < dt.year; ++y) {
    days += is_leap_year(y) ? 366U : 365U;
  }

  for (int m = 1; m < dt.month; ++m) {
    days += (uint32_t)days_in_month(dt.year, m);
  }

  days += (uint32_t)(dt.day - 1);
  return days * 86400U + (uint32_t)dt.hour * 3600U +
         (uint32_t)dt.minute * 60U + (uint32_t)dt.second;
}

static void seconds_to_datetime(uint32_t seconds, SysDateTime *dt) {
  uint32_t days = seconds / 86400U;
  uint32_t rem = seconds % 86400U;
  int year = 2000;
  int month = 1;

  while (1) {
    uint32_t yd = is_leap_year(year) ? 366U : 365U;
    if (days < yd) break;
    days -= yd;
    ++year;
  }

  while (1) {
    uint32_t md = (uint32_t)days_in_month(year, month);
    if (days < md) break;
    days -= md;
    ++month;
  }

  dt->year = year;
  dt->month = month;
  dt->day = (int)days + 1;
  dt->hour = (int)(rem / 3600U);
  rem %= 3600U;
  dt->minute = (int)(rem / 60U);
  dt->second = (int)(rem % 60U);
}

static void add_seconds(SysDateTime *dt, uint32_t seconds) {
  seconds_to_datetime(datetime_to_seconds(*dt) + seconds, dt);
}

static int32_t tick_delta(uint32_t a, uint32_t b) {
  return (int32_t)(a - b);
}

static uint8_t weekday_monday_1(int year, int month, int day) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

  if (month < 3) --year;
  int w = (year + year / 4 - year / 100 + year / 400 +
           offsets[month - 1] + day) % 7;
  return (uint8_t)(w == 0 ? 7 : w);
}

static void apply_datetime_to_rtc(const SysDateTime &dt) {
  uint8_t weekday = weekday_monday_1(dt.year, dt.month, dt.day);

  boardTRTC.setDateTime((uint8_t)(dt.year - 2000), (uint8_t)dt.month,
                        (uint8_t)dt.day, weekday, (uint8_t)dt.hour,
                        (uint8_t)dt.minute, (uint8_t)dt.second);
}

static SysDateTime align_and_apply_sample(const TimeSample &sample) {
  uint32_t now = HAL_GetTick();
  uint32_t elapsed = now - sample.tick_ms;
  uint32_t target_seconds = elapsed / 1000U + 1U;
  uint32_t target_tick = sample.tick_ms + target_seconds * 1000U;
  uint32_t wait_ms = target_tick - now;
  if (wait_ms < 100U) {
    ++target_seconds;
    target_tick += 1000U;
    wait_ms += 1000U;
  }
  uint32_t apply_tick =
      target_tick > RTC_SET_ADVANCE_MS ? target_tick - RTC_SET_ADVANCE_MS
                                       : target_tick;
  SysDateTime target = sample.dt;

  add_seconds(&target, target_seconds);

  LOG_D("SYTM", "RTC align: age=%lums wait=%lums precise=%d",
        (unsigned long)elapsed, (unsigned long)wait_ms,
        sample.precise_tick ? 1 : 0);

  while (tick_delta(apply_tick, HAL_GetTick()) > 3) {
    HAL_Delay(1);
  }
  while (tick_delta(apply_tick, HAL_GetTick()) > 0) {
  }

  apply_datetime_to_rtc(target);
  return target;
}

static bool station_has_ip(void) {
  char *buf = at_rx_buf();
  bool status_ok = raw_at_command("AT+CIPSTATUS", "OK", 2500, 40,
                                  buf, AT_RX_SIZE);

  if (status_ok) {
    log_response_summary("CIPSTATUS", buf);
  } else {
    log_response_summary("CIPSTATUS fail", buf);
  }

  bool cifr_ok = raw_at_command("AT+CIFSR", "OK", 2500, 40, buf, AT_RX_SIZE);
  log_response_summary(cifr_ok ? "CIFSR" : "CIFSR fail", buf);

  if (!cifr_ok) return status_ok;
  if (strstr(buf, "STAIP") && !strstr(buf, "\"0.0.0.0\"")) return true;
  return false;
}

static void prepare_esp_client_mode(void) {
  char *buf = at_rx_buf();

  if (!raw_at_command("AT+CIPCLOSE", "OK", 1500, 30, buf, AT_RX_SIZE)) {
    raw_at_command("AT+CIPCLOSE", "CLOSED", 1500, 30, buf, AT_RX_SIZE);
  }

  if (!raw_at_command("AT+CIPMUX=0", "OK", 2000, 40, buf, AT_RX_SIZE)) {
    log_response_summary("CIPMUX=0", buf);
  }
}

static bool query_sntp(TimeSample *sample) {
  char *buf = at_rx_buf();
  const char *cfg_with_servers =
      "AT+CIPSNTPCFG=1,8,\"ntp.aliyun.com\",\"cn.ntp.org.cn\",\"pool.ntp.org\"";
  bool configured = raw_at_command(cfg_with_servers, "OK", 5000, 50,
                                   buf, AT_RX_SIZE);

  if (!configured) {
    log_response_summary("SNTP cfg", buf);
    LOG_W("SYTM", "SNTP server cfg failed, trying default cfg");
    configured = raw_at_command("AT+CIPSNTPCFG=1,8", "OK", 5000, 50,
                                buf, AT_RX_SIZE);
  }

  if (!configured) {
    log_response_summary("SNTP cfg default", buf);
    LOG_W("SYTM", "SNTP cfg unsupported");
    return false;
  }

  TimeSample prev;
  bool have_prev = false;
  uint32_t start = HAL_GetTick();
  int attempts = 0;

  while (HAL_GetTick() - start < 9000U) {
    SysDateTime dt;
    TimeSample cur;
    uint32_t rx_tick = 0;
    bool ok = raw_at_command("AT+CIPSNTPTIME?", "OK", 1200, 25,
                             buf, AT_RX_SIZE, &rx_tick);
    ++attempts;

    if (parse_any_datetime(buf, &dt)) {
      cur.dt = dt;
      cur.tick_ms = rx_tick ? rx_tick : HAL_GetTick();
      cur.precise_tick = false;

      if (have_prev) {
        uint32_t prev_sec = datetime_to_seconds(prev.dt);
        uint32_t cur_sec = datetime_to_seconds(cur.dt);

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

      prev = cur;
      have_prev = true;
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

  if (have_prev) {
    *sample = prev;
    LOG_W("SYTM", "SNTP edge not captured, using last sample");
    return true;
  }

  LOG_W("SYTM", "SNTP did not return a valid time");
  return false;
}

static bool tcp_start(const char *host, uint16_t port) {
  char *buf = at_rx_buf();
  char cmd[96];
  int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                   host, (unsigned)port);

  if (n <= 0 || n >= (int)sizeof(cmd)) return false;
  bool ok = raw_at_command(cmd, "CONNECT", 8000, 80, buf, AT_RX_SIZE);
  if (!ok) {
    log_response_summary("CIPSTART", buf);
  }
  return ok;
}

static bool raw_http_request(const char *host, const char *path,
                             TimeSample *sample) {
  char *buf = at_rx_buf();
  char request[192];
  char cmd[32];
  SysDateTime dt;
  uint32_t rx_tick = 0;
  int len = snprintf(request, sizeof(request),
                     "GET %s HTTP/1.0\r\n"
                     "Host: %s\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     path, host);

  if (len <= 0 || len >= (int)sizeof(request)) return false;
  int cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
  if (cmd_len <= 0 || cmd_len >= (int)sizeof(cmd)) return false;

  raw_at_begin();

  char tx[40];
  int tx_len = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)tx_len, 1000);
  bool prompt = raw_collect(buf, AT_RX_SIZE, ">", 3000, 20, false, 0);
  if (!prompt) {
    log_response_summary("CIPSEND", buf);
    raw_at_end();
    return false;
  }

  memset(buf, 0, AT_RX_SIZE);
  HAL_UART_Transmit(&huart2, (uint8_t *)request, (uint16_t)len, 2000);
  raw_collect(buf, AT_RX_SIZE, "CLOSED", 6000, 120, true, &rx_tick);
  bool parsed = parse_any_datetime(buf, &dt);
  if (!parsed) log_response_summary("HTTP raw", buf);

  raw_at_end();
  if (parsed) {
    sample->dt = dt;
    sample->tick_ms = rx_tick ? rx_tick : HAL_GetTick();
    sample->precise_tick = false;
  }
  return parsed;
}

static bool query_http_time(TimeSample *sample) {
  struct Endpoint {
    const char *host;
    const char *path;
  };

  static const Endpoint endpoints[] = {
      {"quan.suning.com", "/getSysTime.do"},
      {"www.baidu.com", "/"}};

  prepare_esp_client_mode();

  for (unsigned i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); ++i) {
    LOG_I("SYTM", "HTTP time via %s", endpoints[i].host);

    if (!tcp_start(endpoints[i].host, 80)) {
      LOG_W("SYTM", "TCP connect failed: %s", endpoints[i].host);
      continue;
    }

    if (raw_http_request(endpoints[i].host, endpoints[i].path, sample)) {
      raw_at_command("AT+CIPCLOSE", "CLOSED", 1500, 40,
                     at_rx_buf(), AT_RX_SIZE);
      LOG_I("SYTM", "HTTP response parsed");
      return true;
    }

    raw_at_command("AT+CIPCLOSE", "CLOSED", 1500, 40,
                   at_rx_buf(), AT_RX_SIZE);
  }

  return false;
}

bool SysTime_Sync(void) {
  TimeSample sample;
  SysDateTime synced;

  LOG_I("SYTM", "ESP8266 time sync...");

  if (!ESP8266_IsConnected()) {
    LOG_W("SYTM", "WiFi is not connected");
    return false;
  }

  prepare_esp_client_mode();

  if (!station_has_ip()) {
    LOG_E("SYTM", "ESP8266 has no STA IP");
    return false;
  }

  if (!query_sntp(&sample)) {
    LOG_W("SYTM", "SNTP failed, trying HTTP");
    if (!query_http_time(&sample)) {
      LOG_E("SYTM", "Time sync failed");
      return false;
    }
  }

  synced = align_and_apply_sample(sample);
  LOG_I("SYTM", "Synced: %04d-%02d-%02d %02d:%02d:%02d",
        synced.year, synced.month, synced.day, synced.hour, synced.minute,
        synced.second);
  return true;
}
