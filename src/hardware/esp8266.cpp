#include "hardware/include/esp8266.hpp"
#include "hardware/include/usart.hpp"
#include "syslog.h"
#include <stdio.h>
#include <string.h>

extern USART boardSerial;

extern "C" {
extern uint8_t esp8266_global_buffer[32];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
extern volatile uint32_t uart2_rx_count;
}

// 全局实例
ESP8266 esp8266(&huart2);

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
}

void ESP8266::processPendingData(void) {
  if (esp8266_data_ready) {
    esp8266_data_ready = 0;
    /* LOG_D("ESP", "PROCESS: Got data, len=%d", esp8266_global_index); */
    processRxData(esp8266_global_buffer, esp8266_global_index);
  }
}

bool ESP8266::waitForResponse(const char *expected, uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      if (expected && strstr((char *)_rx_buffer, expected) != NULL) {
        return true;
      }

      if (strstr((char *)_rx_buffer, "ERROR") != NULL ||
          strstr((char *)_rx_buffer, "FAIL") != NULL) {
        return false;
      }
    }
    HAL_Delay(10);
  }
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
  LOG_I("ESP", "UART2 RX count before: %lu", (unsigned long)uart2_rx_count);

  clearRxBuffer();
  HAL_Delay(500);

  LOG_I("ESP", "Sending AT command...");
  if (sendCommand("AT", "OK", 3000)) {
    LOG_I("ESP", "AT OK");
    sendCommand("ATE0", "OK", 1000);
  } else {
    LOG_E("ESP", "No response!");
    LOG_I("ESP", "UART2 RX count after: %lu", (unsigned long)uart2_rx_count);
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

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      /* LOG_D("ESP", "RSP: %s", (char *)_rx_buffer); */

      if (expected_response) {
        if (strstr((char *)_rx_buffer, expected_response) != NULL) {
          return true;
        }
      } else {
        return true;
      }

      if (strstr((char *)_rx_buffer, "ERROR") != NULL ||
          strstr((char *)_rx_buffer, "FAIL") != NULL) {
        return false;
      }
    }
    HAL_Delay(10);
  }

  LOG_E("ESP", "TIMEOUT");
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

bool ESP8266::scanNetworks(void) {
  return sendCommand("AT+CWLAP", "OK", 10000);
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