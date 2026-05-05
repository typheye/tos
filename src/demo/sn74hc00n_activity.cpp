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

static int menu_select = 0;

// ==================== 通用 GUI 绘制函数 ====================

static void draw_status_bar(const char *title) {
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString((240 - strlen(title) * 8) / 2, 4, title);
}

static void draw_menu(void) {
  const char *menus[] = {"1. Monitor Mode", "2. Truth Table", "3. Logic Test",
                         "4. Back"};

  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 140);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 38, "Select Function:");

  for (int i = 0; i < 4; i++) {
    int y = 58 + i * 24;

    if (i == menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 20);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }

    PD_DrawString(20, y, menus[i]);
  }
}

static void draw_bottom_bar(void) {
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 215, 240, 25);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(10, 219, "A8:Down");
  PD_DrawString(75, 219, "D0:Up");
  PD_DrawString(130, 219, "Enter:OK");
}

// ==================== Monitor 模式 ====================

void hc00n_monitor_activity(void) {
  uint32_t last_update = HAL_GetTick();

  printf("\r\n========== SN74HC00N Monitor Mode ==========\r\n");
  printf("Logic: Y = NOT (A AND B)\r\n");
  printf("Press Enter to exit\r\n");

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      uint8_t outputs = boardHC00N.readOutputByte();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("HC00N Monitor");

      // 表格标题
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(20, 35, "Channel   Y");
      PD_DrawString(20, 50, "-------------");

      // 显示每个通道
      for (int ch = 0; ch < 4; ch++) {
        int y = 70 + ch * 25;
        uint8_t actual = (outputs >> (3 - ch)) & 0x01;

        PD_SetColor(LCD_COLOR_CYAN);
        char dbg[16];
        snprintf(dbg, sizeof(dbg), "  CH%d      ", ch + 1);
        PD_DrawString(20, y, dbg);

        // ========== 关键修改：输出翻转显示 ==========
        // actual=1(HIGH) 显示为 "LOW"
        // actual=0(LOW)  显示为 "HIGH"
        if (actual) {
          PD_SetColor(LCD_COLOR_RED);
          PD_DrawString(95, y, "LOW");
        } else {
          PD_SetColor(LCD_COLOR_GREEN);
          PD_DrawString(95, y, "HIGH");
        }
      }

      // 逻辑说明
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 175, "A and B are inputs from switches");
      PD_DrawString(10, 188, "Y = LOW only when A=H AND B=H");
      PD_DrawString(10, 201, "Otherwise Y = HIGH");

      // 输出值
      PD_SetColor(LCD_COLOR_WHITE);
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "Output Value: 0x%02X", outputs);
      PD_DrawString(10, 220, val_str);

      LCD_Flush();

      static uint8_t last_out = 0;
      if (outputs != last_out) {
        last_out = outputs;
        printf("[HC00N] Outputs: 0x%02X\r\n", outputs);
      }
    }

    HAL_Delay(50);
  }

  printf("========== Monitor Exit ==========\r\n");
}

// ==================== 真值表演示 ====================

void hc00n_truth_table_activity(void) {
  int step = 0;
  const uint8_t combos[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  const char *descriptions[4] = {"A=0, B=0", "A=1, B=0", "A=0, B=1",
                                 "A=1, B=1"};
  uint32_t last_update = HAL_GetTick();

  printf("\r\n========== NAND Truth Table ==========\r\n");
  printf("Press Enter to exit\n");

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      step++;
      if (step >= 4)
        step = 0;
      HAL_Delay(200);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      step--;
      if (step < 0)
        step = 3;
      HAL_Delay(200);
    }

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("NAND Truth Table");

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(30, 35, "  A    B   |   Y");
      PD_DrawString(30, 50, "----------------");

      for (int i = 0; i < 4; i++) {
        int y = 75 + i * 25;
        uint8_t result = SN74HC00N::nandGate(combos[i][0], combos[i][1]);

        PD_SetColor(LCD_COLOR_WHITE);
        char dbg[32];
        snprintf(dbg, sizeof(dbg), "  %d     %d    |    %d", combos[i][0],
                 combos[i][1], result);
        PD_DrawString(30, y, dbg);

        if (i == step) {
          PD_SetColor(LCD_COLOR_BLUE);
          PD_SetFill(true);
          PD_DrawRect(25, y - 2, 180, 18);
          PD_SetFill(false);
          PD_SetColor(LCD_COLOR_WHITE);
          PD_DrawString(30, y, dbg);
        }
      }

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_CYAN);
      char dbg[64];
      snprintf(dbg, sizeof(dbg), "Current: %s", descriptions[step]);
      PD_DrawString(10, 190, dbg);

      uint8_t result = SN74HC00N::nandGate(combos[step][0], combos[step][1]);
      snprintf(dbg, sizeof(dbg), "Y = %d (%s)", result,
               result ? "HIGH" : "LOW");
      PD_DrawString(10, 210, dbg);

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 225, "A8/D0: Navigate  Enter: Exit");

      LCD_Flush();
    }

    HAL_Delay(50);
  }

  printf("========== Truth Table Exit ==========\n");
}

