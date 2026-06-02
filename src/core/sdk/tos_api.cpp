/**
 * @file    tos_api.cpp
 * @brief   Cloud API — HTTP GET + JSON parse
 */

#include "tos_api.h"
#include "../include/config.h"
#include "hardware/include/esp8266.hpp"
#include "syslog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern ESP8266 esp8266;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t esp8266_global_buffer[512];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
}

static void tapi_reset_shared_rx(void) {
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  memset(esp8266_global_buffer, 0, 512);
}

static void tapi_uart_discard(uint32_t idle_ms) {
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

static void tapi_raw_begin(void) {
  HAL_NVIC_DisableIRQ(USART2_IRQn);
  tapi_reset_shared_rx();
  esp8266.resetRxBuffer();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  tapi_uart_discard(20);
}

static void tapi_raw_end(void) {
  tapi_uart_discard(2);
  tapi_reset_shared_rx();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static bool has_token(const char *buf, const char *a, const char *b = 0,
                      const char *c = 0) {
  return (a && strstr(buf, a)) || (b && strstr(buf, b)) ||
         (c && strstr(buf, c));
}

static bool tapi_raw_collect(const char *ok1, const char *ok2,
                             const char *ok3, uint32_t timeout_ms,
                             uint32_t settle_ms, bool closed_ok,
                             bool fail_on_error) {
  uint32_t start = HAL_GetTick();
  uint32_t last_rx = start;
  bool matched = false;
  bool failed = false;

  while (HAL_GetTick() - start < timeout_ms) {
    bool got = false;

    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      uint8_t c = (uint8_t)(huart2.Instance->DR & 0xFF);
      esp8266.processRxData(&c, 1);
      got = true;
      last_rx = HAL_GetTick();
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
      __HAL_UART_CLEAR_OREFLAG(&huart2);
      last_rx = HAL_GetTick();
    }

    if (got) {
      const char *buf = esp8266.getRxBuffer();
      if (has_token(buf, ok1, ok2, ok3)) matched = true;
      if (closed_ok && strstr(buf, "CLOSED")) matched = true;
      if (fail_on_error &&
          (strstr(buf, "ERROR") || strstr(buf, "FAIL") ||
           strstr(buf, "DNS Fail"))) {
        failed = true;
      }
    }

    if ((matched || failed) && (HAL_GetTick() - last_rx >= settle_ms)) break;
  }

  return matched && !failed;
}

static bool tapi_raw_at(const char *cmd, const char *ok1,
                        uint32_t timeout_ms, uint32_t settle_ms,
                        const char *ok2 = 0, const char *ok3 = 0) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);

  if (n <= 0 || n >= (int)sizeof(tx)) return false;

  tapi_raw_begin();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);
  bool ok = tapi_raw_collect(ok1, ok2, ok3, timeout_ms, settle_ms, false, true);
  tapi_raw_end();
  return ok;
}

static void tapi_log_response(const char *label) {
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
  LOG_D("TAPI", "%s: %s", label, summary);
}

static bool tapi_station_has_ip(void) {
  bool status_ok = tapi_raw_at("AT+CIPSTATUS", "OK", 2500, 40);
  tapi_log_response(status_ok ? "CIPSTATUS" : "CIPSTATUS fail");

  bool cifr_ok = tapi_raw_at("AT+CIFSR", "OK", 2500, 40);
  const char *buf = esp8266.getRxBuffer();
  tapi_log_response(cifr_ok ? "CIFSR" : "CIFSR fail");

  if (!cifr_ok) return status_ok;
  return strstr(buf, "STAIP") && !strstr(buf, "\"0.0.0.0\"");
}

static void tapi_prepare_client_mode(void) {
  tapi_raw_at("AT+CIPSERVER=0", "OK", 1200, 30);
  tapi_raw_at("AT+CIPCLOSE", "OK", 1200, 30, "CLOSED", "ERROR");
  if (!tapi_raw_at("AT+CIPMUX=0", "OK", 2500, 50)) {
    tapi_log_response("CIPMUX=0");
  }
}

static bool tapi_tcp_start(const char *host, uint16_t port) {
  char cmd[128];
  int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
                   host, (unsigned)port);

  if (n <= 0 || n >= (int)sizeof(cmd)) return false;
  bool ok = tapi_raw_at(cmd, "CONNECT", 15000, 120, "ALREADY CONNECTED", "OK");
  tapi_log_response(ok ? "CIPSTART" : "CIPSTART fail");
  return ok;
}

static bool tapi_http_get(const char *host, const char *path) {
  char request[384];
  char cmd[32];
  int len = snprintf(request, sizeof(request),
                     "GET %s HTTP/1.0\r\n"
                     "Host: %s\r\n"
                     "Accept: application/json\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     path, host);
  if (len <= 0 || len >= (int)sizeof(request)) return false;

  int cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
  if (cmd_len <= 0 || cmd_len >= (int)sizeof(cmd)) return false;

  tapi_raw_begin();

  char tx[40];
  int tx_len = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)tx_len, 1000);

  if (!tapi_raw_collect(">", 0, 0, 4000, 20, false, true)) {
    tapi_log_response("CIPSEND fail");
    tapi_raw_end();
    return false;
  }

  esp8266.resetRxBuffer();
  HAL_UART_Transmit(&huart2, (uint8_t *)request, (uint16_t)len, 3000);
  bool closed = tapi_raw_collect("CLOSED", 0, 0, 12000, 150, true, false);
  const char *buf = esp8266.getRxBuffer();
  bool has_response = strstr(buf, "HTTP/") || strstr(buf, "{");

  if (!closed && !has_response) {
    tapi_log_response("HTTP fail");
    tapi_raw_end();
    return false;
  }

  tapi_log_response(closed ? "HTTP" : "HTTP partial");
  tapi_raw_end();
  return has_response;
}

