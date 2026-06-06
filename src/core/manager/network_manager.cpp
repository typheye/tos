/**
 ******************************************************************************
 * @file    network_manager.cpp
 * @author  Typheye
 * @brief   ESP8266 HTTP network manager implementation.
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

#include "include/network_manager.h"


/* ── Externals ────────────────────────────────────────────────── */

extern ESP8266 esp8266;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t  esp8266_global_buffer[];
extern uint16_t esp8266_global_index;
extern uint8_t  esp8266_data_ready;
}

static const size_t NET_RX_SIZE = 2048;
static const size_t NET_ASYNC_RESPONSE_SIZE = 3072;

/* The ESP8266 AT interface is strictly serial.  Synchronous raw AT helpers
 * call SysWatchdog_Tick() while waiting for replies; the watchdog may in turn
 * try to advance the async HTTP state machine.  Guard raw AT sections so no
 * async command is emitted while another AT command is still in progress. */
static volatile uint8_t g_net_raw_busy = 0;

/* ── LED helpers ──────────────────────────────────────────────── */

static void net_led_success(void) {
  /* Do not spend 100ms here.  Synchronous GET/POST already costs enough time;
   * a blocking LED blink was visible as UI and auto-brightness jitter. */
  warnLed.off();
  boardLed.on();
  HAL_Delay(6);
  boardLed.off();
  SysWatchdog_Tick();
}

static void net_led_failure(void) {
  LED_WarnBlink300ms();
}

/* ── Low-level UART helpers ───────────────────────────────────── */

static void net_rx_reset(void) {
  esp8266_global_index = 0;
  esp8266_data_ready   = 0;
  memset(esp8266_global_buffer, 0, NET_RX_SIZE);
}

static void net_uart_discard(uint32_t idle_ms) {
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
    SysWatchdog_Tick();
  }
}

static void net_raw_begin(void) {
  g_net_raw_busy = 1;
  HAL_NVIC_DisableIRQ(USART2_IRQn);
  net_rx_reset();
  esp8266.resetRxBuffer();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  net_uart_discard(20);
}

static void net_raw_end(void) {
  net_uart_discard(2);
  net_rx_reset();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  g_net_raw_busy = 0;
}

static bool net_has_token(const char *buf, const char *a,
                          const char *b = nullptr,
                          const char *c = nullptr) {
  return (a && strstr(buf, a)) ||
         (b && strstr(buf, b)) ||
         (c && strstr(buf, c));
}

/* ── AT command core ──────────────────────────────────────────── */

static bool net_raw_collect(const char *ok1, const char *ok2,
                            const char *ok3, uint32_t timeout_ms,
                            uint32_t settle_ms, bool closed_ok,
                            bool fail_on_error) {
  uint32_t start   = HAL_GetTick();
  uint32_t last_rx = start;
  bool matched     = false;
  bool failed      = false;

  while (HAL_GetTick() - start < timeout_ms) {
    bool got = false;

    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      uint8_t c = (uint8_t)(huart2.Instance->DR & 0xFF);
      esp8266.processRxData(&c, 1);
      got     = true;
      last_rx = HAL_GetTick();
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
      __HAL_UART_CLEAR_OREFLAG(&huart2);
      last_rx = HAL_GetTick();
    }

    if (got) {
      const char *buf = esp8266.getRxBuffer();
      if (esp8266.hasRxOverflow()) {
        LOG_E("NET", "RX overflow while waiting for AT response");
        failed = true;
        break;
      }
      if (net_has_token(buf, ok1, ok2, ok3)) matched = true;
      if (closed_ok && strstr(buf, "CLOSED")) matched = true;
      if (fail_on_error &&
          (strstr(buf, "ERROR") || strstr(buf, "FAIL") ||
           strstr(buf, "DNS Fail"))) {
        failed = true;
      }
    }

    if ((matched || failed) && (HAL_GetTick() - last_rx >= settle_ms))
      break;
    SysWatchdog_Tick();
  }

  return matched && !failed;
}

static bool net_raw_at(const char *cmd, const char *ok1,
                       uint32_t timeout_ms, uint32_t settle_ms,
                       const char *ok2 = nullptr,
                       const char *ok3 = nullptr) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  if (n <= 0 || n >= (int)sizeof(tx)) return false;

  net_raw_begin();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);
  bool ok = net_raw_collect(ok1, ok2, ok3, timeout_ms, settle_ms, false, true);
  net_raw_end();
  return ok;
}

static bool net_raw_at_best_effort(const char *cmd, const char *ok1,
                                   uint32_t timeout_ms, uint32_t settle_ms,
                                   const char *ok2 = nullptr,
                                   const char *ok3 = nullptr) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  if (n <= 0 || n >= (int)sizeof(tx)) return false;

  net_raw_begin();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);
  /* Cleanup commands intentionally accept ERROR because no active connection /
   * unsupported AT subcommand is a harmless outcome in best-effort recovery. */
  bool ok = net_raw_collect(ok1, ok2, ok3, timeout_ms, settle_ms, false, false);
  net_raw_end();
  return ok;
}

static bool net_raw_at_capture(const char *cmd, const char *ok1,
                               uint32_t timeout_ms, uint32_t settle_ms,
                               char *out, size_t out_sz,
                               const char *ok2 = nullptr,
                               const char *ok3 = nullptr) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  if (out && out_sz > 0U) out[0] = '\0';
  if (n <= 0 || n >= (int)sizeof(tx)) return false;

  net_raw_begin();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);
  bool ok = net_raw_collect(ok1, ok2, ok3, timeout_ms, settle_ms, false, true);
  if (out && out_sz > 0U) {
    const char *rx = esp8266.getRxBuffer();
    size_t copy_len = rx ? strlen(rx) : 0U;
    if (copy_len >= out_sz) copy_len = out_sz - 1U;
    if (rx && copy_len > 0U) memcpy(out, rx, copy_len);
    out[copy_len] = '\0';
  }
  net_raw_end();
  return ok;
}

static void net_log_response_text(const char *label, const char *resp) {
  char summary[128];
  size_t j = 0;

  for (size_t i = 0; resp && resp[i] && j + 1 < sizeof(summary); ++i) {
    char c = resp[i];
    if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    if ((unsigned char)c < 32 || (unsigned char)c > 126) c = '.';
    summary[j++] = c;
  }
  summary[j] = '\0';
  LOG_D("NET", "%s: %s", label, summary);
}

static void net_log_response(const char *label) {
  net_log_response_text(label, esp8266.getRxBuffer());
}

static const char *net_http_header_start(const char *rx) {
  if (!rx) return nullptr;
  return strstr(rx, "HTTP/");
}

static const char *net_http_header_end(const char *rx) {
  const char *h = net_http_header_start(rx);
  if (!h) return nullptr;
  return strstr(h, "\r\n\r\n");
}

static const char *net_http_body_start(const char *rx) {
  const char *eoh = net_http_header_end(rx);
  if (eoh) return eoh + 4;
  /* Never use a CRLFCRLF before HTTP/ as the body boundary. ESP8266 often
   * prepends "Recv ... SEND OK" before +IPD.  Using that early blank line was
   * the root cause of API code=-1 and missed commands after the backend added
   * more headers. */
  const char *h = net_http_header_start(rx);
  const char *p = h ? strchr(h, '{') : strchr(rx, '{');
  return p;
}

static int net_http_content_length(const char *rx) {
  const char *h = net_http_header_start(rx);
  const char *e = net_http_header_end(rx);
  if (!h) return -1;
  if (!e) e = h + strlen(h);

  const char *p = h;
  while (p && p < e) {
    const char *hit = strstr(p, "Content-Length:");
    const char *hit2 = strstr(p, "content-length:");
    if (!hit || (hit2 && hit2 < hit)) hit = hit2;
    if (!hit || hit >= e) return -1;
    p = strchr(hit, ':');
    if (!p || p >= e) return -1;
    p++;
    while (p < e && *p == ' ') p++;
    int v = 0;
    bool any = false;
    while (p < e && *p >= '0' && *p <= '9') {
      any = true;
      v = v * 10 + (*p - '0');
      p++;
    }
    return any ? v : -1;
  }
  return -1;
}

static size_t net_http_body_rx_len(const char *body) {
  if (!body) return 0;
  const char *closed = strstr(body, "CLOSED");
  size_t n = closed ? (size_t)(closed - body) : strlen(body);
  while (n > 0) {
    char c = body[n - 1];
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') n--;
    else break;
  }
  return n;
}

static bool net_http_body_complete(const char *rx) {
  const char *body = net_http_body_start(rx);
  if (!body) return false;

  int clen = net_http_content_length(rx);
  size_t n = net_http_body_rx_len(body);
  if (clen >= 0) {
    return n >= (size_t)clen;
  }

  if (n == 0) return false;
  return body[n - 1] == '}' || body[n - 1] == ']';
}

/* ── DNS cache for TCP connect target ──────────────────────────── */

