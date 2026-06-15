/**
 ******************************************************************************
 * @file    syswatchdog.c
 * @author  Typheye
 * @brief   System watchdog service implementation.
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

#include "include/syswatchdog.h"
#include "core/manager/include/network_manager.h"
#include "core/manager/include/hid_manager.h"



extern void LED_ServiceTick(void);
extern void PD_SplashTick(void);

static uint32_t g_boot_code = SYS_ERR_NONE;
static uint8_t g_inited = 0;
static uint32_t g_last_feed_ms = 0;
static uint32_t g_last_display_service_ms = 0;
static uint32_t g_last_net_service_ms = 0;
static uint32_t g_last_hid_service_ms = 0;
static uint8_t g_net_service_guard = 0;
static uint8_t g_hid_service_guard = 0;

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


static void service_cloud_transport_background(uint32_t now) {
  /* Keep the non-blocking ESP8266 HTTP state machine moving even while the
   * foreground UI is inside another activity, a component delay loop, or a
   * long operation that only feeds the watchdog.  This only advances the raw
   * transport; TosApi_Tick() still owns response parsing and heartbeat/ACK
   * scheduling when the normal UI loop resumes.
   */
  if (!g_inited || g_net_service_guard) return;
  if ((uint32_t)(now - g_last_net_service_ms) < 20U) return;

  g_last_net_service_ms = now;
  g_net_service_guard = 1;
  Net_AsyncTick();
  g_net_service_guard = 0;
}


static void service_hid_background(uint32_t now) {
  /* Cloud HID commands may be queued from a heartbeat ACK path and are
   * intentionally non-blocking.  Run the small HID state machine from the
   * same watchdog heartbeat used by long UI/network loops, otherwise the
   * reported HID state can stay stale until the pet UI calls TosApi_Tick(). */
  if (!g_inited || g_hid_service_guard) return;
  if ((uint32_t)(now - g_last_hid_service_ms) < 5U) return;

  g_last_hid_service_ms = now;
  g_hid_service_guard = 1;
  HidManager_ServiceTick();
  g_hid_service_guard = 0;
}

static void service_display_background(uint32_t now) {
  /* Auto brightness used to be driven almost only by UI frame drawing.  During
   * long ESP8266/HTTP sections the UI may not redraw, so brightness appeared to
   * stop working.  SysWatchdog_Tick() is already called inside those long loops,
   * therefore we hook a low-rate display service here.
   *
   * LCD_UpdateAutoBrightness() has its own guards: it returns immediately when
   * auto brightness is off or the TCS3472 is not initialized.
   */
  if ((uint32_t)(now - g_last_display_service_ms) >= 1000U) {
    g_last_display_service_ms = now;
    LCD_UpdateAutoBrightness();
  }
}

void SysWatchdog_Init(void) {
  if (!g_inited) {
    g_boot_code = detect_reset_code();
    __HAL_RCC_CLEAR_RESET_FLAGS();
    g_inited = 1;
    g_last_feed_ms = HAL_GetTick();
    g_last_display_service_ms = g_last_feed_ms;
    g_last_net_service_ms = g_last_feed_ms;
    g_last_hid_service_ms = g_last_feed_ms;
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
  LED_ServiceTick();
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
  service_hid_background(now);
  service_display_background(now);
  service_cloud_transport_background(now);
}

void SysWatchdog_ShowBootReasonIfAny(void) {
  if (!g_inited) SysWatchdog_Init();
  if (g_boot_code == SYS_ERR_NONE) return;

  uint32_t code = g_boot_code;
  g_boot_code = SYS_ERR_NONE;
  LOG_W("WDG", "Showing previous watchdog reset: 0x%08lX", (unsigned long)code);
  SysHandle_Exception(code);
}
