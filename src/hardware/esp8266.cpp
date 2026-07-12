/**
 ******************************************************************************
 * @file    esp8266.cpp
 * @author  Typheye
 * @brief   ESP8266 AT driver implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "include/esp8266.hpp"
#include "library/include/libdly.h"

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram"), aligned(4)))
#endif

extern USART boardSerial;

static void esp_led_success(void) { LED_EspCommSuccess(); }

static void esp_led_failure(void) { LED_EspCommFailure(); }

extern "C" {
extern uint8_t esp8266_global_buffer[];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
extern volatile uint32_t uart2_rx_count;
}

#define ESP8266_EN_PORT GPIOF
#define ESP8266_EN_PIN GPIO_PIN_0
#define ESP8266_RST_PORT GPIOF
#define ESP8266_RST_PIN GPIO_PIN_1

// Global instance
CCMRAM ESP8266 esp8266(&huart2);

// ==================== C++ class implementation ====================

ESP8266::ESP8266(UART_HandleTypeDef *huart) {
  _huart = huart;
  _state = 0;
  _hardDisabled = false;
  _lastRecoverMs = 0;
  _recoverAttempts = 0;
  _recoverFailures = 0;
  _uartRearms = 0;
  _rxIndex = 0;
  _rxOverflow = false;
  memset(_rxBuffer, 0, sizeof(_rxBuffer));
}

void ESP8266::serviceUartRx(void) {
  if (!_huart || !_huart->Instance)
    return;

  USART_TypeDef *uart = _huart->Instance;
  uint32_t sr = uart->SR;
  bool error =
      (sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U;
  bool rx_irq_off = (uart->CR1 & USART_CR1_RXNEIE) == 0U;
  bool err_irq_off = (uart->CR3 & USART_CR3_EIE) == 0U;

  if (!error && !rx_irq_off && !err_irq_off)
    return;

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (error) {
    /* STM32F4 clears ORE/NE/FE/PE by reading SR followed by DR. */
    volatile uint32_t clear_sr = uart->SR;
    volatile uint32_t clear_dr = uart->DR;
    (void)clear_sr;
    (void)clear_dr;
    _huart->ErrorCode = HAL_UART_ERROR_NONE;
  }

  __HAL_UART_ENABLE_IT(_huart, UART_IT_RXNE);
  __HAL_UART_ENABLE_IT(_huart, UART_IT_ERR);

  if (primask == 0U)
    __enable_irq();

  if (_uartRearms < 0xFFFFU)
    _uartRearms++;
  LOG_W("ESP", "UART2 RX rearmed #%u sr=0x%08lX cr1=0x%08lX cr3=0x%08lX",
        (unsigned)_uartRearms, (unsigned long)sr, (unsigned long)uart->CR1,
        (unsigned long)uart->CR3);
}

void ESP8266::clearRxBuffer(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  _rxIndex = 0;
  _rxOverflow = false;
  memset(_rxBuffer, 0, sizeof(_rxBuffer));
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  memset(esp8266_global_buffer, 0, 2048);
  if (primask == 0U)
    __enable_irq();
}

void ESP8266::processPendingData(void) {
  serviceUartRx();

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (!esp8266_data_ready || esp8266_global_index == 0U) {
    if (primask == 0U)
      __enable_irq();
    return;
  }

  uint16_t len = esp8266_global_index;
  /* Keep the ISR-owned staging buffer atomic with respect to reset/copy.
   * Without this guard an RX interrupt can append while the foreground resets
   * the index, losing bytes and joining two unrelated AT responses. */
  processRxData(esp8266_global_buffer, len);
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  esp8266_global_buffer[0] = '\0';

  if (primask == 0U)
    __enable_irq();
}

void ESP8266::resetRxBuffer(void) { clearRxBuffer(); }

void ESP8266::waitWithService(uint32_t delay_ms) {
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < delay_ms) {
    serviceUartRx();
    processPendingData();
    JPDelay(20U);
    SysWatchdog_Tick();
  }
}