// ==================== 逻辑测试模式 ====================

void hc00n_test_activity(void) {
  uint32_t last_update = HAL_GetTick();

  printf("\r\n========== HC00N Logic Test ==========\r\n");
  printf("Read actual outputs from PE10-PE13\n");
  printf("Press Enter to exit\n");

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      uint8_t outputs = boardHC00N.readOutputByte();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("Logic Test");

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(30, 35, "Channel   Output");
      PD_DrawString(30, 50, "----------------");

      for (int ch = 0; ch < 4; ch++) {
        int y = 70 + ch * 25;
        uint8_t state = (outputs >> (3 - ch)) & 0x01;

        PD_SetColor(LCD_COLOR_WHITE);
        char dbg[16];
        snprintf(dbg, sizeof(dbg), "  CH%d        ", ch + 1);
        PD_DrawString(30, y, dbg);

        // ========== 关键修改：输出翻转显示 ==========
        // state=1(HIGH) 显示为 "LOW"
        // state=0(LOW)  显示为 "HIGH"
        if (state) {
          PD_SetColor(LCD_COLOR_RED);
          PD_DrawString(120, y, "LOW");
        } else {
          PD_SetColor(LCD_COLOR_GREEN);
          PD_DrawString(120, y, "HIGH");
        }
      }

      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(LCD_COLOR_CYAN);
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "Value: 0x%02X", outputs);
      PD_DrawString(10, 185, val_str);

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 210, "Y = NOT (A AND B)");
      PD_DrawString(10, 222, "Output HIGH unless both inputs HIGH");

      PD_SetColor(LCD_COLOR_DARK_BLUE);
      PD_SetFill(true);
      PD_DrawRect(0, 215, 240, 25);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(130, 219, "Enter:Exit");

      LCD_Flush();

      static uint8_t last_out = 0;
      if (outputs != last_out) {
        last_out = outputs;
        printf("[TEST] Outputs: 0x%02X | ", outputs);
        for (int i = 3; i >= 0; i--) {
          printf("%s", (outputs >> i) & 0x01 ? "LOW " : "HIGH ");
        }
        printf("\r\n");
      }
    }

    HAL_Delay(50);
  }

  printf("========== Test Exit ==========\n");
}

// ==================== 主菜单 ====================

void hc00n_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  menu_select = 0;

  boardHC00N.init();

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== SN74HC00N Activity Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= 4)
        menu_select = 3;
      HAL_Delay(150);
    }

    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0)
        menu_select--;
      HAL_Delay(150);
    }

    uint8_t current_enter_state =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter_state == 1 && last_enter_state == 0) {
      switch (menu_select) {
      case 0:
        hc00n_monitor_activity();
        break;
      case 1:
        hc00n_truth_table_activity();
        break;
      case 2:
        hc00n_test_activity();
        break;
      case 3:
        printf("Exit SN74HC00N Activity\n");
        return;
      }
      PD_FillScreen(LCD_COLOR_BLACK);
    }
    last_enter_state = current_enter_state;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("SN74HC00N");
      draw_menu();
      draw_bottom_bar();

      uint8_t preview = boardHC00N.readOutputByte();
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_GREEN);
      char dbg[32];
      snprintf(dbg, sizeof(dbg), "Output: 0x%02X", preview);
      PD_DrawString(10, 195, dbg);

      LCD_Flush();
    }

    HAL_Delay(20);
  }
}