/**
 ******************************************************************************
 * @file    mem.h
 * @author  Typheye
 * @brief   SBL memory allocator interface.
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
#ifndef SBL_MEM_H
#define SBL_MEM_H

#include "common.h"

SBL_CODE void SBL_MemReset(void);
SBL_CODE void *SBL_MemAlloc(uint32_t size);
SBL_CODE void *SBL_MemAllocDma(uint32_t size);
SBL_CODE void SBL_MemFree(void *ptr);

#endif /* SBL_MEM_H */
