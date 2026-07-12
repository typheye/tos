/**
 ******************************************************************************
 * @file    mem.h
 * @author  Typheye
 * @brief   SBL memory allocator interface.
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
#ifndef SBL_MEM_H
#define SBL_MEM_H

#include "common.h"

SBL_CODE void SBL_MemReset(void);
SBL_CODE void *SBL_MemAlloc(uint32_t size);
SBL_CODE void *SBL_MemAllocDma(uint32_t size);
SBL_CODE void SBL_MemFree(void *ptr);

#endif /* SBL_MEM_H */
