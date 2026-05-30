#include "include/alert.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include <cstring>
#include "syslog.h"

extern KeyManager keyManager;
extern LCD boardLCD;

void alert_show(const char *title, const char *msg) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  uint32_t lu = 0;

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) return;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      PD_Init();
      PD_FillScreen(TOS_BG);
      PD_DrawFrame();

      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(TOS_ACCENT);
      PD_DrawString(22, 5, title);

      // Message — split on \n, draw each line
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(TOS_TEXT);
      int msg_y = 33;
      const char *p = msg;
      while (*p && msg_y < 200) {
        /* Find end of this line (\n or \0) */
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        int len = eol - p;
        /* Truncate to screen width if needed */
        if (len > 22) len = 22;
        char line[24];
        memcpy(line, p, len); line[len] = '\0';
        PD_DrawString(16, msg_y, line);
        msg_y += 20;
        p = (*eol == '\n') ? eol + 1 : eol;
      }

      PD_DrawFooterCenter("ENTER", NULL, NULL);
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
