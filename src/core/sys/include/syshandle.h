/**
 ******************************************************************************
 * @file    syshandle.h
 * @author  Typheye
 * @brief   System exception code interface.
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

#ifndef SYSHANDLE_H
#define SYSHANDLE_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"
#include "hardware/include/lcd.h"
#include "library/include/libpd.h"
#include "main.h"
#include "tim.h"
#include "core/sys/include/syslog.h"
#include "core/sys/include/syswatchdog.h"
#include <stdio.h>

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

#endif /* SYSHANDLE_H */
