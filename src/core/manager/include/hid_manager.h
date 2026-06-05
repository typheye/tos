/**
 ******************************************************************************
 * @file    hid_manager.h
 * @author  Typheye
 * @brief   Cloud-facing HID command manager interface.
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

#ifndef HID_MANAGER_H
#define HID_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core/sys/include/syslog.h"
#include "core/sys/include/syswatchdog.h"
#include "include/libjson.h"
#include "main.h"
#include "usb_device.h"
#include "usbd_custom_hid_if.h"
#include "usbd_customhid.h"
#include "usbd_def.h"

#ifdef __cplusplus
extern "C" {
#endif

extern USBD_HandleTypeDef hUsbDeviceFS;

void HidManager_Init(void);
void HidManager_Tick(void);

bool HidManager_IsBusy(void);
bool HidManager_IsConfigured(void);
const char *HidManager_GetReportStatus(void);
const char *HidManager_GetLastError(void);

bool HidManager_QueueCloudCommand(const char *action, const char *json_obj,
                                  const char **err_out);

#ifdef __cplusplus
}
#endif

#endif /* HID_MANAGER_H */
