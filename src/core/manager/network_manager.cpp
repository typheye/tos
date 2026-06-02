/**
 * @file    network_manager.cpp
 * @brief   Unified network request manager — HTTP via ESP8266
 *
 * All network communication is routed through this module.
 * LED rules are applied at the low-level I/O boundaries:
 *   success → boardLed blink (100 ms)
 *   failure → warnLed ON
 */

#include "include/network_manager.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/led.hpp"
#include "include/syshandle.h"
#include "syslog.h"
#include <cstdio>
#include <cstring>

/* ── Externals ────────────────────────────────────────────────── */

extern ESP8266 esp8266;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t  esp8266_global_buffer[];
extern uint16_t esp8266_global_index;
extern uint8_t  esp8266_data_ready;
}

static const size_t NET_RX_SIZE = 2048;

/* ── LED helpers ──────────────────────────────────────────────── */

static void net_led_success(void) {
  /* Turn off warnLed (clear previous error), blink boardLed 100 ms */
  warnLed.off();
  boardLed.on();
  HAL_Delay(100);
  boardLed.off();
}

static void net_led_failure(void) {
  warnLed.on();
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
  }
}

static void net_raw_begin(void) {
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

static void net_log_response(const char *label) {
  char summary[128];
  const char *resp = esp8266.getRxBuffer();
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
  uint16_t port;
  char request[1024];
  uint16_t req_len;
  char response[1536];
  uint32_t timeout_ms;
  uint32_t step_start_ms;
  uint32_t last_rx_ms;
  uint16_t last_rx_len;
};

static NetAsyncCtx g_async = {NET_ASYNC_IDLE};
static uint8_t g_async_fail_streak = 0;

static bool net_async_busy(void) {
  return g_async.state == NET_ASYNC_BUSY;
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

static void net_async_finish(bool ok, const char *reason) {
  if (ok) {
    g_async_fail_streak = 0;
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
    net_log_response(reason ? reason : "Async fail");
    warnLed.on();
    if (++g_async_fail_streak >= 3U) {
      LOG_W("NET", "Async fail streak=%u, asking ESP recovery", g_async_fail_streak);
      bool recovered = ESP8266_TryRecover(false);
      g_async_fail_streak = 0;
      if (!recovered) {
        SysHandle_Fatal(SYS_ERR_NET_TRANSPORT_STUCK);
      }
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

  memset(&g_async, 0, sizeof(g_async));
  g_async.state = NET_ASYNC_BUSY;
  g_async.port = port;
  g_async.timeout_ms = timeout_ms ? timeout_ms : 15000U;
  strncpy(g_async.host, host, sizeof(g_async.host) - 1);

  int req_len = snprintf(g_async.request, sizeof(g_async.request),
                         "POST %s HTTP/1.0\r\n"
                         "Host: %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Accept: application/json\r\n"
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

  LOG_I("NET", "Async POST %s:%u%s (%uB)",
        host, (unsigned)port, path, (unsigned)g_async.req_len);
  net_async_send_line(NET_ASYNC_STEP_CLOSE, "AT+CIPCLOSE");
  return true;
}

void Net_AsyncTick(void) {
  if (g_async.state != NET_ASYNC_BUSY) return;

  uint32_t now = HAL_GetTick();
  net_async_pump_rx();

  switch (g_async.step) {
  case NET_ASYNC_STEP_CLOSE:
    if (net_async_has("OK", "CLOSED", "ERROR") ||
        now - g_async.step_start_ms > 1200U) {
      net_async_send_line(NET_ASYNC_STEP_MUX, "AT+CIPMUX=0");
    }
    break;

  case NET_ASYNC_STEP_MUX:
    if (net_async_has("OK") || net_async_has("ERROR") ||
        now - g_async.step_start_ms > 2500U) {
      char cmd[128];
      int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                       g_async.host, (unsigned)g_async.port);
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
    } else if (net_async_has("CONNECT", "ALREADY CONNECTED", "OK")) {
      char cmd[32];
      int n = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u",
                       (unsigned)g_async.req_len);
      if (n <= 0 || n >= (int)sizeof(cmd)) {
        net_async_finish(false, "CIPSEND build fail");
      } else {
        net_async_send_line(NET_ASYNC_STEP_SEND_LEN, cmd);
      }
    } else if (now - g_async.step_start_ms > 12000U) {
      net_async_finish(false, "CIPSTART timeout");
    }
    break;

  case NET_ASYNC_STEP_SEND_LEN:
    if (net_async_has(">")) {
      net_async_rx_reset();
      HAL_UART_Transmit(&huart2, (uint8_t *)g_async.request,
                        g_async.req_len, 3000);
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
}

NetAsyncState_t Net_AsyncState(void) { return g_async.state; }

const char *Net_AsyncResponse(void) { return g_async.response; }

void Net_AsyncReset(void) {
  if (g_async.state == NET_ASYNC_BUSY) return;
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
  net_raw_at("AT+CIPSERVER=0", "OK", 1200, 30);
  net_raw_at("AT+CIPCLOSE", "OK", 1200, 30, "CLOSED", "ERROR");
  if (!net_raw_at("AT+CIPMUX=0", "OK", 2500, 50)) {
    net_log_response("CIPMUX=0");
  }
}

bool Net_HasStationIP(void) {
  bool status_ok = net_raw_at("AT+CIPSTATUS", "OK", 2500, 40);
  net_log_response(status_ok ? "CIPSTATUS" : "CIPSTATUS fail");

  bool cifr_ok = net_raw_at("AT+CIFSR", "OK", 2500, 40);
  net_log_response(cifr_ok ? "CIFSR" : "CIFSR fail");

  if (!cifr_ok) return status_ok;
  const char *rx = esp8266.getRxBuffer();
  return (rx && strstr(rx, "STAIP") && !strstr(rx, "\"0.0.0.0\""));
}

/* ── TCP ──────────────────────────────────────────────────────── */

static bool net_tcp_start(const char *host, uint16_t port) {
  char cmd[128];
  int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                   host, (unsigned)port);
  if (n <= 0 || n >= (int)sizeof(cmd)) return false;

  bool ok = net_raw_at(cmd, "CONNECT", 15000, 120, "ALREADY CONNECTED", "OK");
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
                         "Accept: */*\r\n"
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
                         "Content-Type: application/json\r\n"
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
