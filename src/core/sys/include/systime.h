/**
 ******************************************************************************
 * @file    systime.h
 * @author  Typheye
 * @brief   Systime interface.
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

#ifndef SYSTIME_H
#define SYSTIME_H

#include <stdbool.h>
#include "core/manager/include/network_manager.h"
#include "core/manager/include/settings_manager.h"
#ifdef __cplusplus
#include "hardware/include/esp8266.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/trtc.hpp"
#endif
#include "core/sys/include/syslog.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Attempt NTP time sync via ESP8266 SNTP AT commands.
 *         Requires WiFi to be connected before calling.
 * @return true if time was synced and RTC updated, false otherwise.
 */
bool SysTime_Sync(void);

/**
 * @brief  Bounded background SNTP attempt used after the UI is running.
 *         No HTTP fallback and no long retry loop; failure is retried later.
 */
bool SysTime_SyncQuick(void);

/* Format time string respecting 24H/12H setting from Settings */
void SysTime_Fmt(char *buf, int sz, int h24, int m);

#ifdef __cplusplus
}
#endif

#endif