static char g_dns_host[64];
static char g_dns_ip[24];
static uint32_t g_dns_ok_ms = 0;
static uint32_t g_dns_fail_ms = 0;

static void net_dns_cache_clear(void) {
  g_dns_host[0] = '\0';
  g_dns_ip[0] = '\0';
  g_dns_ok_ms = 0;
  g_dns_fail_ms = 0;
}

static bool net_is_ipv4_literal(const char *s) {
  if (!s || !*s) return false;
  uint8_t dots = 0;
  uint8_t digits = 0;
  int octet = 0;
  bool have_digit = false;
  for (const char *p = s; ; ++p) {
    char c = *p;
    if (c >= '0' && c <= '9') {
      have_digit = true;
      digits++;
      octet = octet * 10 + (c - '0');
      if (octet > 255 || digits > 3) return false;
    } else if (c == '.' || c == '\0') {
      if (!have_digit) return false;
      if (c == '\0') return dots == 3;
      dots++;
      digits = 0;
      octet = 0;
      have_digit = false;
      if (dots > 3) return false;
    } else {
      return false;
    }
  }
}

static bool net_extract_cipdomain_ip(const char *resp, char *out, size_t out_sz) {
  if (!resp || !out || out_sz == 0U) return false;
  const char *p = strstr(resp, "+CIPDOMAIN:");
  if (!p) return false;
  p += strlen("+CIPDOMAIN:");
  while (*p == ' ' || *p == '\t' || *p == '\"') p++;
  char ip[24];
  size_t n = 0;
  while (*p && *p != '\r' && *p != '\n' && *p != '\"' &&
         n + 1U < sizeof(ip)) {
    ip[n++] = *p++;
  }
  ip[n] = '\0';
  if (!net_is_ipv4_literal(ip)) return false;
  snprintf(out, out_sz, "%s", ip);
  return true;
}

static bool net_resolve_host_cached(const char *host, char *out, size_t out_sz) {
  if (!host || !out || out_sz == 0U) return false;
  out[0] = '\0';

  if (net_is_ipv4_literal(host)) {
    snprintf(out, out_sz, "%s", host);
    return true;
  }

  uint32_t now = HAL_GetTick();
  if (strcmp(g_dns_host, host) == 0 && net_is_ipv4_literal(g_dns_ip) &&
      (uint32_t)(now - g_dns_ok_ms) < 1800000U) {
    snprintf(out, out_sz, "%s", g_dns_ip);
    return true;
  }

  /* If DNS just failed, fall back to the domain for a while.  This keeps cloud
   * usable on AT firmwares without CIPDOMAIN support. */
  if (strcmp(g_dns_host, host) == 0 && g_dns_fail_ms != 0U &&
      (uint32_t)(now - g_dns_fail_ms) < 120000U) {
    return false;
  }

  char cmd[128];
  char resp[192];
  int n = snprintf(cmd, sizeof(cmd), "AT+CIPDOMAIN=\"%s\"", host);
  if (n <= 0 || n >= (int)sizeof(cmd)) return false;

  bool ok = net_raw_at_capture(cmd, "OK", 5000, 80, resp, sizeof(resp),
                               "+CIPDOMAIN:");
  if (ok && net_extract_cipdomain_ip(resp, out, out_sz)) {
    snprintf(g_dns_host, sizeof(g_dns_host), "%s", host);
    snprintf(g_dns_ip, sizeof(g_dns_ip), "%s", out);
    g_dns_ok_ms = now;
    g_dns_fail_ms = 0;
    LOG_I("NET", "DNS cache %s -> %s", host, out);
    return true;
  }

  snprintf(g_dns_host, sizeof(g_dns_host), "%s", host);
  g_dns_fail_ms = now;
  net_log_response_text("CIPDOMAIN fail", resp);
  return false;
}

static const char *net_connect_host(const char *host, char *tmp, size_t tmp_sz) {
  if (net_resolve_host_cached(host, tmp, tmp_sz)) return tmp;
  return host;
}

/* ── Public API ───────────────────────────────────────────────── */

/* ---- Non-blocking HTTP POST --------------------------------------------- */

enum NetAsyncStep {
  NET_ASYNC_STEP_IDLE = 0,
  NET_ASYNC_STEP_CLOSE,
  NET_ASYNC_STEP_MUX,
  NET_ASYNC_STEP_START,
  NET_ASYNC_STEP_SEND_LEN,
  NET_ASYNC_STEP_WAIT_RESP,
};

