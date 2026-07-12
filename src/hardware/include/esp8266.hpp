/**
 ******************************************************************************
 * @file    esp8266.hpp
 * @author  Typheye
 * @brief   ESP8266 AT driver interface.
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
 * @brief Perform one bounded ESP/UART recovery attempt.
 * @param force bypass the recovery rate limit.
 * @return true if the AT interface responds after recovery.
 */
bool ESP8266_TryRecover(bool force);
void ESP8266_ServiceUartRx(void);
uint32_t ESP8266_GetUartRxCount(void);
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
  bool isHardDisabled(void) { return _hardDisabled; }
  bool tryRecover(bool force = false);
  void serviceUartRx(void);
  uint16_t recoveryFailureCount(void) const { return _recoverFailures; }
  void clearRecoveryFailureCount(void) { _recoverFailures = 0; }

  // Data processing, called from the interrupt path
  void processRxData(uint8_t *data, uint16_t len);

  // ESP8266 helper methods
  bool scanNetworks(void);
  void disconnect(void);
  bool getIP(char *ip_buffer, uint16_t buffer_size);
  bool getRSSI(int *rssi);
  bool sendString(const char *str);
  const char *getRxBuffer(void) const { return (const char *)_rxBuffer; }
  uint16_t getRxLength(void) const { return _rxIndex; }
  uint16_t getRxCapacity(void) const { return RX_BUFFER_SIZE; }
  bool hasRxOverflow(void) const { return _rxOverflow; }

  void processPendingData(void);
  void resetRxBuffer(void);

private:
  UART_HandleTypeDef *_huart;
  int _state; // 0=disconnected, 1=joining, 2=TCP connected, 3=got IP
  bool _hardDisabled; // true = unavailable for this boot
  uint32_t _lastRecoverMs;
  uint8_t _recoverAttempts;
  uint16_t _recoverFailures;
  uint16_t _uartRearms;
  uint8_t _rxBuffer[RX_BUFFER_SIZE];
  uint16_t _rxIndex;
  bool _rxOverflow;

  void clearRxBuffer(void);
  void waitWithService(uint32_t delay_ms);
  void driveControlPins(bool en_high, bool rst_high);
  void hardwareReset(bool cycle_en, uint32_t boot_wait_ms);
  bool waitForResponse(const char *expected, uint32_t timeout_ms);
  void parseResponse(const char *response);
};


extern ESP8266 esp8266;

#endif // __cplusplus

#endif // ESP8266_HPP
