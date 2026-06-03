/**
 ******************************************************************************
 * @file    esp8266.cpp
 * @author  Typheye
 * @brief   ESP8266 AT driver implementation.
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

#include "hardware/include/esp8266.hpp"
#include "hardware/include/led.hpp"
#include "hardware/include/usart.hpp"
#include "include/syshandle.h"
#include "syslog.h"
#include "core/sys/include/syswatchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram"), aligned(4)))
#endif

extern USART boardSerial;

static void esp_led_success(void) {
  warnLed.off();
  boardLed.on();
  HAL_Delay(6);
  boardLed.off();
  SysWatchdog_Tick();
}

static void esp_led_failure(void) {
  boardLed.off();
  LED_WarnBlink300ms();
}

extern "C" {
extern uint8_t esp8266_global_buffer[];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
extern volatile uint32_t uart2_rx_count;
}

// Global instance
CCMRAM ESP8266 esp8266(&huart2);

// ==================== C++ class implementation ====================

ESP8266::ESP8266(UART_HandleTypeDef *huart) {
  _huart = huart;
  _state = 0;
  _hard_disabled = false;
  _last_recover_ms = 0;
  _recover_attempts = 0;
  _recover_failures = 0;
  _rx_index = 0;
  _rx_overflow = false;
  memset(_rx_buffer, 0, sizeof(_rx_buffer));
}

void ESP8266::clearRxBuffer(void) {
  _rx_index = 0;
  _rx_overflow = false;
  memset(_rx_buffer, 0, sizeof(_rx_buffer));
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  memset(esp8266_global_buffer, 0, 2048);
}

void ESP8266::processPendingData(void) {
  if (esp8266_data_ready) {
    esp8266_data_ready = 0;
    /* LOG_D("ESP", "PROCESS: Got data, len=%d", esp8266_global_index); */
    processRxData(esp8266_global_buffer, esp8266_global_index);
    esp8266_global_index = 0;
  }
}

void ESP8266::resetRxBuffer(void) { clearRxBuffer(); }

bool ESP8266::waitForResponse(const char *expected, uint32_t timeout_ms) {
  if (_hard_disabled) return false;

  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      if (_rx_overflow) {
        LOG_E("ESP", "RX overflow while waiting for %s",
              expected ? expected : "(any)");
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      if (expected && strstr((char *)_rx_buffer, expected) != NULL) {
        /* Communication success: brief blink to acknowledge */
        esp_led_success();
        return true;
      }

      if (strstr((char *)_rx_buffer, "ERROR") != NULL ||
          strstr((char *)_rx_buffer, "FAIL") != NULL) {
        /* Communication failure: blink warn LED without blocking */
        clearRxBuffer();
        esp_led_failure();
        return false;
      }
    }
    HAL_Delay(10);
    SysWatchdog_Tick();
  }
  /* Timeout: communication failure */
  clearRxBuffer();
  esp_led_failure();
  return false;
}

void ESP8266::parseResponse(const char *response) {
  if (strstr(response, "WIFI GOT IP") != NULL) {
    _state = 3;
    LOG_I("ESP", "Got IP");
  } else if (strstr(response, "CONNECT") != NULL) {
    _state = 2;
    LOG_I("ESP", "Connected");
  } else if (strstr(response, "CLOSED") != NULL ||
             strstr(response, "DISCONNECT") != NULL) {
    _state = 0;
    LOG_I("ESP", "Disconnected");
  }
}

void ESP8266::processRxData(uint8_t *data, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    if (_rx_index >= sizeof(_rx_buffer) - 1) {
      _rx_overflow = true;
      break;
    }
    _rx_buffer[_rx_index++] = data[i];
  }
  _rx_buffer[_rx_index] = '\0';
}

bool ESP8266::tryRecover(bool force) {
  (void)force;
  if (_recover_failures < 0xFFFFU) _recover_failures++;
  LOG_W("ESP", "Runtime ESP recovery disabled by policy");
  /* Do not send AT+RST, do not re-init, and do not change hard-disabled here.
   * The cloud policy layer either keeps this boot offline or reboots the whole
   * device through syshandle after a proven online session becomes stuck. */
  return false;
}

