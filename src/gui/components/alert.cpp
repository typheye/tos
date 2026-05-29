#include "include/alert.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include <cstring>

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

      // Message — same top margin as first menu card (y=33)
      int msg_y = 33;
      PD_SetColor(TOS_TEXT);
      int len = strlen(msg);
      int max_chars = 22; // fits 240px screen at x=16 with 16px font
      if (len <= max_chars) {
        PD_DrawString(16, msg_y, msg);
      } else {
        char line[32];
        strncpy(line, msg, max_chars);
        line[max_chars] = '\0';
        PD_DrawString(16, msg_y, line);
        const char *rest = msg + max_chars;
        if (*rest == ' ') rest++;
        PD_DrawString(16, msg_y + 22, rest);
      }

      PD_DrawFooterCenter("ENTER", NULL, NULL);
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
