/**
 ******************************************************************************
 * @file    syswatchdog.c
 * @author  Typheye
 * @brief   System watchdog service implementation.
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

#include "include/syswatchdog.h"
extern void LED_ServiceTick(void);
extern void PD_SplashTick(void);

static uint32_t g_boot_code = SYS_ERR_NONE;
static uint8_t g_inited = 0;
static uint32_t g_last_feed_ms = 0;

static uint32_t detect_reset_code(void) {
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) return SYS_ERR_IWDG_RESET;
  return SYS_ERR_NONE;
}

static void feed_iwdg(void) {
#ifdef IWDG
  if (hiwdg.Instance == IWDG) {
    (void)HAL_IWDG_Refresh(&hiwdg);
  }
#endif
}


void SysWatchdog_Init(void) {
  if (!g_inited) {
    g_boot_code = detect_reset_code();
    __HAL_RCC_CLEAR_RESET_FLAGS();
    g_inited = 1;
    g_last_feed_ms = HAL_GetTick();
    if (g_boot_code != SYS_ERR_NONE) {
      LOG_W("WDG", "Previous reset: 0x%08lX %s", (unsigned long)g_boot_code,
            SysHandle_CodeName(g_boot_code));
    }
  }
  SysWatchdog_FeedNow();
}

uint32_t SysWatchdog_GetBootCode(void) { return g_boot_code; }

void SysWatchdog_FeedNow(void) {
  feed_iwdg();
  g_last_feed_ms = HAL_GetTick();
}

void SysWatchdog_Tick(void) {
  uint32_t now = HAL_GetTick();

  /* IWDG timeout is intentionally long (CubeMX: Prescaler=128, Reload=4095).
   * Feeding every 200 ms is enough and avoids doing extra work in hot loops.
   */
  if ((uint32_t)(now - g_last_feed_ms) >= 200U) {
    SysWatchdog_FeedNow();
  }

  LED_ServiceTick();
  PD_SplashTick();
}

void SysWatchdog_ShowBootReasonIfAny(void) {
  if (!g_inited) SysWatchdog_Init();
  if (g_boot_code == SYS_ERR_NONE) return;

  uint32_t code = g_boot_code;
  g_boot_code = SYS_ERR_NONE;
  if (code == SYS_ERR_IWDG_RESET) {
    LOG_W("WDG", "Previous IWDG reset recorded; skip fatal boot screen");
    return;
  }
  LOG_W("WDG", "Showing previous reset: 0x%08lX", (unsigned long)code);
  SysHandle_Exception(code);
}
