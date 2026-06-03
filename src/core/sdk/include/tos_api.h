/**
 ******************************************************************************
 * @file    tos_api.h
 * @author  Typheye
 * @brief   Tos Api interface.
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

#ifndef TOS_API_H
#define TOS_API_H

#include <stdbool.h>
#include "core/include/config.h"
#include "core/manager/include/emotion_manager.h"
#include "core/manager/include/network_manager.h"
#include "core/manager/include/settings_manager.h"
#ifdef __cplusplus
#include "hardware/include/buzzer.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/esp8266.hpp"
#endif
#include "include/libjson.h"
#include "main.h"
#include "core/sys/include/systime.h"
#include "core/sys/include/syswatchdog.h"
#include "core/sys/include/syshandle.h"
#include "core/sys/include/syslog.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOS_API_HOST "proxy-api.otodone.com"
#define TOS_API_PORT 80
#define TOS_API_PATH "/tos/upgrade/check"
#define TOS_API_V1_BASE "/v1"

typedef struct {
  bool has_update;
  char latest_version[8];
  int  latest_version_code;
  char latest_build[16];
  char latest_patch[16];
  char latest_url[128];
  int  latest_size;
  char latest_sha256[72];
} TosUpgradeInfo;

/**
 * @brief  Check for firmware update from cloud API
 * @param  info  Output struct (only valid if returns true)
 * @return true on success, false on network/parse error
 */
bool TosApi_CheckUpgrade(TosUpgradeInfo *info);

/**
 * @brief  Initialize background cloud heartbeat / command handling.
 */
void TosApi_Init(void);

/**
 * @brief  Advance background cloud communication.
 *         Call frequently from UI/main loops. Never blocks on network I/O.
 */
void TosApi_Tick(void);

/**
 * @brief  Whether the background cloud client owns the ESP8266 now.
 */
bool TosApi_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif
