#include "include/key_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include <stdio.h>
#include "syslog.h"

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

void key_test_activity(void) {
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);

    PD_DrawFrame();
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, "01");

    // Switches card
    PD_DrawAngledCard(8, 44, 224, 50, 6, TOS_CARD_BG);
    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(TOS_GREY);
    PD_DrawString(16, 50, "SW1-SW6");
    PD_SetColor(TOS_TEXT_SEC);
    PD_DrawString(16, 66, "Press any switch to test");

    // Collision card
    PD_DrawAngledCard(8, 100, 224, 36, 6, TOS_CARD_BG);
    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(TOS_GREY);
    PD_DrawString(16, 106, "Collision A8 / D0");
    PD_SetColor(TOS_TEXT_SEC);
    PD_DrawString(16, 120, "Press Enter to exit");

    // Status area
    PD_DrawAngledCard(8, 144, 224, 40, 6, TOS_CARD_BG);
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(16, 154, "Status: Waiting...");

    // Bottom bar
    PD_DrawFooterCenter("EXIT", NULL, NULL);
  });

  LOG_I("KACT", "Key Test GUI started");

  uint8_t last_mask = 0xFF;
  uint32_t last_update = HAL_GetTick();

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    // 收集所有开关状态
    uint8_t mask = 0;
    if (keyManager.sw1_E0.isOn())  mask |= (1<<0);
    if (keyManager.sw2_G13.isOn()) mask |= (1<<1);
    if (keyManager.sw3_E2.isOn())  mask |= (1<<2);
    if (keyManager.sw4_E4.isOn())  mask |= (1<<3);
    if (keyManager.sw5_D6.isOn())  mask |= (1<<4);
    if (keyManager.sw6_G9.isOn())  mask |= (1<<5);

    bool coll_a8 = keyManager.collision_A8.isPressed();
    bool coll_d0 = keyManager.collision_D0.isPressed();
    bool sd_ok = keyManager.isSdCardInserted();

    uint8_t changed = (mask != last_mask);

    if (changed || (HAL_GetTick() - last_update > 200)) {
      last_update = HAL_GetTick();
      last_mask = mask;

      LCD_FLUSH({
        // Update Switches card
        PD_DrawAngledCard(8, 44, 224, 50, 6, TOS_CARD_BG);
        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(TOS_GREY);
        PD_DrawString(16, 50, "SW1-SW6");

        PD_SetFont(FONT_ASCII_16);
        for (int i = 0; i < 6; i++) {
          int sx = 20 + i * 36;
          if (mask & (1<<i)) {
            PD_SetColor(TOS_GREEN);
            PD_SetFill(true);
            PD_DrawRoundRect(sx, 62, 30, 22, 4);
            PD_SetFill(false);
            PD_SetColor(TOS_TEXT);
          } else {
            PD_SetColor(TOS_GREY);
            PD_DrawRoundRect(sx, 62, 30, 22, 4);
          }
          char lbl[4];
          sprintf(lbl, "%d", i+1);
          PD_DrawString(sx + 10, 65, lbl);
        }

        // Update Collision card
        PD_DrawAngledCard(8, 100, 224, 36, 6, TOS_CARD_BG);
        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(TOS_GREY);
        PD_DrawString(16, 106, "A8 / D0");
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(coll_a8 ? TOS_YELLOW : TOS_GREY);
        PD_DrawString(16, 120, coll_a8 ? "A8: ON " : "A8: -- ");
        PD_SetColor(coll_d0 ? TOS_YELLOW : TOS_GREY);
        PD_DrawString(110, 120, coll_d0 ? "D0: ON" : "D0: --");

        // Update status card
        PD_DrawAngledCard(8, 144, 224, 40, 6, TOS_CARD_BG);
        PD_SetFont(FONT_ASCII_16);
        char st[32];
        sprintf(st, "Mask: 0x%02X  SD:%s", mask, sd_ok ? "IN" : "OUT");
        PD_SetColor(TOS_ACCENT);
        PD_DrawString(16, 154, st);
      });
    }

    HAL_Delay(10);
  }
}