/**
 ******************************************************************************
 * @file    libdly.h
 * @author  Typheye
 * @brief   Unified delay library interface.
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
