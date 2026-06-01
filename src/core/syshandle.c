#include "include/syshandle.h"

#include "hardware/include/lcd.h"
#include "library/include/libpd.h"
#include "main.h"
#include <stdio.h>

static volatile uint32_t g_last_exception_code = SYS_ERR_NONE;
static uint32_t g_draw_code = SYS_ERR_NONE;
static int g_draw_seconds = 5;

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
  default: return "UNKNOWN";
  }
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
  PD_DrawString(18, 158, "The system will restart");
  PD_DrawString(18, 176, sec_line);

  /* Subtle square-screen friendly bottom bar. */
  PD_FillRect(18, 214, 204, 2, LCD_COLOR_GRAY);
}

void SysHandle_Exception(uint32_t code) {
  g_last_exception_code = code;
  g_draw_code = code;

  /* Draw before reset. Keep interrupts enabled so SysTick/SPI/RTOS-less HAL
   * timing can still work. Fatal callers should stop normal activity before
   * jumping here if needed.
   */
  for (g_draw_seconds = 5; g_draw_seconds >= 1; --g_draw_seconds) {
    LCD_FlushTiled(syshandle_render);
    HAL_Delay(1000);
  }

  NVIC_SystemReset();

  while (1) {
  }
}

void SysHandle_Fatal(uint32_t code) { SysHandle_Exception(code); }
