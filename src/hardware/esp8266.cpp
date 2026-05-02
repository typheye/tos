#include "hardware/include/esp8266.hpp"
#include "hardware/include/usart.hpp"
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
    printf("[PROCESS] Got data, len=%d\r\n", esp8266_global_index);
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
    printf("ESP8266: Got IP\r\n");
  } else if (strstr(response, "CONNECT") != NULL) {
    _state = 2;
    printf("ESP8266: Connected\r\n");
  } else if (strstr(response, "CLOSED") != NULL ||
             strstr(response, "DISCONNECT") != NULL) {
    _state = 0;
    printf("ESP8266: Disconnected\r\n");
  }
}

void ESP8266::processRxData(uint8_t *data, uint16_t len) {
  printf("[ESP8266 RAW] ");
  for (uint16_t i = 0; i < len; i++) {
    if (data[i] >= 0x20 && data[i] <= 0x7E) {
      printf("%c", data[i]);
    } else if (data[i] == '\r') {
      printf("\\r");
    } else if (data[i] == '\n') {
      printf("\\n");
    } else {
      printf("[%02X]", data[i]);
    }
  }
  printf("\r\n");

  for (uint16_t i = 0; i < len && _rx_index < sizeof(_rx_buffer) - 1; i++) {
    _rx_buffer[_rx_index++] = data[i];
  }
  _rx_buffer[_rx_index] = '\0';
}

void ESP8266::init(void) {
  printf("ESP8266: Initializing...\r\n");
  printf("UART2 RX count before: %lu\r\n", uart2_rx_count);

  clearRxBuffer();
  HAL_Delay(500);

  printf("Sending AT command...\r\n");
  if (sendCommand("AT", "OK", 3000)) {
    printf("ESP8266: AT OK\r\n");
    sendCommand("ATE0", "OK", 1000);
  } else {
    printf("ESP8266: No response!\r\n");
    printf("UART2 RX count after: %lu\r\n", uart2_rx_count);
  }
}

bool ESP8266::sendCommand(const char *cmd, const char *expected_response,
                          uint32_t timeout_ms) {
  char buffer[128];

  clearRxBuffer();

  sprintf(buffer, "%s\r\n", cmd);

  printf("\r\n[ESP8266 SEND] %s\r\n", cmd);

  HAL_UART_Transmit(_huart, (uint8_t *)buffer, strlen(buffer), 1000);

  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    processPendingData();

    if (_rx_index > 0) {
      printf("[ESP8266 RSP] %s\r\n", (char *)_rx_buffer);

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

  printf("[ESP8266 TIMEOUT]\r\n");
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