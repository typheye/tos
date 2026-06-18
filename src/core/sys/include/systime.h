/**
 ******************************************************************************
 * @file    systime.h
 * @author  Typheye
 * @brief   Systime interface.
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
void time_fmt(char *buf, int sz, int h24, int m);

#ifdef __cplusplus
}
#endif

#endif
