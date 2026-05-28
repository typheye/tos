#include "include/sn74hc00n_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/sn74hc00n.hpp"
#include <stdio.h>
#include <string.h>

extern KeyManager keyManager;
extern LCD boardLCD;
extern SN74HC00N boardHC00N;

#define HC00_MENU_ITEMS 4
static const char *hc00_menus[HC00_MENU_ITEMS] = {
    "01 Monitor", "02 Truth Table", "03 Logic", "04 Back"};

static int menu_select = 0;

static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, t);
}
static void bbar(const char *l, const char *m, const char *r) {
  PD_DrawFooterCenter(l, m, r);
}
static void menu_cards(int sel) {
  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < HC00_MENU_ITEMS; i++) {
    int cy = 28 + i * 25;
    if (i == sel) {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }
    PD_DrawString(26, cy + 2, hc00_menus[i]);
  }
}

void hc00n_monitor_activity(void) {
  uint32_t lu = HAL_GetTick();
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      uint8_t outputs = boardHC00N.readOutputByte();

      PD_FillScreen(LV_BG_DARK);
      bar("HC00N Monitor");

      PD_DrawAngledCard(8, 44, 224, 120, 6, TOS_CARD_BG);
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LV_TEXT_HINT);
      PD_DrawString(16, 52, "Channel   Output");
      PD_SetColor(LV_BORDER);
      PD_DrawLine(16, 70, 220, 70);

      for (int ch = 0; ch < 4; ch++) {
        int y = 80 + ch * 20;
        uint8_t actual = (outputs >> (3 - ch)) & 0x01;

        PD_SetColor(LV_ACCENT);
        char dbg[16];
        snprintf(dbg, sizeof(dbg), "  CH%d", ch + 1);
        PD_DrawString(20, y, dbg);

        if (actual) {
          PD_SetColor(LV_ERROR);
          PD_DrawString(140, y, "LOW");
        } else {
          PD_SetColor(LV_SUCCESS);
          PD_DrawString(140, y, "HIGH");
        }
      }

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LV_TEXT_HINT);
      PD_DrawString(16, 172, "Y = NOT (A AND B)");
      PD_DrawString(16, 188, "Y=LOW only when A=H AND B=H");

      bbar("EXIT", NULL, NULL);
      LCD_Flush();
    }
    HAL_Delay(50);
  }
}

void hc00n_truth_table_activity(void) {
  int step = 0;
  const uint8_t combos[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  const char *descriptions[4] = {"A=0,B=0: Y=1", "A=1,B=0: Y=1", "A=0,B=1: Y=1", "A=1,B=1: Y=0"};
  uint32_t lu = HAL_GetTick();

  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) { step++; if (step >= 4) step = 0; HAL_Delay(200); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) { step--; if (step < 0) step = 3; HAL_Delay(200); }

    if (HAL_GetTick() - lu > 50) {
      lu = HAL_GetTick();

      PD_FillScreen(LV_BG_DARK);
      bar("NAND Truth Table");

      PD_DrawAngledCard(8, 44, 224, 130, 6, TOS_CARD_BG);
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LV_TEXT_HINT);
      PD_DrawString(30, 52, "  A    B  |  Y");
      PD_SetColor(LV_BORDER);
      PD_DrawLine(24, 68, 210, 68);

      for (int i = 0; i < 4; i++) {
        int y = 80 + i * 22;
        uint8_t result = SN74HC00N::nandGate(combos[i][0], combos[i][1]);

        if (i == step) {
          PD_DrawAngledCard(22, y - 2, 190, 20, 4, TOS_ACCENT);
          PD_SetColor(TOS_TEXT);
        } else {
          PD_SetColor(LV_TEXT_PRIMARY);
        }

        char dbg[32];
        snprintf(dbg, sizeof(dbg), "  %d    %d    |    %d", combos[i][0], combos[i][1], result);
        PD_DrawString(30, y + 1, dbg);
      }

      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LV_ACCENT);
      PD_DrawString(16, 180, descriptions[step]);

      bbar("EXIT", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(50);
  }
}

void hc00n_test_activity(void) {
  uint32_t lu = HAL_GetTick();
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      uint8_t outputs = boardHC00N.readOutputByte();

      PD_FillScreen(LV_BG_DARK);
      bar("Logic Test");

      PD_DrawAngledCard(8, 44, 224, 100, 6, TOS_CARD_BG);
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LV_TEXT_HINT);
      PD_DrawString(30, 52, "Channel   Output");
      PD_SetColor(LV_BORDER);
      PD_DrawLine(20, 68, 220, 68);

      for (int ch = 0; ch < 4; ch++) {
        int y = 78 + ch * 20;
        uint8_t state = (outputs >> (3 - ch)) & 0x01;

        PD_SetColor(LV_TEXT_PRIMARY);
        char dbg[16];
        snprintf(dbg, sizeof(dbg), "  CH%d", ch + 1);
        PD_DrawString(30, y, dbg);

        if (state) {
          PD_SetColor(LV_ERROR);
          PD_DrawString(130, y, "LOW");
        } else {
          PD_SetColor(LV_SUCCESS);
          PD_DrawString(130, y, "HIGH");
        }
      }

      PD_SetFont(FONT_ASCII_20);
      PD_SetColor(LV_ACCENT);
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "Value: 0x%02X", outputs);
      PD_DrawString(16, 155, val_str);

      bbar("EXIT", NULL, NULL);
      LCD_Flush();
    }
    HAL_Delay(50);
  }
}

void hc00n_activity(void) {
  uint8_t le = 0;
  uint32_t lu = HAL_GetTick();

  PD_Init();
  menu_select = 0;
  boardHC00N.init();
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= HC00_MENU_ITEMS) menu_select = HC00_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0) menu_select--;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (menu_select) {
      case 0: hc00n_monitor_activity(); break;
      case 1: hc00n_truth_table_activity(); break;
      case 2: hc00n_test_activity(); break;
      case 3: return;
      }
      PD_FillScreen(LV_BG_DARK);
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      bar("10");
      menu_cards(menu_select);

      uint8_t preview = boardHC00N.readOutputByte();
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LV_ACCENT);
      char dbg[32];
      snprintf(dbg, sizeof(dbg), "Output: 0x%02X", preview);
      PD_DrawString(16, 160, dbg);

      bbar("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
