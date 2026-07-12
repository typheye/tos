/**
 ******************************************************************************
 * @file    mem.c
 * @author  Typheye
 * @brief   SBL memory allocator implementation.
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

#include "mem.h"
#include "dram.h"

SBL_CODE void SBL_MemReset(void) {
  SysDram_Init();
}

SBL_CODE void *SBL_MemAlloc(uint32_t size) {
  return SysDram_AllocFast(size);
}

SBL_CODE void *SBL_MemAllocDma(uint32_t size) {
  return SysDram_AllocDma(size);
}

SBL_CODE void SBL_MemFree(void *ptr) {
  SysDram_Free(ptr);
}
