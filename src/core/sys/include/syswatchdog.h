/**
 ******************************************************************************
 * @file    syswatchdog.h
 * @author  Typheye
 * @brief   System watchdog service interface.
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

#ifndef __SYSWATCHDOG_H
#define __SYSWATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IWDG-only watchdog integration layer.
 * This layer refreshes IWDG, records IWDG reset reason, and provides a small
 * system-service tick used by long network loops.
 */
void SysWatchdog_Init(void);
void SysWatchdog_FeedNow(void);
void SysWatchdog_Tick(void);
void SysWatchdog_ShowBootReasonIfAny(void);
uint32_t SysWatchdog_GetBootCode(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSWATCHDOG_H */
