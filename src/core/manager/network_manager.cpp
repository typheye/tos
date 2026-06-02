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
#include "syslog.h"
#include <cstdio>
#include <cstring>

/* ── Externals ────────────────────────────────────────────────── */

extern ESP8266 esp8266;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t  esp8266_global_buffer[512];
extern uint16_t esp8266_global_index;
extern uint8_t  esp8266_data_ready;
}

static const size_t NET_RX_SIZE = 512;

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

/* ── Public API ───────────────────────────────────────────────── */

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

  /* Hard-disabled check */
  if (Net_IsHardDisabled()) {
    LOG_W("NET", "HTTP POST blocked: ESP8266 hard-disabled");
    return false;
  }

  /* Build HTTP request with Content-Length header */
  char request[512];
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
