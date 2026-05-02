/**
 ******************************************************************************
 * @file    libdly.c
 * @author  Binchao Hu, Yutao Cheng
 * @brief   Unified delay library implementation
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 JanPNP Development Team. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "include/libdly.h"

#ifdef USE_SYSTEM_TICK
static volatile uint32_t system_tick = 0;

void SysTick_Handler(void) { system_tick++; }
#endif

void JPDelay(uint32_t ms) { HAL_Delay(ms); }

void JPDelayUs(uint32_t us) {
#if defined(USE_HAL_DELAY_US)
  HAL_Delay_us(us);
#else
  JPDelayUsBlocking(us);
#endif
}

void JPDelayUsBlocking(uint32_t us) {
  const uint32_t LOOP_PER_US = 42;

  uint32_t cycles = us * LOOP_PER_US;

  while (cycles--) {
    __NOP();
  }
}

uint32_t JPGetTick(void) {
#ifdef USE_SYSTEM_TICK
  return system_tick;
#else
  return HAL_GetTick();
#endif
}

void JPDelay_Init(void) {
#ifdef USE_SYSTEM_TICK
#endif
}
