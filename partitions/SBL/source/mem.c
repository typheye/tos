/**
 ******************************************************************************
 * @file    mem.c
 * @author  Typheye
 * @brief   SBL memory allocator implementation.
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