static const char *json_find_value(const char *buf, const char *key) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\"", key);
  const char *p = strstr(buf, search);
  if (!p) return NULL;
  p += strlen(search);
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p != ':') return NULL;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

/* Minimal JSON integer extractor: find "key":<number> */
static int json_get_int(const char *buf, const char *key) {
  const char *p = json_find_value(buf, key);
  if (!p) return 0;
  return atoi(p);
}

/* Minimal JSON string extractor: find "key": "value" */
static void json_get_str(const char *buf, const char *key, char *out, int outsz) {
  const char *p = json_find_value(buf, key);
  if (!p) { out[0] = '\0'; return; }
  if (*p != '"') { out[0] = '\0'; return; }
  ++p;
  int i = 0;
  while (*p && *p != '"' && i < outsz - 1) {
    if (*p == '\\' && p[1]) {
      ++p;
      if (*p == 'n') out[i++] = '\n';
      else if (*p == 'r') out[i++] = '\r';
      else if (*p == 't') out[i++] = '\t';
      else out[i++] = *p;
      ++p;
    } else {
      out[i++] = *p++;
    }
  }
  out[i] = '\0';
}

/* Minimal JSON bool extractor */
static bool json_get_bool(const char *buf, const char *key) {
  const char *p = json_find_value(buf, key);
  if (!p) return false;
  return (*p == 't' || *p == 'T');
}

bool TosApi_CheckUpgrade(TosUpgradeInfo *info) {
  if (!info) return false;
  memset(info, 0, sizeof(*info));

  char host[64];
  snprintf(host, sizeof(host), "%s", TOS_API_HOST);

  char path[128];
  snprintf(path, sizeof(path),
           "%s?model=%s&version=%s&version_code=%lu",
           TOS_API_PATH, CFG_MODEL, CFG_TOS_VERSION,
           (unsigned long)CFG_VERSION_CODE);

  LOG_I("TAPI", "Checking update: %s:%d%s", host, TOS_API_PORT, path);

  tapi_prepare_client_mode();
  if (!tapi_station_has_ip()) {
    LOG_E("TAPI", "ESP8266 has no STA IP");
    return false;
  }

  if (!tapi_tcp_start(host, TOS_API_PORT)) {
    LOG_E("TAPI", "TCP connect failed");
    return false;
  }

  if (!tapi_http_get(host, path)) {
    LOG_E("TAPI", "HTTP request failed");
    tapi_raw_at("AT+CIPCLOSE", "OK", 1500, 40, "CLOSED", "ERROR");
    return false;
  }

  const char *buf = esp8266.getRxBuffer();
  if (!buf || !*buf) {
    LOG_E("TAPI", "Empty response");
    return false;
  }

  LOG_D("TAPI", "Raw: %.200s", buf);

  /* Parse JSON body (after HTTP headers inside +IPD payload) */
  const char *http = strstr(buf, "HTTP/");
  const char *body = http ? strstr(http, "\r\n\r\n") : NULL;
  if (body) {
    body += 4;
  } else {
    body = strchr(buf, '{');
    if (!body) body = buf;
  }

  /* Check response code */
  int code = json_get_int(body, "code");
  if (code != 0) {
    LOG_E("TAPI", "API error code: %d", code);
    tapi_raw_at("AT+CIPCLOSE", "OK", 1500, 40, "CLOSED", "ERROR");
    return false;
  }

  /* Parse "data" -> "has_update" */
  info->has_update = json_get_bool(body, "has_update");

  /* Parse "latest" fields */
  const char *latest = strstr(body, "\"latest\"");
  if (latest) {
    json_get_str(latest, "version",      info->latest_version, sizeof(info->latest_version));
    info->latest_version_code = json_get_int(latest, "version_code");
    json_get_str(latest, "build",        info->latest_build, sizeof(info->latest_build));
    json_get_str(latest, "patch",        info->latest_patch, sizeof(info->latest_patch));
    json_get_str(latest, "url",          info->latest_url, sizeof(info->latest_url));
    info->latest_size = json_get_int(latest, "size");
    json_get_str(latest, "sha256",       info->latest_sha256, sizeof(info->latest_sha256));
  }

  LOG_D("TAPI", "Latest: ver='%s' build='%s' patch='%s' size=%d",
        info->latest_version, info->latest_build, info->latest_patch,
        info->latest_size);
  LOG_I("TAPI", "Update check done. has_update=%d", info->has_update);
  tapi_raw_at("AT+CIPCLOSE", "OK", 1500, 40, "CLOSED", "ERROR");
  return true;
}
