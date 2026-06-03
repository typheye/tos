/**
 ******************************************************************************
 * @file    syshandle.c
 * @author  Typheye
 * @brief   System exception screen implementation.
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

#include "syshandle.h"

#include "hardware/include/lcd.h"
#include "library/include/libpd.h"
#include "main.h"
#include "tim.h"
#include "syslog.h"
#include "syswatchdog.h"
#include <stdio.h>

/* C-compatible LED wrappers (defined in hardware/led.cpp) */
extern void LED_ErrorOn(void);
extern void LED_BoardOff(void);
extern void LED_WarnOff(void);

static volatile uint32_t g_last_exception_code = SYS_ERR_NONE;
static volatile uint8_t g_in_exception = 0;
static uint32_t g_draw_code = SYS_ERR_NONE;
static int g_draw_seconds = 6;

static void syshandle_force_reset(void) {
  /* NVIC_SystemReset() normally resets immediately.  If it is blocked for any
   * reason, do NOT keep refreshing IWDG here; let IWDG reset the chip.  The old
   * re-entry path refreshed IWDG forever and could leave the product on a black
   * screen. */
  __disable_irq();
  NVIC_SystemReset();
  while (1) {
    /* no feed: IWDG fallback */
  }
}

uint32_t SysHandle_GetLastCode(void) { return g_last_exception_code; }

const char *SysHandle_CodeName(uint32_t code) {
  switch (code) {
  case SYS_ERR_NONE: return "NONE";
  case SYS_ERR_SD_NOT_READY: return "SD_NOT_READY";
  case SYS_ERR_SD_TIMEOUT: return "SD_TIMEOUT";
  case SYS_ERR_SD_DISK_ERR: return "SD_DISK_ERR";
  case SYS_ERR_SD_LOST: return "SD_LOST";
  case SYS_ERR_SD_NO_FILESYSTEM: return "SD_NO_FILESYSTEM";
  case SYS_ERR_SD_FORMAT_FAILED: return "SD_FORMAT_FAILED";
  case SYS_ERR_SD_INIT_FAILED: return "SD_INIT_FAILED";
  case SYS_ERR_SD_BROWSER_FAILED: return "SD_BROWSER_FAILED";
  case SYS_ERR_SD_FILE_OP_FAILED: return "SD_FILE_OP_FAILED";
  case SYS_ERR_SD_PATH_TOO_LONG: return "SD_PATH_TOO_LONG";
  case SYS_ERR_UI_STORAGE_PROBE: return "UI_STORAGE_PROBE";
  case SYS_ERR_UI_FILE_MANAGER: return "UI_FILE_MANAGER";
  case SYS_ERR_UI_HID_TOOLS: return "UI_HID_TOOLS";
  case SYS_ERR_ESP8266_AT_TIMEOUT: return "ESP8266_AT_TIMEOUT";
  case SYS_ERR_ESP8266_RECOVERY_FAIL: return "ESP8266_RECOVERY_FAIL";
  case SYS_ERR_NET_TRANSPORT_STUCK: return "NET_TRANSPORT_STUCK";
  case SYS_ERR_IWDG_RESET: return "IWDG_RESET";
  case SYS_ERR_MAIN_LOOP_STALL: return "MAIN_LOOP_STALL";
  default: return "UNKNOWN";
  }
}

uint32_t SysHandle_CodeFromFResult(FRESULT res, uint32_t fallback) {
  switch (res) {
  case FR_OK: return SYS_ERR_NONE;
  case FR_NOT_READY: return SYS_ERR_SD_NOT_READY;
  case FR_TIMEOUT: return SYS_ERR_SD_TIMEOUT;
  case FR_DISK_ERR: return SYS_ERR_SD_DISK_ERR;
  case FR_INT_ERR: return SYS_ERR_SD_LOST;
  case FR_NO_FILESYSTEM: return SYS_ERR_SD_NO_FILESYSTEM;
  case FR_INVALID_NAME:
  case FR_INVALID_OBJECT:
  case FR_INVALID_PARAMETER:
    return SYS_ERR_SD_FILE_OP_FAILED;
  default: return fallback;
  }
}

bool SysHandle_IsStorageFatal(FRESULT res) {
  return res == FR_NOT_READY || res == FR_TIMEOUT || res == FR_DISK_ERR ||
         res == FR_INT_ERR;
}

void SysHandle_FatalFResult(FRESULT res, uint32_t fallback) {
  uint32_t code = SysHandle_CodeFromFResult(res, fallback);
  if (SysHandle_IsStorageFatal(res)) {
    LOG_E("SYSH", "fatal FatFs result: %d -> 0x%08lX %s", (int)res,
          (unsigned long)code, SysHandle_CodeName(code));
    SysHandle_Exception(code);
  }
}

extern TIM_HandleTypeDef htim4;

static void syshandle_prepare_display(void) {
  /* Fatal errors may be raised from a network/SPI path while the LCD is dimmed
   * or DMA is busy.  Use the emergency blocking path instead of full LCD_Init(),
   * otherwise the screen can stay black and IWDG may reset before the page is
   * drawn. */
  SysWatchdog_FeedNow();
  LCD_EmergencyPrepare();
  SysWatchdog_FeedNow();
}

static void syshandle_render(void) {
  char code_line[32];
  char sec_line[32];

  snprintf(code_line, sizeof(code_line), "Code: 0x%08lX",
           (unsigned long)g_draw_code);
  snprintf(sec_line, sizeof(sec_line), "in %d seconds", g_draw_seconds);

  PD_Init();
  PD_FillScreen(LCD_COLOR_BLACK);

  PD_SetBgColor(LCD_COLOR_BLACK);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_SetFont(FONT_ASCII_32);
  PD_DrawString(18, 28, ": )");

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(18, 82, "System Exception");

  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(18, 112, code_line);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(18, 150, SysHandle_CodeName(g_draw_code));
  PD_DrawString(18, 172, "The system will restart");
  PD_DrawString(18, 190, sec_line);

  /* Square-screen friendly bottom bar. */
  PD_FillRect(18, 214, 204, 2, LCD_COLOR_GRAY);
}

void SysHandle_Exception(uint32_t code) {
  if (g_in_exception) {
    /* A second fatal error while rendering syshandle usually means LCD/SPI/NET
     * code re-entered the exception path.  Reset immediately instead of
     * spinning forever. */
    g_last_exception_code = code;
    syshandle_force_reset();
  }
  g_in_exception = 1;

  g_last_exception_code = code;
  g_draw_code = code;

  /* Fatal LED policy: board/warn off, error LED on until reset. */
  LED_BoardOff();
  LED_WarnOff();
  LED_ErrorOn();

  LOG_F("SYSH", "System exception: 0x%08lX %s", (unsigned long)code,
        SysHandle_CodeName(code));

  syshandle_prepare_display();

  for (g_draw_seconds = 6; g_draw_seconds >= 1; --g_draw_seconds) {
    SysWatchdog_FeedNow();
    LCD_FlushTiledBlocking(syshandle_render);
    SysWatchdog_FeedNow();
    for (uint32_t i = 0; i < 1000U; i += 20U) {
      SysWatchdog_FeedNow();
      HAL_Delay(20);
    }
  }

  syshandle_force_reset();
}

void SysHandle_Fatal(uint32_t code) { SysHandle_Exception(code); }