struct NetAsyncCtx {
  NetAsyncState_t state;
  NetAsyncStep step;
  char host[64];
  char connect_host[64];
  uint16_t port;
  char request[1024];
  uint16_t req_len;
  char response[NET_ASYNC_RESPONSE_SIZE];
  uint32_t timeout_ms;
  uint32_t step_start_ms;
  uint32_t last_rx_ms;
  uint16_t last_rx_len;
  uint32_t request_start_ms;
};

static NetAsyncCtx g_async = {NET_ASYNC_IDLE};
static uint8_t g_async_fail_streak = 0;
static uint8_t g_async_cipstart_fail_streak = 0;
static uint32_t g_async_tcp_cooldown_until_ms = 0;

static bool net_async_busy(void) {
  return g_async.state == NET_ASYNC_BUSY;
}

static bool net_async_time_due(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

static void net_async_tcp_stack_cleanup(void) {
  LOG_W("NET", "Async TCP close cleanup after CIPSTART failure");

  /* Keep recovery short.  Each new request already sends AT+CIPMUX=0 before
   * CIPSTART, so doing another blocking CIPMUX here only creates slow frames
   * and increases the chance of overlapping stale +IPD/CLOSED bytes. */
  (void)net_raw_at_best_effort("AT+CIPCLOSE", "OK", 450, 10,
                               "CLOSED", "ERROR");

  g_async_tcp_cooldown_until_ms = HAL_GetTick() + 800U;
}

static void net_async_rx_reset(void) {
  HAL_NVIC_DisableIRQ(USART2_IRQn);
  esp8266.resetRxBuffer();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void net_async_send_line(NetAsyncStep step, const char *cmd) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  if (n <= 0 || n >= (int)sizeof(tx)) {
    g_async.state = NET_ASYNC_FAILED;
    g_async.step = NET_ASYNC_STEP_IDLE;
    return;
  }

  net_async_rx_reset();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);
  g_async.step = step;
  g_async.step_start_ms = HAL_GetTick();
  g_async.last_rx_ms = g_async.step_start_ms;
  g_async.last_rx_len = 0;
}

static void net_async_pump_rx(void) {
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
    __HAL_UART_CLEAR_OREFLAG(&huart2);
  }
  esp8266.processPendingData();

  const char *rx = esp8266.getRxBuffer();
  if (esp8266.hasRxOverflow()) {
    g_async.last_rx_len = esp8266.getRxLength();
    g_async.last_rx_ms = HAL_GetTick();
    return;
  }
  uint16_t len = rx ? (uint16_t)strlen(rx) : 0;
  if (len != g_async.last_rx_len) {
    g_async.last_rx_len = len;
    g_async.last_rx_ms = HAL_GetTick();
  }
}

static bool net_async_has(const char *a, const char *b = nullptr,
                          const char *c = nullptr) {
  const char *rx = esp8266.getRxBuffer();
  return rx && ((a && strstr(rx, a)) || (b && strstr(rx, b)) ||
                (c && strstr(rx, c)));
}

static bool net_async_failed_token(void) {
  const char *rx = esp8266.getRxBuffer();
  return rx && (strstr(rx, "ERROR") || strstr(rx, "FAIL") ||
                strstr(rx, "DNS Fail"));
}

static void net_async_close_best_effort(void) {
  const char close_cmd[] = "AT+CIPCLOSE\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)close_cmd,
                    (uint16_t)(sizeof(close_cmd) - 1U), 100);
  SysWatchdog_Tick();
}

static void net_async_finish(bool ok, const char *reason) {
  if (ok) {
    g_async_fail_streak = 0;
    g_async_cipstart_fail_streak = 0;
    const char *rx = esp8266.getRxBuffer();
    uint16_t copy_len = rx ? (uint16_t)strlen(rx) : 0;
    if (copy_len >= sizeof(g_async.response))
      copy_len = sizeof(g_async.response) - 1;
    if (rx && copy_len > 0)
      memcpy(g_async.response, rx, copy_len);
    g_async.response[copy_len] = '\0';

    warnLed.off();
    g_async.state = NET_ASYNC_DONE;
    LOG_D("NET", "Async POST done, rx=%u/%u", copy_len, (unsigned)(sizeof(g_async.response) - 1));
  } else {
    bool cipstart_fail = reason && strstr(reason, "CIPSTART");
    net_log_response(reason ? reason : "Async fail");
    net_async_close_best_effort();
    esp8266.resetRxBuffer();
    LED_WarnBlink300ms();

    if (cipstart_fail) {
      if (g_async_cipstart_fail_streak < 255U) g_async_cipstart_fail_streak++;
      if (g_async_cipstart_fail_streak >= 1U) {
        net_async_tcp_stack_cleanup();
        g_async_cipstart_fail_streak = 0;
      }
    } else if (reason && strstr(reason, "done")) {
      g_async_cipstart_fail_streak = 0;
    }

    if (++g_async_fail_streak >= 3U) {
      LOG_W("NET", "Async fail streak=%u; TCP cleanup only, no ESP reset", g_async_fail_streak);
      net_dns_cache_clear();
      g_async_fail_streak = 0;
    }
    g_async.state = NET_ASYNC_FAILED;
  }
  g_async.step = NET_ASYNC_STEP_IDLE;
}

