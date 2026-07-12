/**
 ******************************************************************************
 * @file    libdly.c
 * @author  Typheye
 * @brief   Unified delay library implementation.
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

#include "include/libdly.h"


#ifdef USE_SYSTEM_TICK
static volatile uint32_t system_tick = 0;

void SysTick_Handler(void) { system_tick++; }
#endif

static volatile uint32_t delay_idle_ms = 0;

static void delay_add_idle_ms(uint32_t ms) {
  if (ms == 0U) {
    return;
  }

  __disable_irq();
  delay_idle_ms += ms;
  __enable_irq();
}

void JPDelay(uint32_t ms) {
  uint32_t start = HAL_GetTick();
  HAL_Delay(ms);
  uint32_t elapsed = HAL_GetTick() - start;
  delay_add_idle_ms(elapsed);
}

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

uint32_t JPDelay_ConsumeIdleMs(void) {
  uint32_t idle;

  __disable_irq();
  idle = delay_idle_ms;
  delay_idle_ms = 0U;
  __enable_irq();

  return idle;
}

void JPDelay_Init(void) {
#ifdef USE_SYSTEM_TICK
#endif
}