void ESP8266::init(void) {
  LOG_I("ESP", "Initializing...");
  LOG_D("ESP", "UART2 RX count before: %lu", (unsigned long)uart2_rx_count);

  _hard_disabled = false;
  _state = 0;

  /* Flush any stale boot data from ESP8266 */
  clearRxBuffer();
  HAL_Delay(500);
  SysWatchdog_FeedNow();
  processPendingData();
  clearRxBuffer();
  HAL_Delay(200);
  SysWatchdog_FeedNow();

  LOG_D("ESP", "Sending AT test...");
  if (sendCommand("AT", "OK", 3000)) {
    LOG_I("ESP", "AT OK");
    sendCommand("ATE0", "OK", 1000);
    sendCommand("AT+CIPMODE=0", "OK", 1000);
    sendCommand("AT+CIPMUX=0", "OK", 1000);
  } else {
    LOG_D("ESP", "UART2 RX count: %lu", (unsigned long)uart2_rx_count);
    _hard_disabled = true;
    _state = 4;
    LOG_E("ESP", "ESP8266 unavailable for this boot");
  }
}

bool ESP8266::sendCommand(const char *cmd, const char *expected_response,
                          uint32_t timeout_ms) {
  if (_hard_disabled) {
    LOG_W("ESP", "sendCommand blocked: hard-disabled");
    return false;
  }

  char buffer[128];

  clearRxBuffer();

  sprintf(buffer, "%s\r\n", cmd);

  /* LOG_D("ESP", "SEND: %s", cmd); */

  HAL_UART_Transmit(_huart, (uint8_t *)buffer, strlen(buffer), 1000);

  uint32_t start = HAL_GetTick();
  uint32_t last_dbg = 0;
  bool near_full_logged = false;

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      if (_rx_overflow) {
        LOG_E("ESP", "RX overflow after %lums, rx=%u/%u while waiting for '%s'",
              (unsigned long)(HAL_GetTick() - start), _rx_index,
              (unsigned)sizeof(_rx_buffer),
              expected_response ? expected_response : "(any)");
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      if (expected_response) {
        if (strstr((char *)_rx_buffer, expected_response) != NULL) {
          LOG_D("ESP", "OK after %lums, rx=%u bytes",
                (unsigned long)(HAL_GetTick() - start), _rx_index);
          /* Communication success: brief blink to acknowledge */
          esp_led_success();
          return true;
        }
        /* Buffer nearly full — search for partial match */
        if (!near_full_logged && _rx_index >= sizeof(_rx_buffer) - 64) {
          near_full_logged = true;
          LOG_W("ESP", "Rx buffer nearly full (%u/%u), searching for '%s'",
                _rx_index, (unsigned)sizeof(_rx_buffer), expected_response);
        }
      } else {
        /* No expected response specified — any data counts as success */
        esp_led_success();
        return true;
      }

      if (strstr((char *)_rx_buffer, "ERROR") != NULL ||
          strstr((char *)_rx_buffer, "FAIL") != NULL) {
        LOG_E("ESP", "Got ERROR/FAIL after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rx_index);
        /* Communication failure: blink warn LED without blocking */
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      /* Periodic debug during long waits */
      if (timeout_ms > 3000 && HAL_GetTick() - last_dbg > 2000) {
        last_dbg = HAL_GetTick();
        LOG_D("ESP", "Waiting... %lums, rx=%u/%u bytes",
              (unsigned long)(HAL_GetTick() - start),
              _rx_index, (unsigned)sizeof(_rx_buffer));
      }
    }
    HAL_Delay(10);
    SysWatchdog_Tick();
  }

  LOG_E("ESP", "TIMEOUT after %lums, rx=%u bytes",
        (unsigned long)timeout_ms, _rx_index);
  /* Communication failure: blink warn LED without blocking */
  clearRxBuffer();
  esp_led_failure();
  return false;
}

bool ESP8266::connectWiFi(const char *ssid, const char *password) {
  char cmd[256];
  sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

  _state = 1;
  /* 8s is too short for some APs after a cold boot or after the ESP finished a
   * previous TCP/AT sequence.  A single 15s join is safer than two short joins
   * that leave a late OK in the UART stream and confuse the next command. */
  bool result = sendCommand(cmd, "OK", 15000);

  if (result) {
    HAL_Delay(2000);
    SysWatchdog_FeedNow();
    _state = 3;
  } else {
    _state = 0;
  }
  return result;
}

bool ESP8266::sendData(const uint8_t *data, uint16_t len) {
  char cmd[32];
  sprintf(cmd, "AT+CIPSEND=%d", len);

  if (sendCommand(cmd, ">", 2000)) {
    HAL_UART_Transmit(_huart, (uint8_t *)data, len, 2000);
    return waitForResponse("SEND OK", 5000);
  }
  return false;
}

bool ESP8266::startTCP(const char *host, uint16_t port) {
  char cmd[128];
  sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
  return sendCommand(cmd, "CONNECT", 10000);
}

bool ESP8266::closeConnection(void) {
  return sendCommand("AT+CIPCLOSE", "CLOSED", 3000);
}

bool ESP8266::isConnected(void) { return (_state == 2 || _state == 3); }

void ESP8266::disconnect(void) {
  sendCommand("AT+CWQAP", "OK", 3000);
  _state = 0;
}

bool ESP8266::scanNetworks(void) {
  if (_hard_disabled) {
    LOG_W("ESP", "scanNetworks blocked: hard-disabled");
    return false;
  }

  clearRxBuffer();
  const char cmd[] = "AT+CWLAP\r\n";
  HAL_UART_Transmit(_huart, (uint8_t *)cmd, (uint16_t)(sizeof(cmd) - 1U), 1000);

  uint32_t start = HAL_GetTick();
  uint32_t last_dbg = 0;
  bool near_full_logged = false;

  while (HAL_GetTick() - start < 15000U) {
    processPendingData();

    if (_rx_index > 0) {
      const char *rx = (const char *)_rx_buffer;

      if (_rx_overflow) {
        LOG_E("ESP", "CWLAP RX overflow after %lums, rx=%u/%u",
              (unsigned long)(HAL_GetTick() - start), _rx_index,
              (unsigned)sizeof(_rx_buffer));
        break;
      }

      if (strstr(rx, "\r\nOK") || strstr(rx, "OK\r\n")) {
        LOG_D("ESP", "CWLAP OK after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rx_index);
        esp_led_success();
        return true;
      }

      if (strstr(rx, "ERROR") || strstr(rx, "FAIL")) {
        LOG_E("ESP", "CWLAP ERROR after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rx_index);
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      if (!near_full_logged && _rx_index >= sizeof(_rx_buffer) - 64) {
        near_full_logged = true;
        LOG_W("ESP", "CWLAP RX nearly full (%u/%u), will stop on overflow",
              _rx_index, (unsigned)sizeof(_rx_buffer));
      }

      if (HAL_GetTick() - last_dbg > 3000U) {
        last_dbg = HAL_GetTick();
        LOG_D("ESP", "CWLAP waiting... %lums, rx=%u/%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rx_index,
              (unsigned)sizeof(_rx_buffer));
      }
    }

    HAL_Delay(10);
    SysWatchdog_Tick();
  }

  LOG_E("ESP", "CWLAP failed after %lums, rx=%u/%u",
        (unsigned long)(HAL_GetTick() - start), _rx_index,
        (unsigned)sizeof(_rx_buffer));

  /* Let late scan output drain briefly, then verify the module still answers.
   * The WLAN page will decide whether to retry; keep this function from
   * polluting later HTTP/heartbeat transactions. */
  uint32_t settle = HAL_GetTick();
  while (HAL_GetTick() - settle < 300U) {
    processPendingData();
    HAL_Delay(10);
    SysWatchdog_Tick();
  }
  clearRxBuffer();
  (void)sendCommand("AT", "OK", 1000);
  clearRxBuffer();
  esp_led_failure();
  return false;
}

bool ESP8266::getIP(char *ip_buffer, uint16_t buffer_size) {
  clearRxBuffer();
  sendCommand("AT+CIFSR", "", 2000);

  const char *ip_start = strstr((char *)_rx_buffer, "STAIP");
  if (ip_start) {
    ip_start = strchr(ip_start, '"');
    if (ip_start) {
      ip_start++;
      const char *ip_end = strchr(ip_start, '"');
      if (ip_end) {
        uint16_t len = ip_end - ip_start;
        if (len < buffer_size) {
          strncpy(ip_buffer, ip_start, len);
          ip_buffer[len] = '\0';
          return true;
        }
      }
    }
  }
  return false;
}


static bool esp_parse_rssi_from_cwjap(const char *rx, int *rssi) {
  if (!rx || !rssi) return false;
  const char *p = strstr(rx, "+CWJAP:");
  if (!p) return false;

  const char *line_end = strpbrk(p, "\r\n");
  if (!line_end) line_end = p + strlen(p);

  /* ESP AT variants differ.  Some return:
   *   +CWJAP:"ssid","bssid",channel,rssi
   * older ones may omit RSSI.  Scan every comma-separated numeric token on the
   * +CWJAP line and use the last plausible RSSI value. */
  bool found = false;
  int best = 0;
  const char *q = p;
  while (q && q < line_end) {
    q = strchr(q, ',');
    if (!q || q >= line_end) break;
    q++;
    while (q < line_end && (*q == ' ' || *q == '\t' || *q == '"')) q++;
    char *endp = NULL;
    long v = strtol(q, &endp, 10);
    if (endp && endp > q) {
      /* RSSI must be a negative dBm value.  Do not treat channel numbers
       * such as 0/1/6/11 as RSSI; that was why the cloud kept seeing 0 dBm. */
      if (v < 0 && v >= -127) {
        best = (int)v;
        found = true;
      }
      q = endp;
    }
  }

  if (!found) return false;
  *rssi = best;
  return true;
}

bool ESP8266::getRSSI(int *rssi) {
  if (!rssi) return false;
  if (_hard_disabled) return false;

  const char *cmds[] = {"AT+CWJAP?", "AT+CWJAP_CUR?"};
  for (unsigned i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
    char tx[24];
    int n = snprintf(tx, sizeof(tx), "%s\r\n", cmds[i]);
    if (n <= 0 || n >= (int)sizeof(tx)) continue;

    clearRxBuffer();
    HAL_UART_Transmit(_huart, (uint8_t *)tx, (uint16_t)n, 300);

    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 900U) {
      processPendingData();
      const char *rx = (const char *)_rx_buffer;
      if (_rx_overflow) break;
      if (strstr(rx, "OK")) {
        int v = 0;
        if (esp_parse_rssi_from_cwjap(rx, &v)) {
          *rssi = v;
          LOG_D("ESP", "RSSI=%d dBm", v);
          clearRxBuffer();
          return true;
        }
        break;
      }
      if (strstr(rx, "ERROR") || strstr(rx, "FAIL")) break;
      HAL_Delay(10);
      SysWatchdog_Tick();
    }
  }

  clearRxBuffer();
  return false;
}

bool ESP8266::sendString(const char *str) {
  return sendData((const uint8_t *)str, strlen(str));
}

// ==================== C interface implementation ====================

void ESP8266_Init(void) { esp8266.init(); }

bool ESP8266_IsHardDisabled(void) { return esp8266.isHardDisabled(); }

bool ESP8266_TryRecover(bool force) { return esp8266.tryRecover(force); }
uint16_t ESP8266_GetRecoveryFailureCount(void) { return esp8266.recoveryFailureCount(); }
void ESP8266_ClearRecoveryFailureCount(void) { esp8266.clearRecoveryFailureCount(); }

bool ESP8266_SendCommand(const char *cmd, const char *expected_response,
                         uint32_t timeout_ms) {
  if (esp8266.isHardDisabled()) {
    LOG_W("ESP", "SendCommand blocked: ESP8266 is hard-disabled");
    return false;
  }
  return esp8266.sendCommand(cmd, expected_response, timeout_ms);
}

bool ESP8266_ConnectWiFi(const char *ssid, const char *password) {
  if (esp8266.isHardDisabled()) {
    LOG_W("ESP", "ConnectWiFi blocked: ESP8266 is hard-disabled");
    return false;
  }
  return esp8266.connectWiFi(ssid, password);
}

bool ESP8266_SendData(const uint8_t *data, uint16_t len) {
  if (esp8266.isHardDisabled()) {
    LOG_W("ESP", "SendData blocked: ESP8266 is hard-disabled");
    return false;
  }
  return esp8266.sendData(data, len);
}

bool ESP8266_StartTCP(const char *host, uint16_t port) {
  if (esp8266.isHardDisabled()) {
    LOG_W("ESP", "StartTCP blocked: ESP8266 is hard-disabled");
    return false;
  }
  return esp8266.startTCP(host, port);
}

int ESP8266_GetState(void) {
  if (esp8266.isHardDisabled()) return 4; // 4 = error / hard-disabled
  return esp8266.getState();
}

bool ESP8266_IsConnected(void) {
  if (esp8266.isHardDisabled()) return false;
  return esp8266.isConnected();
}

void ESP8266_Disconnect(void) {
  if (esp8266.isHardDisabled()) return;
  esp8266.disconnect();
}

bool ESP8266_GetIP(char *buf, uint16_t sz) {
  if (esp8266.isHardDisabled()) return false;
  return esp8266.getIP(buf, sz);
}

bool ESP8266_GetRSSI(int *rssi) {
  if (esp8266.isHardDisabled()) return false;
  return esp8266.getRSSI(rssi);
}
