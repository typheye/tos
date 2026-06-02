#include "hardware/include/esp8266.hpp"
#include "hardware/include/led.hpp"
#include "hardware/include/usart.hpp"
#include "syslog.h"
#include <stdio.h>
#include <string.h>

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram"), aligned(4)))
#endif

extern USART boardSerial;

extern "C" {
extern uint8_t esp8266_global_buffer[512];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
extern volatile uint32_t uart2_rx_count;
}

// 全局实例
CCMRAM ESP8266 esp8266(&huart2);

// ==================== C++ 类实现 ====================

ESP8266::ESP8266(UART_HandleTypeDef *huart) {
  _huart = huart;
  _state = 0;
  _rx_index = 0;
  memset(_rx_buffer, 0, sizeof(_rx_buffer));
}

void ESP8266::clearRxBuffer(void) {
  _rx_index = 0;
  memset(_rx_buffer, 0, sizeof(_rx_buffer));
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  memset(esp8266_global_buffer, 0, 512);
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
  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      if (expected && strstr((char *)_rx_buffer, expected) != NULL) {
        /* Communication success: brief blink to acknowledge */
        boardLed.on();
        HAL_Delay(100);
        boardLed.off();
        return true;
      }

      if (strstr((char *)_rx_buffer, "ERROR") != NULL ||
          strstr((char *)_rx_buffer, "FAIL") != NULL) {
        /* Communication failure: keep boardLed on until next success */
        boardLed.on();
        return false;
      }
    }
    HAL_Delay(10);
  }
  /* Timeout: communication failure */
  boardLed.on();
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
  for (uint16_t i = 0; i < len && _rx_index < sizeof(_rx_buffer) - 1; i++) {
    _rx_buffer[_rx_index++] = data[i];
  }
  _rx_buffer[_rx_index] = '\0';
}

void ESP8266::init(void) {
  LOG_I("ESP", "Initializing...");
  LOG_D("ESP", "UART2 RX count before: %lu", (unsigned long)uart2_rx_count);

  /* Flush any stale boot data from ESP8266 */
  clearRxBuffer();
  HAL_Delay(500);
  processPendingData();
  clearRxBuffer();
  HAL_Delay(200);

  LOG_D("ESP", "Sending AT test...");
  if (sendCommand("AT", "OK", 3000)) {
    LOG_I("ESP", "AT OK");
    sendCommand("ATE0", "OK", 1000);
  } else {
    LOG_E("ESP", "No response to AT! Check wiring/power.");
    LOG_D("ESP", "UART2 RX count: %lu", (unsigned long)uart2_rx_count);
  }
}

bool ESP8266::sendCommand(const char *cmd, const char *expected_response,
                          uint32_t timeout_ms) {
  char buffer[128];

  clearRxBuffer();

  sprintf(buffer, "%s\r\n", cmd);

  /* LOG_D("ESP", "SEND: %s", cmd); */

  HAL_UART_Transmit(_huart, (uint8_t *)buffer, strlen(buffer), 1000);

  uint32_t start = HAL_GetTick();
  uint32_t last_dbg = 0;

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      if (expected_response) {
        if (strstr((char *)_rx_buffer, expected_response) != NULL) {
          LOG_D("ESP", "OK after %lums, rx=%u bytes",
                (unsigned long)(HAL_GetTick() - start), _rx_index);
          /* Communication success: brief blink to acknowledge */
          boardLed.on();
          HAL_Delay(100);
          boardLed.off();
          return true;
        }
        /* Buffer nearly full — search for partial match */
        if (_rx_index >= sizeof(_rx_buffer) - 10) {
          LOG_W("ESP", "Rx buffer nearly full (%u/%u), searching for '%s'",
                _rx_index, (unsigned)sizeof(_rx_buffer), expected_response);
        }
      } else {
        /* No expected response specified — any data counts as success */
        boardLed.on();
        HAL_Delay(100);
        boardLed.off();
        return true;
      }

      if (strstr((char *)_rx_buffer, "ERROR") != NULL ||
          strstr((char *)_rx_buffer, "FAIL") != NULL) {
        LOG_E("ESP", "Got ERROR/FAIL after %lums, rx=%u bytes",
              (unsigned long)(HAL_GetTick() - start), _rx_index);
        /* Communication failure: keep boardLed on until next success */
        boardLed.on();
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
  }

  LOG_E("ESP", "TIMEOUT after %lums, rx=%u bytes",
        (unsigned long)timeout_ms, _rx_index);
  /* Communication failure: keep boardLed on until next success */
  boardLed.on();
  return false;
}

bool ESP8266::connectWiFi(const char *ssid, const char *password) {
  char cmd[256];
  sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

  _state = 1;
  bool result = sendCommand(cmd, "OK", 15000);

  if (result) {
    HAL_Delay(2000);
    _state = 3;
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
  return sendCommand("AT+CWLAP", "OK", 15000);
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

bool ESP8266::sendString(const char *str) {
  return sendData((const uint8_t *)str, strlen(str));
}

// ==================== C 接口实现 ====================

void ESP8266_Init(void) { esp8266.init(); }

bool ESP8266_SendCommand(const char *cmd, const char *expected_response,
                         uint32_t timeout_ms) {
  return esp8266.sendCommand(cmd, expected_response, timeout_ms);
}

bool ESP8266_ConnectWiFi(const char *ssid, const char *password) {
  return esp8266.connectWiFi(ssid, password);
}

bool ESP8266_SendData(const uint8_t *data, uint16_t len) {
  return esp8266.sendData(data, len);
}

bool ESP8266_StartTCP(const char *host, uint16_t port) {
  return esp8266.startTCP(host, port);
}

int ESP8266_GetState(void) { return esp8266.getState(); }

bool ESP8266_IsConnected(void) { return esp8266.isConnected(); }

void ESP8266_Disconnect(void) { esp8266.disconnect(); }

bool ESP8266_GetIP(char *buf, uint16_t sz) { return esp8266.getIP(buf, sz); }