void ESP8266::driveControlPins(bool en_high, bool rst_high) {
  __HAL_RCC_GPIOF_CLK_ENABLE();
  HAL_GPIO_WritePin(ESP8266_EN_PORT, ESP8266_EN_PIN,
                    en_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN,
                    rst_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void ESP8266::hardwareReset(bool cycle_en, uint32_t boot_wait_ms) {
  LOG_W("ESP", "Hardware reset via EN/RST%s", cycle_en ? " (power-cycle)" : "");

  _state = 0;
  clearRxBuffer();

  if (cycle_en) {
    driveControlPins(false, false);
    waitWithService(250U);
    driveControlPins(true, false);
    waitWithService(120U);
  } else {
    driveControlPins(true, false);
    waitWithService(120U);
  }

  driveControlPins(true, true);
  waitWithService(boot_wait_ms);
  serviceUartRx();
  clearRxBuffer();
}

bool ESP8266::waitForResponse(const char *expected, uint32_t timeout_ms) {
  if (_hardDisabled)
    return false;

  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rxIndex > 0) {
      if (_rxOverflow) {
        LOG_E("ESP", "RX overflow while waiting for %s",
              expected ? expected : "(any)");
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      if (expected && strstr((char *)_rxBuffer, expected) != NULL) {
        /* Communication success: brief blink to acknowledge */
        esp_led_success();
        return true;
      }

      if (strstr((char *)_rxBuffer, "ERROR") != NULL ||
          strstr((char *)_rxBuffer, "FAIL") != NULL) {
        /* Communication failure: blink warn LED without blocking */
        clearRxBuffer();
        esp_led_failure();
        return false;
      }
    }
    JPDelay(10);
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
    if (_rxIndex >= sizeof(_rxBuffer) - 1) {
      _rxOverflow = true;
      break;
    }
    _rxBuffer[_rxIndex++] = data[i];
  }
  _rxBuffer[_rxIndex] = '\0';
}

bool ESP8266::tryRecover(bool force) {
  uint32_t now = HAL_GetTick();
  if (!force && _lastRecoverMs != 0U &&
      (uint32_t)(now - _lastRecoverMs) < 60000U) {
    return false;
  }

  _lastRecoverMs = now;
  if (_recoverAttempts < 0xFFU)
    _recoverAttempts++;
  LOG_W("ESP", "Bounded recovery attempt #%u, uart_rx=%lu",
        (unsigned)_recoverAttempts, (unsigned long)uart2_rx_count);

  _hardDisabled = false;
  serviceUartRx();
  clearRxBuffer();

  /* First distinguish a dead TCP/WLAN state from a dead AT/UART path. */
  if (sendCommand("AT", "OK", 1200U)) {
    LOG_I("ESP", "AT interface alive after UART rearm");
    _recoverFailures = 0;
    return true;
  }

  hardwareReset(true, 3200U);
  if (sendCommand("AT", "OK", 2000U)) {
    (void)sendCommand("ATE0", "OK", 1000U);
    (void)sendCommand("AT+CIPMODE=0", "OK", 1000U);
    (void)sendCommand("AT+CIPMUX=0", "OK", 1000U);
    _hardDisabled = false;
    _state = 0;
    _recoverFailures = 0;
    LOG_I("ESP", "ESP8266 recovered after EN/RST reset");
    return true;
  }

  if (_recoverFailures < 0xFFFFU)
    _recoverFailures++;
  _hardDisabled = true;
  _state = 4;
  LOG_E("ESP", "ESP8266 recovery failed, uart_rx=%lu",
        (unsigned long)uart2_rx_count);
  return false;
}

void ESP8266::init(void) {
  LOG_I("ESP", "Initializing...");
  LOG_D("ESP", "UART2 RX count before: %lu", (unsigned long)uart2_rx_count);

  _hardDisabled = false;
  _state = 0;

  hardwareReset(true, 2800U);

  LOG_D("ESP", "Sending AT test...");
  if (sendCommand("AT", "OK", 3000)) {
    LOG_I("ESP", "AT OK");
    sendCommand("ATE0", "OK", 1000);
    sendCommand("AT+CIPMODE=0", "OK", 1000);
    sendCommand("AT+CIPMUX=0", "OK", 1000);
  } else {
    LOG_D("ESP", "UART2 RX count: %lu", (unsigned long)uart2_rx_count);
    LOG_W("ESP", "Initial AT failed; trying one module-local reset");
    if (!tryRecover(true)) {
      _hardDisabled = false;
      bool late_ok = false;
      for (uint8_t i = 0; i < 2U && !late_ok; ++i) {
        LOG_W("ESP", "Late boot AT probe %u/2", (unsigned)(i + 1U));
        uint32_t wait_start = HAL_GetTick();
        while ((uint32_t)(HAL_GetTick() - wait_start) < 3000U) {
          serviceUartRx();
          JPDelay(20U);
          SysWatchdog_Tick();
        }
        serviceUartRx();
        clearRxBuffer();
        late_ok = sendCommand("AT", "OK", 2000U);
      }

      if (late_ok) {
        LOG_I("ESP", "AT OK after late boot probe");
        (void)sendCommand("ATE0", "OK", 1000U);
        (void)sendCommand("AT+CIPMODE=0", "OK", 1000U);
        (void)sendCommand("AT+CIPMUX=0", "OK", 1000U);
        _hardDisabled = false;
        _state = 0;
      } else {
        _hardDisabled = true;
        _state = 4;
        LOG_E("ESP", "ESP8266 unavailable for this boot");
      }
    }
  }
}

bool ESP8266::sendCommand(const char *cmd, const char *expected_response,
                          uint32_t timeout_ms) {
  if (_hardDisabled) {
    LOG_W("ESP", "sendCommand blocked: hard-disabled");
    return false;
  }

  char buffer[128];

  serviceUartRx();
  clearRxBuffer();

  sprintf(buffer, "%s\r\n", cmd);

  /* LOG_D("ESP", "SEND: %s", cmd); */

  HAL_UART_Transmit(_huart, (uint8_t *)buffer, strlen(buffer), 1000);

  uint32_t start = HAL_GetTick();
  uint32_t last_dbg = 0;
  bool near_full_logged = false;

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rxIndex > 0) {
      if (_rxOverflow) {
        LOG_E("ESP", "RX overflow after %lums, rx=%u/%u while waiting for '%s'",
              (unsigned long)(HAL_GetTick() - start), _rxIndex,
              (unsigned)sizeof(_rxBuffer),
              expected_response ? expected_response : "(any)");
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      if (expected_response) {
        if (strstr((char *)_rxBuffer, expected_response) != NULL) {
          LOG_D("ESP", "OK after %lums, rx=%u bytes",
                (unsigned long)(HAL_GetTick() - start), _rxIndex);
          /* Communication success: brief blink to acknowledge */
          esp_led_success();
          return true;
        }
        /* Buffer nearly full search for partial match */
        if (!near_full_logged && _rxIndex >= sizeof(_rxBuffer) - 64) {
          near_full_logged = true;
          LOG_W("ESP", "Rx buffer nearly full (%u/%u), searching for '%s'",
                _rxIndex, (unsigned)sizeof(_rxBuffer), expected_response);
        }
      } else {
        /* No expected response specified any data counts as success */
        esp_led_success();
        return true;
      }

      if (strstr((char *)_rxBuffer, "ERROR") != NULL ||
          strstr((char *)_rxBuffer, "FAIL") != NULL) {
        LOG_E("ESP", "Got ERROR/FAIL after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rxIndex);
        /* Communication failure: blink warn LED without blocking */
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      /* Periodic debug during long waits */
      if (timeout_ms > 3000 && HAL_GetTick() - last_dbg > 2000) {
        last_dbg = HAL_GetTick();
        LOG_D("ESP", "Waiting... %lums, rx=%u/%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rxIndex,
              (unsigned)sizeof(_rxBuffer));
      }
    }
    JPDelay(10);
    SysWatchdog_Tick();
  }

  LOG_E("ESP", "TIMEOUT after %lums, rx=%u bytes", (unsigned long)timeout_ms,
        _rxIndex);
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
    JPDelay(2000);
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
  if (_hardDisabled) {
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

    if (_rxIndex > 0) {
      const char *rx = (const char *)_rxBuffer;

      if (_rxOverflow) {
        LOG_E("ESP", "CWLAP RX overflow after %lums, rx=%u/%u",
              (unsigned long)(HAL_GetTick() - start), _rxIndex,
              (unsigned)sizeof(_rxBuffer));
        break;
      }

      if (strstr(rx, "\r\nOK") || strstr(rx, "OK\r\n")) {
        LOG_D("ESP", "CWLAP OK after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rxIndex);
        esp_led_success();
        return true;
      }

      if (strstr(rx, "ERROR") || strstr(rx, "FAIL")) {
        LOG_E("ESP", "CWLAP ERROR after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rxIndex);
        clearRxBuffer();
        esp_led_failure();
        return false;
      }

      if (!near_full_logged && _rxIndex >= sizeof(_rxBuffer) - 64) {
        near_full_logged = true;
        LOG_W("ESP", "CWLAP RX nearly full (%u/%u), will stop on overflow",
              _rxIndex, (unsigned)sizeof(_rxBuffer));
      }

      if (HAL_GetTick() - last_dbg > 3000U) {
        last_dbg = HAL_GetTick();
        LOG_D("ESP", "CWLAP waiting... %lums, rx=%u/%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rxIndex,
              (unsigned)sizeof(_rxBuffer));
      }
    }

    JPDelay(10);
    SysWatchdog_Tick();
  }

  LOG_E("ESP", "CWLAP failed after %lums, rx=%u/%u",
        (unsigned long)(HAL_GetTick() - start), _rxIndex,
        (unsigned)sizeof(_rxBuffer));

  /* Let late scan output drain briefly, then verify the module still answers.
   * The WLAN page will decide whether to retry; keep this function from
   * polluting later HTTP/heartbeat transactions. */
  uint32_t settle = HAL_GetTick();
  while (HAL_GetTick() - settle < 300U) {
    processPendingData();
    JPDelay(10);
    SysWatchdog_Tick();
  }
  clearRxBuffer();
  (void)sendCommand("AT", "OK", 1000);
  clearRxBuffer();
  esp_led_failure();
  return false;
}

bool ESP8266::getIP(char *ip_buffer, uint16_t buffer_size) {
  if (!ip_buffer || buffer_size == 0U)
    return false;
  ip_buffer[0] = '\0';

  clearRxBuffer();
  if (!sendCommand("AT+CIFSR", "OK", 2000)) {
    clearRxBuffer();
    return false;
  }

  const char *ip_start = strstr((char *)_rxBuffer, "STAIP");
  if (ip_start) {
    ip_start = strchr(ip_start, '"');
    if (ip_start) {
      ip_start++;
      const char *ip_end = strchr(ip_start, '"');
      if (ip_end) {
        uint16_t len = ip_end - ip_start;
        if (len < buffer_size && len > 0U) {
          strncpy(ip_buffer, ip_start, len);
          ip_buffer[len] = '\0';
          if (strcmp(ip_buffer, "0.0.0.0") == 0)
            return false;
          return true;
        }
      }
    }
  }
  return false;
}

static bool esp_parse_rssi_from_cwjap(const char *rx, int *rssi) {
  if (!rx || !rssi)
    return false;
  const char *p = strstr(rx, "+CWJAP:");
  if (!p)
    return false;

  const char *line_end = strpbrk(p, "\r\n");
  if (!line_end)
    line_end = p + strlen(p);

  /* ESP AT variants differ.  Some return:
   *   +CWJAP:"ssid","bssid",channel,rssi
   * older ones may omit RSSI.  Scan every comma-separated numeric token on the
   * +CWJAP line and use the last plausible RSSI value. */
  bool found = false;
  int best = 0;
  const char *q = p;
  while (q && q < line_end) {
    q = strchr(q, ',');
    if (!q || q >= line_end)
      break;
    q++;
    while (q < line_end && (*q == ' ' || *q == '\t' || *q == '"'))
      q++;
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

  if (!found)
    return false;
  *rssi = best;
  return true;
}

bool ESP8266::getRSSI(int *rssi) {
  if (!rssi)
    return false;
  if (_hardDisabled)
    return false;

  const char *cmds[] = {"AT+CWJAP?", "AT+CWJAP_CUR?"};
  for (unsigned i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
    char tx[24];
    int n = snprintf(tx, sizeof(tx), "%s\r\n", cmds[i]);
    if (n <= 0 || n >= (int)sizeof(tx))
      continue;

    clearRxBuffer();
    HAL_UART_Transmit(_huart, (uint8_t *)tx, (uint16_t)n, 300);

    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 900U) {
      processPendingData();
      const char *rx = (const char *)_rxBuffer;
      if (_rxOverflow)
        break;
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
      if (strstr(rx, "ERROR") || strstr(rx, "FAIL"))
        break;
      JPDelay(10);
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
void ESP8266_ServiceUartRx(void) { esp8266.serviceUartRx(); }
uint32_t ESP8266_GetUartRxCount(void) { return uart2_rx_count; }
uint16_t ESP8266_GetRecoveryFailureCount(void) {
  return esp8266.recoveryFailureCount();
}
void ESP8266_ClearRecoveryFailureCount(void) {
  esp8266.clearRecoveryFailureCount();
}

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
  if (esp8266.isHardDisabled())
    return 4; // 4 = error / hard-disabled
  return esp8266.getState();
}

bool ESP8266_IsConnected(void) {
  if (esp8266.isHardDisabled())
    return false;
  return esp8266.isConnected();
}

void ESP8266_Disconnect(void) {
  if (esp8266.isHardDisabled())
    return;
  esp8266.disconnect();
}

bool ESP8266_GetIP(char *buf, uint16_t sz) {
  if (esp8266.isHardDisabled())
    return false;
  return esp8266.getIP(buf, sz);
}

bool ESP8266_GetRSSI(int *rssi) {
  if (esp8266.isHardDisabled())
    return false;
  return esp8266.getRSSI(rssi);
}
