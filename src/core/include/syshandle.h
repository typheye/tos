/**
 ******************************************************************************
 * @file    syshandle.h
 * @author  Typheye
 * @brief   System exception code interface.
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

#ifndef __SYSHANDLE_H
#define __SYSHANDLE_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/* System exception/error codes.
 * Keep values stable: these codes are shown on the fatal UI and can be used
 * later for log upload or customer support.
 */
#define SYS_ERR_NONE                  0x00000000UL
#define SYS_ERR_SD_NOT_READY          0x00001001UL
#define SYS_ERR_SD_TIMEOUT            0x00001002UL
#define SYS_ERR_SD_DISK_ERR           0x00001003UL
#define SYS_ERR_SD_LOST               0x00001004UL
#define SYS_ERR_SD_NO_FILESYSTEM      0x00001005UL
#define SYS_ERR_SD_FORMAT_FAILED      0x00001006UL
#define SYS_ERR_SD_INIT_FAILED        0x00001007UL
#define SYS_ERR_SD_BROWSER_FAILED     0x00001008UL
#define SYS_ERR_SD_FILE_OP_FAILED     0x00001009UL
#define SYS_ERR_SD_PATH_TOO_LONG      0x0000100AUL
#define SYS_ERR_SD_LOG_FAILED         0x0000100BUL

#define SYS_ERR_UI_STORAGE_PROBE      0x00002001UL
#define SYS_ERR_UI_FILE_MANAGER       0x00002002UL
#define SYS_ERR_UI_HID_TOOLS          0x00002003UL

#define SYS_ERR_ESP8266_AT_TIMEOUT     0x00003001UL
#define SYS_ERR_ESP8266_RECOVERY_FAIL  0x00003002UL
#define SYS_ERR_NET_TRANSPORT_STUCK    0x00003003UL

#define SYS_ERR_IWDG_RESET             0x00004002UL
#define SYS_ERR_MAIN_LOOP_STALL        0x00004004UL

/* Show the fatal exception UI, count down 5 seconds, then reset.
 * This function does not return under normal conditions.
 */
void SysHandle_Exception(uint32_t code);
void SysHandle_ExceptionNoDump(uint32_t code);
void SysHandle_Fatal(uint32_t code);
uint32_t SysHandle_GetLastCode(void);
bool SysHandle_IsInException(void);
const char *SysHandle_CodeName(uint32_t code);

uint32_t SysHandle_CodeFromFResult(FRESULT res, uint32_t fallback);
bool SysHandle_IsStorageFatal(FRESULT res);
void SysHandle_FatalFResult(FRESULT res, uint32_t fallback);

#ifdef __cplusplus
}
#endif

#endif /* __SYSHANDLE_H */
