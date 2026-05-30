/**
 * @file    systime.h
 * @brief   NTP time sync via ESP8266 AT commands
 */

#ifndef SYSTIME_H
#define SYSTIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Attempt NTP time sync via ESP8266 SNTP AT commands.
 *         Requires WiFi to be connected before calling.
 * @return true if time was synced and RTC updated, false otherwise.
 */
bool SysTime_Sync(void);

#ifdef __cplusplus
}
#endif

#endif
