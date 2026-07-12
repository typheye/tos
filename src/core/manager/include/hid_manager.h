/**
 ******************************************************************************
 * @file    hid_manager.h
 * @author  Typheye
 * @brief   Cloud-facing HID command manager interface.
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
uint32_t HidManager_GetReportSeq(void);
bool HidManager_IsReportDirty(void);
void HidManager_ClearReportDirty(void);
void HidManager_ServiceTick(void);

bool HidManager_QueueCloudCommand(const char *action, const char *json_obj,
                                  const char **err_out);

#ifdef __cplusplus
}
#endif

#endif /* HID_MANAGER_H */