bool Net_AsyncHttpPostStart(const char *host, uint16_t port, const char *path,
                            const char *body, uint16_t body_len,
                            uint32_t timeout_ms) {
  if (!host || !path) return false;
  if (net_async_busy()) return false;
  if (Net_IsHardDisabled()) {
    LOG_W("NET", "Async POST blocked: ESP8266 hard-disabled");
    return false;
  }

  uint32_t now = HAL_GetTick();
  if (g_async_tcp_cooldown_until_ms != 0U &&
      !net_async_time_due(now, g_async_tcp_cooldown_until_ms)) {
    LOG_W("NET", "Async POST delayed: TCP cleanup cooldown");
    return false;
  }

  char resolved[64];
  const char *connect_host = net_connect_host(host, resolved, sizeof(resolved));

  memset(&g_async, 0, sizeof(g_async));
  g_async.port = port;
  g_async.timeout_ms = timeout_ms ? timeout_ms : 15000U;
  strncpy(g_async.host, host, sizeof(g_async.host) - 1);
  strncpy(g_async.connect_host, connect_host, sizeof(g_async.connect_host) - 1);

  int req_len = snprintf(g_async.request, sizeof(g_async.request),
                         "POST %s HTTP/1.0\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %u\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                         path, host, (unsigned)body_len);
  if (req_len <= 0 || req_len >= (int)sizeof(g_async.request)) {
    g_async.state = NET_ASYNC_IDLE;
    return false;
  }

  if (body && body_len > 0) {
    if ((size_t)req_len + (size_t)body_len >= sizeof(g_async.request)) {
      LOG_W("NET", "Async POST too large: header=%d body=%u",
            req_len, (unsigned)body_len);
      g_async.state = NET_ASYNC_IDLE;
      return false;
    }
    memcpy(g_async.request + req_len, body, body_len);
    req_len += body_len;
  }

  g_async.request[req_len] = '\0';
  g_async.req_len = (uint16_t)req_len;

  LOG_I("NET", "Async POST %s:%u%s via %s (%uB)",
        host, (unsigned)port, path, g_async.connect_host,
        (unsigned)g_async.req_len);
  g_async.state = NET_ASYNC_BUSY;
  g_async.request_start_ms = HAL_GetTick();
  net_async_send_line(NET_ASYNC_STEP_CLOSE, "AT+CIPCLOSE");
  return true;
}

void Net_AsyncTick(void) {
  static uint8_t g_net_async_tick_guard = 0;

  if (g_net_async_tick_guard) return;
  if (g_net_raw_busy) return;
  if (g_async.state != NET_ASYNC_BUSY) return;

  g_net_async_tick_guard = 1;

#define NET_ASYNC_TICK_RETURN() do { \
    g_net_async_tick_guard = 0; \
    return; \
  } while (0)

  uint32_t now = HAL_GetTick();

  net_async_pump_rx();
  if (esp8266.hasRxOverflow()) {
    net_async_finish(false, "Async RX overflow");
    NET_ASYNC_TICK_RETURN();
  }

  /* Total-request guard.  Pump RX before checking the guard so a response that
   * arrives exactly at the timeout edge can still be parsed below.  The caller
   * now gives heartbeat/ACK a realistic whole-request budget, so this guard is a
   * deadlock escape hatch rather than a normal network deadline. */
  if (g_async.request_start_ms != 0U &&
      (uint32_t)(now - g_async.request_start_ms) > g_async.timeout_ms) {
    const char *rx = esp8266.getRxBuffer();
    bool has_resp = rx && (strstr(rx, "HTTP/") || strstr(rx, "{"));
    bool complete = has_resp && net_http_body_complete(rx);
    if (complete) {
      net_async_finish(true, "HTTP complete at timeout edge");
    } else {
      net_async_finish(false, has_resp ? "Async response incomplete" : "Async total timeout");
    }
    NET_ASYNC_TICK_RETURN();
  }

  switch (g_async.step) {
  case NET_ASYNC_STEP_CLOSE:
    if ((net_async_has("OK", "CLOSED", "ERROR") &&
         now - g_async.last_rx_ms > 150U) ||
        now - g_async.step_start_ms > 2200U) {
      net_async_send_line(NET_ASYNC_STEP_MUX, "AT+CIPMUX=0");
    }
    break;

  case NET_ASYNC_STEP_MUX:
    if (((net_async_has("OK") || net_async_has("ERROR")) &&
         now - g_async.last_rx_ms > 150U) ||
        now - g_async.step_start_ms > 3000U) {
      char cmd[128];
      int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                       g_async.connect_host[0] ? g_async.connect_host : g_async.host,
                       (unsigned)g_async.port);
      if (n <= 0 || n >= (int)sizeof(cmd)) {
        net_async_finish(false, "CIPSTART build fail");
      } else {
        net_async_send_line(NET_ASYNC_STEP_START, cmd);
      }
    }
    break;

  case NET_ASYNC_STEP_START:
    if (net_async_failed_token()) {
      net_async_finish(false, "CIPSTART fail");
    } else if (net_async_has("CONNECT", "ALREADY CONNECTED")) {
      char cmd[32];
      int n = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u",
                       (unsigned)g_async.req_len);
      if (n <= 0 || n >= (int)sizeof(cmd)) {
        net_async_finish(false, "CIPSEND build fail");
      } else {
        net_async_send_line(NET_ASYNC_STEP_SEND_LEN, cmd);
      }
    } else if (now - g_async.step_start_ms > 15000U) {
      net_async_finish(false, "CIPSTART timeout");
    }
    break;

  case NET_ASYNC_STEP_SEND_LEN:
    if (net_async_has(">")) {
      net_async_rx_reset();
      HAL_UART_Transmit(&huart2, (uint8_t *)g_async.request,
                        g_async.req_len, 3000);
      SysWatchdog_Tick();
      g_async.step = NET_ASYNC_STEP_WAIT_RESP;
      g_async.step_start_ms = now;
      g_async.last_rx_ms = now;
      g_async.last_rx_len = 0;
    } else if (net_async_failed_token()) {
      net_async_finish(false, "CIPSEND fail");
    } else if (now - g_async.step_start_ms > 4000U) {
      net_async_finish(false, "CIPSEND timeout");
    }
    break;

  case NET_ASYNC_STEP_WAIT_RESP:
    {
      const char *rx = esp8266.getRxBuffer();
      bool has_resp = rx && (strstr(rx, "HTTP/") || strstr(rx, "{"));
      bool closed = rx && strstr(rx, "CLOSED");
      bool complete = has_resp && net_http_body_complete(rx);
      bool settled_complete = complete && (now - g_async.last_rx_ms > 250U);

      /* Prefer Content-Length / JSON completion over idle time.  Finishing on
       * "rx idle for 900 ms" alone caused truncated heartbeat bodies, so the
       * command array sometimes looked like "cmds":[ without a closing ]. */
      if (has_resp && (closed || settled_complete)) {
        net_async_finish(true, complete ? "HTTP complete" : "HTTP closed");
      } else if (now - g_async.step_start_ms > g_async.timeout_ms) {
        net_async_finish(false, has_resp ? "HTTP incomplete" : "HTTP timeout");
      }
    }
    break;

  default:
    net_async_finish(false, "Async bad state");
    break;
  }

#undef NET_ASYNC_TICK_RETURN
  g_net_async_tick_guard = 0;
}

NetAsyncState_t Net_AsyncState(void) { return g_async.state; }

const char *Net_AsyncResponse(void) { return g_async.response; }

void Net_AsyncReset(void) {
  if (g_async.state == NET_ASYNC_BUSY) return;
  esp8266.resetRxBuffer();
  memset(&g_async, 0, sizeof(g_async));
  g_async.state = NET_ASYNC_IDLE;
}

bool Net_IsHardDisabled(void) {
  return ESP8266_IsHardDisabled();
}

void Net_LedSuccess(void) { net_led_success(); }
void Net_LedFailure(void) { net_led_failure(); }

/* Best-effort client mode preparation.  AT+CIPSERVER=0 and AT+CIPCLOSE
 * legitimately return ERROR when no server / connection exists — this is
 * normal and must NOT be treated as a failure.  Only CIPMUX=0 is essential. */
void Net_PrepareClient(void) {
  (void)net_raw_at_best_effort("AT+CIPSERVER=0", "OK", 1200, 50,
                               "ERROR");
  (void)net_raw_at_best_effort("AT+CIPCLOSE", "OK", 1800, 80,
                               "CLOSED", "ERROR");
  (void)net_raw_at_best_effort("AT+CIPMODE=0", "OK", 1500, 60,
                               "ERROR");
  if (!net_raw_at_best_effort("AT+CIPMUX=0", "OK", 2500, 80, "ERROR")) {
    net_log_response("CIPMUX=0");
  }
}

