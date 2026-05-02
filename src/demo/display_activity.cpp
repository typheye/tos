#include "include/display_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include <stdio.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

// 显示测试菜单项定义
#define DISPLAY_MENU_ITEMS 5
static const char *display_menus[DISPLAY_MENU_ITEMS] = {
    "1. Display Test", "2. Color Bars", "3. Checker Pattern", "4. Clear Screen",
    "5. Back"};

static int display_menu_select = 0;

// 清除指定区域
// static void clear_area(int x, int y, int w, int h) {
//   PD_SetColor(LCD_COLOR_BLACK);
//   PD_SetFill(true);
//   PD_DrawRect(x, y, w, h);
//   PD_SetFill(false);
// }

// 绘制状态栏
static void draw_status_bar(void) {
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "Display Tests");
}

// 绘制菜单
static void draw_menu(void) {
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 140);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 38, "Select Test:");

  for (int i = 0; i < DISPLAY_MENU_ITEMS; i++) {
    int y = 58 + i * 24;

    if (i == display_menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 20);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }

    PD_DrawString(20, y, display_menus[i]);
  }
}

// 绘制底部提示
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

// 原有测试函数保持不变
void display_test_activity(void) {
  printf("\r\n========== Display Activity ==========\r\n");

  PD_Init();
  PD_SetBgColor(LCD_COLOR_BLACK);
  PD_FillScreen(LCD_COLOR_BLACK);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(65, 5, "Display Test");

  PD_SetColor(LCD_COLOR_CYAN);
  PD_SetFont(FONT_ASCII_12);
  PD_DrawString(10, 30, "FONT_12 (6x12)");

  PD_SetFont(FONT_ASCII_16);
  PD_DrawString(10, 50, "FONT_16 (8x16)");

  PD_SetFont(FONT_ASCII_20);
  PD_DrawString(10, 75, "FONT_20 (10x20)");

  PD_SetFont(FONT_ASCII_24);
  PD_DrawString(10, 105, "FONT_24 (12x24)");

  PD_SetColor(LCD_COLOR_RED);
  PD_DrawCircle(60, 170, 25);

  PD_SetColor(LCD_COLOR_GREEN);
  PD_DrawRect(120, 145, 50, 50);

  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(200, 145, 35, 35);
  PD_SetFill(false);

  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawLine(10, 210, 230, 210);
  PD_DrawLine(10, 215, 230, 215);

  PD_SetColor(LCD_COLOR_MAGENTA);
  PD_DrawLine(0, 220, 239, 235);

  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawCircle(120, 170, 2);
  PD_DrawCircle(200, 162, 2);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(10, 230, "Enter: Exit");

  LCD_Flush();

  printf("Display test complete!\r\n");
  printf("========== Activity Complete ==========\r\n");

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

void display_clear_activity(void) {
  PD_Init();
  PD_FillScreen(LCD_COLOR_BLACK);
  LCD_Flush();
  printf("Screen cleared\r\n");
}

void display_color_bars_activity(void) {
  PD_Init();

  int bar_width = 240 / 6;

  PD_SetFill(true);

  PD_SetColor(LCD_COLOR_RED);
  PD_FillRect(0, 0, bar_width, 240, LCD_COLOR_RED);

  PD_SetColor(LCD_COLOR_GREEN);
  PD_FillRect(bar_width, 0, bar_width, 240, LCD_COLOR_GREEN);

  PD_SetColor(LCD_COLOR_BLUE);
  PD_FillRect(bar_width * 2, 0, bar_width, 240, LCD_COLOR_BLUE);

  PD_SetColor(LCD_COLOR_YELLOW);
  PD_FillRect(bar_width * 3, 0, bar_width, 240, LCD_COLOR_YELLOW);

  PD_SetColor(LCD_COLOR_CYAN);
  PD_FillRect(bar_width * 4, 0, bar_width, 240, LCD_COLOR_CYAN);

  PD_SetColor(LCD_COLOR_MAGENTA);
  PD_FillRect(bar_width * 5, 0, bar_width, 240, LCD_COLOR_MAGENTA);

  PD_SetFill(false);

  LCD_Flush();

  printf("Color bars displayed\r\n");

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

void display_pattern_activity(void) {
  PD_Init();

  int block_size = 20;
  bool white = true;

  for (int y = 0; y < 240; y += block_size) {
    for (int x = 0; x < 240; x += block_size) {
      PD_SetColor(white ? LCD_COLOR_WHITE : LCD_COLOR_BLACK);
      PD_SetFill(true);
      PD_DrawRect(x, y, block_size, block_size);
      white = !white;
    }
    white = !white;
  }

  PD_SetFill(false);

  PD_SetColor(LCD_COLOR_RED);
  PD_DrawCircle(120, 120, 40);

  PD_SetColor(LCD_COLOR_BLUE);
  PD_DrawCircle(120, 120, 30);

  LCD_Flush();

  printf("Pattern displayed\r\n");

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// 统一的显示测试菜单函数
void display_test_menu_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  display_menu_select = 0;

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== Display Test Menu Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // 菜单导航
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      display_menu_select++;
      if (display_menu_select >= DISPLAY_MENU_ITEMS) {
        display_menu_select = DISPLAY_MENU_ITEMS - 1;
      }
      HAL_Delay(150);
    }

    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (display_menu_select > 0) {
        display_menu_select--;
      }
      HAL_Delay(150);
    }

    // 执行选中的测试
    uint8_t current_enter_state =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter_state == 1 && last_enter_state == 0) {
      printf("Executing: ");
      printf(display_menus[display_menu_select]);
      printf("\r\n");

      switch (display_menu_select) {
      case 0:
        display_test_activity();
        break;
      case 1:
        display_color_bars_activity();
        break;
      case 2:
        display_pattern_activity();
        break;
      case 3:
        display_clear_activity();
        break;
      case 4:
        printf("Exit Display Test Menu\r\n");
        return;
      }
      // 刷新菜单界面
      PD_FillScreen(LCD_COLOR_BLACK);
    }
    last_enter_state = current_enter_state;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();
      draw_menu();
      draw_bottom_bar();
      LCD_Flush();
    }

    HAL_Delay(20);
  }
}