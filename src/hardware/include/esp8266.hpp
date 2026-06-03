/**
 ******************************************************************************
 * @file    esp8266.hpp
 * @author  Typheye
 * @brief   ESP8266 AT driver interface.
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

#ifndef ESP8266_HPP
#define ESP8266_HPP

#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include "hardware/include/led.hpp"
#include "hardware/include/usart.hpp"
#include "include/syshandle.h"
#include "core/sys/include/syslog.h"
#include "core/sys/include/syswatchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== C-compatible interface ====================

/**
 * @brief Initialize ESP8266.
 */
void ESP8266_Init(void);

/**
 * @brief Process received data from the interrupt path.
 * @param data Data pointer.
 * @param len Data length.
 */
void ESP8266_ProcessRxData(uint8_t *data, uint16_t len);

/**
 * @brief Send an AT command.
 * @param cmd Command string.
 * @param expected_response Expected response token.
 * @param timeout_ms Timeout in milliseconds.
 * @return true on success, false on failure.
 */
bool ESP8266_SendCommand(const char *cmd, const char *expected_response,
                         uint32_t timeout_ms);

/**
 * @brief Join a Wi-Fi network.
 * @param ssid Wi-Fi SSID.
 * @param password Wi-Fi password.
 * @return true on success, false on failure.
 */
bool ESP8266_ConnectWiFi(const char *ssid, const char *password);

/**
 * @brief Send raw data.
 * @param data Data pointer.
 * @param len Data length.
 * @return true on success, false on failure.
 */
bool ESP8266_SendData(const uint8_t *data, uint16_t len);

/**
 * @brief Start a TCP connection.
 * @param host Host name.
 * @param port TCP port.
 * @return true on success, false on failure.
 */
bool ESP8266_StartTCP(const char *host, uint16_t port);

/**
 * @brief Get ESP8266 state.
 * @return 0=disconnected, 1=joining, 2=connected, 3=got IP, 4=error.
 */
int ESP8266_GetState(void);

/**
 * @brief Check whether ESP8266 is connected.
 * @return true if connected, false otherwise.
 */
bool ESP8266_IsConnected(void);
void ESP8266_Disconnect(void);
bool ESP8266_GetIP(char *buf, uint16_t sz);
bool ESP8266_GetRSSI(int *rssi);

/**
 * @brief Check whether ESP8266 is unavailable for this boot.
 *        Hard-disabled means the module failed boot-time initialization.
 * @return true if unavailable, false if usable.
 */
bool ESP8266_IsHardDisabled(void);

/**
 * @brief Runtime ESP recovery is disabled by policy; this returns false.
 * @param force ignored.
 * @return false.
 */
bool ESP8266_TryRecover(bool force);
uint16_t ESP8266_GetRecoveryFailureCount(void);
void ESP8266_ClearRecoveryFailureCount(void);

#ifdef __cplusplus
}
#endif

// ==================== C++ class definition ====================

#ifdef __cplusplus

class ESP8266 {
public:
  static constexpr uint16_t RX_BUFFER_SIZE = 4096;

  ESP8266(UART_HandleTypeDef *huart);

  void init(void);
  bool sendCommand(const char *cmd, const char *expected_response,
                   uint32_t timeout_ms);
  bool connectWiFi(const char *ssid, const char *password);
  bool sendData(const uint8_t *data, uint16_t len);
  bool startTCP(const char *host, uint16_t port);
  bool closeConnection(void);
  bool isConnected(void);
  int getState(void) { return _state; }
  bool isHardDisabled(void) { return _hard_disabled; }
  bool tryRecover(bool force = false);
  uint16_t recoveryFailureCount(void) const { return _recover_failures; }
  void clearRecoveryFailureCount(void) { _recover_failures = 0; }

  // Data processing, called from the interrupt path
  void processRxData(uint8_t *data, uint16_t len);

  // ESP8266 helper methods
  bool scanNetworks(void);
  void disconnect(void);
  bool getIP(char *ip_buffer, uint16_t buffer_size);
  bool getRSSI(int *rssi);
  bool sendString(const char *str);
  const char *getRxBuffer(void) const { return (const char *)_rx_buffer; }
  uint16_t getRxLength(void) const { return _rx_index; }
  uint16_t getRxCapacity(void) const { return RX_BUFFER_SIZE; }
  bool hasRxOverflow(void) const { return _rx_overflow; }

  void processPendingData(void);
  void resetRxBuffer(void);

private:
  UART_HandleTypeDef *_huart;
  int _state; // 0=disconnected, 1=joining, 2=TCP connected, 3=got IP
  bool _hard_disabled; // true = unavailable for this boot
  uint32_t _last_recover_ms;
  uint8_t _recover_attempts;
  uint16_t _recover_failures;
  uint8_t _rx_buffer[RX_BUFFER_SIZE];
  uint16_t _rx_index;
  bool _rx_overflow;

  void clearRxBuffer(void);
  bool waitForResponse(const char *expected, uint32_t timeout_ms);
  void parseResponse(const char *response);
};


extern ESP8266 esp8266;

#endif // __cplusplus

#endif // ESP8266_HPP
