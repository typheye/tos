/**
 ******************************************************************************
 * @file    libdly.h
 * @author  Typheye
 * @brief   Unified delay library interface.
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

#ifndef LIBDLY_H
#define LIBDLY_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void JPDelay(uint32_t ms);
void JPDelayUs(uint32_t us);
void JPDelayUsBlocking(uint32_t us);
uint32_t JPGetTick(void);
void JPDelay_Init(void);
uint32_t JPDelay_ConsumeIdleMs(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBDLY_H */