void Net_LightCleanup(void) {
  if (Net_IsHardDisabled() || net_async_busy() || g_net_raw_busy) return;

  (void)net_raw_at_best_effort("AT+CIPCLOSE", "OK", 900, 30,
                               "CLOSED", "ERROR");
  (void)net_raw_at_best_effort("AT+CIPMODE=0", "OK", 900, 30,
                               "ERROR");
  (void)net_raw_at_best_effort("AT+CIPMUX=0", "OK", 1200, 40,
                               "ERROR");
  esp8266.resetRxBuffer();
}

void Net_ResetDnsCache(void) {
  net_dns_cache_clear();
}

bool Net_HasStationIP(void) {
  char status_resp[128];
  char cifsr_resp[256];
  bool status_ok = net_raw_at_capture("AT+CIPSTATUS", "OK", 3000, 60,
                                      status_resp, sizeof(status_resp));
  net_log_response_text(status_ok ? "CIPSTATUS" : "CIPSTATUS fail",
                        status_resp);

  bool cifr_ok = net_raw_at_capture("AT+CIFSR", "OK", 3000, 60,
                                    cifsr_resp, sizeof(cifsr_resp));
  net_log_response_text(cifr_ok ? "CIFSR" : "CIFSR fail", cifsr_resp);

  if (!cifr_ok) return false;
  return strstr(cifsr_resp, "STAIP") != nullptr &&
         strstr(cifsr_resp, "\"0.0.0.0\"") == nullptr;
}

/* ── TCP ──────────────────────────────────────────────────────── */

static bool net_tcp_start(const char *host, uint16_t port) {
  char connect_host_buf[64];
  const char *connect_host = net_connect_host(host, connect_host_buf,
                                             sizeof(connect_host_buf));
  char cmd[128];
  int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                   connect_host, (unsigned)port);
  if (n <= 0 || n >= (int)sizeof(cmd)) return false;

  bool ok = net_raw_at(cmd, "CONNECT", 15000, 120, "ALREADY CONNECTED");
  net_log_response(ok ? "CIPSTART" : "CIPSTART fail");
  return ok;
}

/* ── HTTP GET ─────────────────────────────────────────────────── */

bool Net_HttpGet(const char *host, uint16_t port, const char *path,
                 char *resp_buf, uint16_t resp_sz, uint32_t timeout_ms) {
  if (!host || !path || !resp_buf || resp_sz == 0) return false;
  if (net_async_busy()) {
    LOG_W("NET", "HTTP GET blocked: async request busy");
    return false;
  }

  /* Hard-disabled check */
  if (Net_IsHardDisabled()) {
    LOG_W("NET", "HTTP GET blocked: ESP8266 hard-disabled");
    return false;
  }

  LOG_I("NET", "HTTP GET %s:%u%s", host, (unsigned)port, path);

  /* Prepare client mode (best-effort, never fails) */
  Net_PrepareClient();

  /* Check we have an IP */
  if (!Net_HasStationIP()) {
    LOG_E("NET", "No station IP");
    net_led_failure();
    return false;
  }

  /* Start TCP connection */
  if (!net_tcp_start(host, port)) {
    LOG_E("NET", "TCP connect failed");
    net_led_failure();
    return false;
  }

  /* ── Send HTTP GET request ──────────────────────────────── */
  char request[384];
  int req_len = snprintf(request, sizeof(request),
                         "GET %s HTTP/1.0\r\n"
                         "Host: %s\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                         path, host);
  if (req_len <= 0 || req_len >= (int)sizeof(request)) {
    net_led_failure();
    return false;
  }

  char cmd[32];
  int cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", req_len);
  if (cmd_len <= 0 || cmd_len >= (int)sizeof(cmd)) {
    net_led_failure();
    return false;
  }

  net_raw_begin();

  char tx[40];
  int tx_len = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)tx_len, 1000);

  if (!net_raw_collect(">", nullptr, nullptr, 4000, 20, false, true)) {
    net_log_response("CIPSEND fail");
    net_raw_end();
    net_led_failure();
    return false;
  }

  esp8266.resetRxBuffer();
  HAL_UART_Transmit(&huart2, (uint8_t *)request, (uint16_t)req_len, 3000);
  SysWatchdog_Tick();
  bool closed = net_raw_collect("CLOSED", nullptr, nullptr, timeout_ms, 150, true, false);
  const char *rx = esp8266.getRxBuffer();
  bool has_response = (rx && (strstr(rx, "HTTP/") || strstr(rx, "{")));

  if (!closed && !has_response) {
    net_log_response("HTTP fail");
    net_raw_end();
    net_led_failure();
    return false;
  }

  /* Copy response to caller's buffer */
  if (rx && resp_buf && resp_sz > 0) {
    uint16_t copy_len = (uint16_t)strlen(rx);
    if (copy_len >= resp_sz) copy_len = resp_sz - 1;
    memcpy(resp_buf, rx, copy_len);
    resp_buf[copy_len] = '\0';
  }

  net_log_response(closed ? "HTTP" : "HTTP partial");
  net_raw_end();

  /* Close TCP gracefully */
  net_raw_at("AT+CIPCLOSE", "OK", 1500, 40, "CLOSED", "ERROR");

  net_led_success();
  return has_response;
}

