/**
 ******************************************************************************
 * @file    network_manager.h
 * @author  Typheye
 * @brief   Network Manager interface.
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

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#include "hardware/include/esp8266.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/led.hpp"
#endif
#include "include/syshandle.h"
#include "core/sys/include/syslog.h"
#include "core/sys/include/syswatchdog.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Check whether the network module (ESP8266) is hard-disabled.
 *         Same as ESP8266_IsHardDisabled(), re-exported for convenience.
 */
bool Net_IsHardDisabled(void);

/**
 * @brief  Prepare ESP8266 for a client-mode request.
 *         Closes stale connections, sets CIPMUX=0.
 *         Called internally by Net_HttpGet / Net_HttpRequest;
 *         exposed for callers that need explicit control.
 */
void Net_PrepareClient(void);

/**
 * @brief  Apply station settings for infrastructure AP compatibility.
 *         Disables modem sleep and selects active, compact +IPD delivery.
 *         Unsupported commands are ignored.
 */
void Net_ConfigureStationCompatibility(void);

/**
 * @brief  Lightweight cleanup for transient ESP8266 TCP stalls.
 *         Closes stale TCP state and restores single-connection mode without
 *         resetting or re-initializing the ESP8266.
 */
void Net_LightCleanup(void);

/**
 * @brief  Drop the cached DNS result so the next request resolves again.
 */
void Net_ResetDnsCache(void);

/**
 * @brief  Check whether the ESP8266 station has a valid IP.
 */
bool Net_HasStationIP(void);

/**
 * @brief  Apply LED feedback for a successful network operation.
 *         warnLed off, boardLed one 50 ms pulse.
 *         For callers that do their own low-level AT communication
 *         (e.g. SNTP time sync) and want consistent LED rules.
 */
void Net_LedSuccess(void);

/**
 * @brief  Apply LED feedback for a failed network operation.
 *         boardLed off, warnLed on until the next successful ESP operation.
 */
void Net_LedFailure(void);

/**
 * @brief  Perform an HTTP GET request through the ESP8266.
 *
 *         Opens a TCP connection to host:port, sends a minimal HTTP/1.0 GET
 *         request, collects the response, and closes the connection.
 *
 *         LED rules are applied automatically:
 *           success -> warnLed off, boardLed one 50 ms pulse
 *           failure -> boardLed off, warnLed held on
 *
 * @param  host        Target hostname (e.g. "www.baidu.com")
 * @param  port        TCP port (usually 80)
 * @param  path        Request path (e.g. "/getSysTime.do")
 * @param  resp_buf    Output buffer for the raw response
 * @param  resp_sz     Size of resp_buf
 * @param  timeout_ms  Total timeout (typically 8000–15000)
 * @return true on success (valid response received), false on any error.
 */
bool Net_HttpGet(const char *host, uint16_t port, const char *path,
                 char *resp_buf, uint16_t resp_sz, uint32_t timeout_ms);

/**
 * @brief  Perform an HTTP POST request through the ESP8266.
 *
 *         Same LED rules as Net_HttpGet.
 *
 * @param  host        Target hostname
 * @param  port        TCP port
 * @param  path        Request path
 * @param  body        POST body (JSON, form data, etc.)
 * @param  body_len    Length of body in bytes
 * @param  resp_buf    Output buffer for the raw response
 * @param  resp_sz     Size of resp_buf
 * @param  timeout_ms  Total timeout
 * @return true on success, false on any error.
 */
bool Net_HttpPost(const char *host, uint16_t port, const char *path,
                  const char *body, uint16_t body_len,
                  char *resp_buf, uint16_t resp_sz, uint32_t timeout_ms);

typedef enum {
  NET_ASYNC_IDLE = 0,
  NET_ASYNC_BUSY,
  NET_ASYNC_DONE,
  NET_ASYNC_FAILED
} NetAsyncState_t;

/**
 * @brief  Start a non-blocking HTTP POST request.
 *
 *         The caller must call Net_AsyncTick() frequently from UI/main loops.
 *         Only one async request can be active at a time.
 */
bool Net_AsyncHttpPostStart(const char *host, uint16_t port, const char *path,
                            const char *body, uint16_t body_len,
                            uint32_t timeout_ms);

/**
 * @brief  Advance the active async request state machine.
 */
void Net_AsyncTick(void);

/**
 * @brief  Current async request state.
 */
NetAsyncState_t Net_AsyncState(void);

/**
 * @brief  Raw HTTP response for a finished async request.
 *         Valid while state is NET_ASYNC_DONE.
 */
const char *Net_AsyncResponse(void);

/**
 * @brief  Clear a DONE/FAILED async request and return to IDLE.
 */
void Net_AsyncReset(void);

/**
 * @brief Consecutive failed async requests that received no UART bytes.
 */
uint8_t Net_AsyncNoRxFailStreak(void);

/**
 * @brief Clear async transport diagnostics after a successful ESP recovery.
 */
void Net_ResetTransportDiagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MANAGER_H */
