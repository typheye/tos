#include "syswatchdog.h"

#include "iwdg.h"
#include "hardware/include/lcd.h"
#include "syshandle.h"
#include "stm32f4xx_hal.h"
#include "syslog.h"
#include <stdio.h>

static uint32_t g_boot_code = SYS_ERR_NONE;
static uint8_t g_inited = 0;
static uint32_t g_last_feed_ms = 0;
static uint32_t g_last_display_service_ms = 0;

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

  service_display_background(now);
}

void SysWatchdog_ShowBootReasonIfAny(void) {
  if (!g_inited) SysWatchdog_Init();
  if (g_boot_code == SYS_ERR_NONE) return;

  uint32_t code = g_boot_code;
  g_boot_code = SYS_ERR_NONE;
  LOG_W("WDG", "Showing previous watchdog reset: 0x%08lX", (unsigned long)code);
  SysHandle_Exception(code);
}