/* ── HTTP POST ────────────────────────────────────────────────── */

bool Net_HttpPost(const char *host, uint16_t port, const char *path,
                  const char *body, uint16_t body_len,
                  char *resp_buf, uint16_t resp_sz, uint32_t timeout_ms) {
  if (!host || !path || !resp_buf || resp_sz == 0) return false;
  if (net_async_busy()) {
    LOG_W("NET", "HTTP POST blocked: async request busy");
    return false;
  }

  /* Hard-disabled check */
  if (Net_IsHardDisabled()) {
    LOG_W("NET", "HTTP POST blocked: ESP8266 hard-disabled");
    return false;
  }

  /* Build HTTP request with Content-Length header */
  char request[1024];
  int req_len = snprintf(request, sizeof(request),
                         "POST %s HTTP/1.0\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %u\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                         path, host, (unsigned)body_len);
  if (req_len <= 0 || req_len >= (int)sizeof(request)) {
    net_led_failure();
    return false;
  }

  /* Append body (if it fits) */
  int remaining = (int)sizeof(request) - req_len - 1;
  if (body && body_len > 0 && remaining >= (int)body_len) {
    memcpy(request + req_len, body, body_len);
    req_len += body_len;
    request[req_len] = '\0';
  }

  LOG_I("NET", "HTTP POST %s:%u%s", host, (unsigned)port, path);

  /* Prepare client mode (best-effort, never fails) */
  Net_PrepareClient();

  if (!Net_HasStationIP()) {
    LOG_E("NET", "No station IP");
    net_led_failure();
    return false;
  }

  if (!net_tcp_start(host, port)) {
    LOG_E("NET", "TCP connect failed");
    net_led_failure();
    return false;
  }

  /* Send via CIPSEND */
  char cmd[32];
  int cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", req_len);
  if (cmd_len <= 0 || cmd_len >= (int)sizeof(cmd)) {
    net_led_failure();
    return false;
  }

  net_raw_begin();

  char tx[40];
  int tx_len = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)tx_len, 1000);

  if (!net_raw_collect(">", nullptr, nullptr, 4000, 20, false, true)) {
    net_log_response("CIPSEND fail");
    net_raw_end();
    net_led_failure();
    return false;
  }

  esp8266.resetRxBuffer();
  HAL_UART_Transmit(&huart2, (uint8_t *)request, (uint16_t)req_len, 3000);
  SysWatchdog_Tick();
  bool closed = net_raw_collect("CLOSED", nullptr, nullptr, timeout_ms, 150, true, false);
  const char *rx = esp8266.getRxBuffer();
  bool has_response = (rx && (strstr(rx, "HTTP/") || strstr(rx, "{")));

  if (!closed && !has_response) {
    net_log_response("HTTP fail");
    net_raw_end();
    net_led_failure();
    return false;
  }

  /* Copy response */
  if (rx && resp_buf && resp_sz > 0) {
    uint16_t copy_len = (uint16_t)strlen(rx);
    if (copy_len >= resp_sz) copy_len = resp_sz - 1;
    memcpy(resp_buf, rx, copy_len);
    resp_buf[copy_len] = '\0';
  }

  net_log_response(closed ? "HTTP" : "HTTP partial");
  net_raw_end();

  net_raw_at("AT+CIPCLOSE", "OK", 1500, 40, "CLOSED", "ERROR");

  net_led_success();
  return has_response;
}
