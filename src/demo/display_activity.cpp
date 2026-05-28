#include "include/display_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include <stdio.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

#define DISPLAY_MENU_ITEMS 5
static const char *display_menus[DISPLAY_MENU_ITEMS] = {
    "01 Font Test", "02 Color Bars", "03 Checker",
    "04 Clear", "05 Back"};

static int display_menu_select = 0;

// ===== LVGL风格通用组件 =====
static void draw_status_bar(const char *title) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_bottom_hint(const char *left, const char *mid, const char *right) {
  PD_DrawFooterCenter(left, mid, right);
}

static void draw_subtitle(const char *text) {
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(TOS_GREY);
  PD_DrawString(16, 44, text);
}

// ===== 显示测试菜单 =====
void display_test_activity(void) {
  printf("\r\n========== Display Activity ==========\r\n");

  PD_Init();
  PD_FillScreen(LV_BG_DARK);
  draw_status_bar("06");

  // Font showcase card
  PD_DrawAngledCard(8, 44, 224, 125, 6, TOS_CARD_BG);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(TOS_GREY);
  PD_DrawString(16, 52, "Font 12  The quick brown fox jumps");

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_TEXT_SEC);
  PD_DrawString(16, 68, "Font 16  Over the lazy dog");

  PD_SetFont(FONT_ASCII_20);
  PD_SetColor(TOS_TEXT);
  PD_DrawString(16, 88, "Font 20  Hello World!");

  PD_SetFont(FONT_ASCII_24);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(16, 112, "Font 24  STM32");

  PD_SetFont(FONT_ASCII_32);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(16, 142, "Font 32  Hi!");

  // Shape showcase card
  PD_DrawAngledCard(8, 176, 224, 34, 6, TOS_CARD_BG);

  PD_SetColor(TOS_RED);
  PD_SetFill(true);
  PD_DrawRect(28, 185, 12, 12);
  PD_SetFill(false);

  PD_SetColor(TOS_GREEN);
  PD_DrawRoundRect(68, 183, 16, 16, 4);

  PD_SetColor(TOS_YELLOW);
  PD_DrawCircle(130, 191, 8);

  PD_SetColor(TOS_ACCENT);
  PD_DrawLine(168, 183, 198, 198);
  PD_DrawLine(198, 183, 168, 198);

  draw_bottom_hint("EXIT", NULL, NULL);

  LCD_Flush();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;
    HAL_Delay(50);
  }
}

void display_clear_activity(void) {
  PD_Init();
  PD_FillScreen(LV_BG_DARK);
  LCD_Flush();
}

void display_color_bars_activity(void) {
  PD_Init();

  int bar_w = 240 / 6;
  PD_SetFill(true);
  PD_SetColor(LV_ERROR);    PD_FillRect(0, 0, bar_w, 240, LV_ERROR);
  PD_SetColor(LV_PRIMARY);  PD_FillRect(bar_w, 0, bar_w, 240, LV_PRIMARY);
  PD_SetColor(LV_ACCENT);   PD_FillRect(bar_w*2, 0, bar_w, 240, LV_ACCENT);
  PD_SetColor(LV_SUCCESS);  PD_FillRect(bar_w*3, 0, bar_w, 240, LV_SUCCESS);
  PD_SetColor(LV_WARNING);  PD_FillRect(bar_w*4, 0, bar_w, 240, LV_WARNING);
  PD_SetColor(0xFF00FF);    PD_FillRect(bar_w*5, 0, bar_w, 240, 0xFF00FF);
  PD_SetFill(false);

  PD_DrawFooterCenter("EXIT", NULL, NULL);
  LCD_Flush();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;
    HAL_Delay(50);
  }
}

void display_pattern_activity(void) {
  PD_Init();
  PD_FillScreen(LV_BG_DARK);

  // 棋盘格 (使用卡片表面色和背景色)
  int bs = 20;
  for (int y = 0; y < 200; y += bs) {
    for (int x = 0; x < 240; x += bs) {
      bool w = ((x / bs) + (y / bs)) % 2 == 0;
      PD_SetColor(w ? LV_BG_CARD : LV_BG_DARK);
      PD_SetFill(true);
      PD_DrawRect(x, y, bs, bs);
    }
  }
  PD_SetFill(false);

  // 同心圆
  PD_SetColor(LV_ERROR);
  PD_DrawCircle(120, 100, 55);
  PD_DrawCircle(120, 100, 35);
  PD_DrawCircle(120, 100, 15);
  PD_SetColor(LV_ACCENT);
  PD_DrawCircle(120, 100, 5);

  draw_bottom_hint("EXIT", NULL, NULL);
  LCD_Flush();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;
    HAL_Delay(50);
  }
}

// ===== 统一菜单入口 =====
void display_test_menu_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  display_menu_select = 0;
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      display_menu_select++;
      if (display_menu_select >= DISPLAY_MENU_ITEMS)
        display_menu_select = DISPLAY_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (display_menu_select > 0) display_menu_select--;
      HAL_Delay(150);
    }

    uint8_t curr_enter = (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (curr_enter == 1 && last_enter_state == 0) {
      switch (display_menu_select) {
      case 0: display_test_activity(); break;
      case 1: display_color_bars_activity(); break;
      case 2: display_pattern_activity(); break;
      case 3: display_clear_activity(); break;
      case 4: return;
      }
      PD_FillScreen(LV_BG_DARK);
    }
    last_enter_state = curr_enter;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      draw_status_bar("06");

      PD_SetFont(FONT_ASCII_16);
      for (int i = 0; i < DISPLAY_MENU_ITEMS; i++) {
        int card_y = 28 + i * 25;
        if (i == display_menu_select) {
          PD_DrawAngledCard(14, card_y, 212, 20, 5, TOS_ACCENT);
          PD_SetColor(TOS_TEXT);
        } else {
          PD_DrawAngledCard(14, card_y, 212, 20, 5, TOS_CARD_BG);
          PD_SetColor(TOS_TEXT_SEC);
        }
        PD_DrawString(26, card_y + 2, display_menus[i]);
      }

      draw_bottom_hint("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}