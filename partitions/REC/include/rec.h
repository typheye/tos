/**
 ******************************************************************************
 * @file    rec.h
 * @author  Typheye
 * @brief   REC mode dispatch interface.
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

#ifndef REC_H
#define REC_H

#include <stdint.h>

#define REC_CODE  __attribute__((section(".rec.text"), noinline, used))
#define REC_CONST __attribute__((section(".rec.rodata"), used))

REC_CODE void REC_Run(uint8_t mode);
void REC_Main(uint8_t clock_ok);
REC_CODE uint8_t REC_FatProbeInit(void);
REC_CODE uint8_t REC_FatHasUpgradeManifest(void);
REC_CODE uint8_t REC_FatFlashUpgrade(void (*status)(const char *, uint16_t));
REC_CODE uint8_t REC_FatFormat(void);
REC_CODE uint8_t REC_FatInitStorage(void);
REC_CODE void REC_FatRelease(void);
REC_CODE const char *REC_FatLastError(void);

/* REC TDB owns the SD filesystem only while servicing a command.  USB CDC
 * enumeration never waits for SD initialization. */
REC_CODE uint8_t REC_FsMount(void);
REC_CODE void REC_FsUnmount(void);
REC_CODE uint8_t REC_FsIsMounted(void);

#define REC_MODE_WAIT    0U
#define REC_MODE_FORMAT  1U
#define REC_MODE_UPGRADE 2U
#define REC_MODE_CLOCK_ERROR 3U
#define REC_MODE_INIT 4U

#endif /* REC_H */
