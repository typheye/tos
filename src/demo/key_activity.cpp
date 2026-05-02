#include "include/key_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include <stdio.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

// 保存上一次的状态，用于局部更新
static uint8_t last_sw1 = 0xFF;
static uint8_t last_sw2 = 0xFF;
static uint8_t last_sw3 = 0xFF;
static uint8_t last_sw4 = 0xFF;
static uint8_t last_sw5 = 0xFF;
static uint8_t last_sw6 = 0xFF;
static uint8_t last_sw7 = 0xFF;
static uint8_t last_sw8 = 0xFF;
static uint8_t last_sw9 = 0xFF;
static uint8_t last_sw10 = 0xFF;
static uint8_t last_sw11 = 0xFF;
static uint8_t last_sw12 = 0xFF;
static uint8_t last_sw13 = 0xFF;
static uint8_t last_coll_a8 = 0xFF;
static uint8_t last_coll_d0 = 0xFF;
static uint8_t last_btn = 0xFF;
static uint8_t last_sd = 0xFF;
static uint8_t last_buz = 0xFF;

// 清除指定区域的辅助函数
static void clear_area(int x, int y, int w, int h) {
  PD_SetColor(LCD_COLOR_BLACK);
  PD_SetFill(true);
  PD_DrawRect(x, y, w, h);
  PD_SetFill(false);
}

// 绘制状态栏
static void draw_status_bar(void) {
  // 顶部栏背景
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  // 标题
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "Key Test");

  // 右侧状态指示器
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_GREEN);
  PD_DrawString(200, 6, "RUN");
}

// 绘制组1标签 (SW1-SW4)
static void draw_group1_labels(void) {
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);

  PD_DrawString(10, 30, "SW1:");
  PD_DrawString(80, 30, "SW2:");
  PD_DrawString(150, 30, "SW3:");
  PD_DrawString(10, 46, "SW4:");
}

// 绘制组2标签 (SW5-SW9)
static void draw_group2_labels(void) {
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);

  PD_DrawString(10, 67, "SW5:");
  PD_DrawString(80, 67, "SW6:");
  PD_DrawString(150, 67, "SW7:");
  PD_DrawString(10, 83, "SW8:");
  PD_DrawString(80, 83, "SW9:");
}

// 绘制组3标签 (SW10-SW13)
static void draw_group3_labels(void) {
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);

  PD_DrawString(10, 104, "SW10:");
  PD_DrawString(80, 104, "SW11:");
  PD_DrawString(150, 104, "SW12:");
  PD_DrawString(10, 120, "SW13:");
}

// 绘制组4标签 (SD, Buzzer)
static void draw_group4_labels(void) {
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);

  PD_DrawString(10, 141, "SD Card:");
  PD_DrawString(80, 141, "Buzzer:");
}

// 绘制碰撞开关标签
static void draw_collision_labels(void) {
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);

  PD_DrawString(10, 162, "Collision A8:");
  PD_DrawString(130, 162, "Collision D0:");
}

// 更新组1显示
static void update_group1(void) {
  char dbg[16];

  uint8_t sw1 = keyManager.sw1_E0.isOn();
  uint8_t sw2 = keyManager.sw2_G13.isOn();
  uint8_t sw3 = keyManager.sw3_E2.isOn();
  uint8_t sw4 = keyManager.sw4_E4.isOn();

  if (sw1 != last_sw1) {
    clear_area(45, 30, 35, 12);
    sprintf(dbg, "%-3s", sw1 ? "ON" : "OFF");
    PD_SetColor(sw1 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(45, 30, dbg);
    last_sw1 = sw1;
  }
  if (sw2 != last_sw2) {
    clear_area(115, 30, 35, 12);
    sprintf(dbg, "%-3s", sw2 ? "ON" : "OFF");
    PD_SetColor(sw2 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(115, 30, dbg);
    last_sw2 = sw2;
  }
  if (sw3 != last_sw3) {
    clear_area(185, 30, 35, 12);
    sprintf(dbg, "%-3s", sw3 ? "ON" : "OFF");
    PD_SetColor(sw3 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(185, 30, dbg);
    last_sw3 = sw3;
  }
  if (sw4 != last_sw4) {
    clear_area(45, 46, 35, 12);
    sprintf(dbg, "%-3s", sw4 ? "ON" : "OFF");
    PD_SetColor(sw4 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(45, 46, dbg);
    last_sw4 = sw4;
  }
}

// 更新组2显示
static void update_group2(void) {
  char dbg[16];

  uint8_t sw5 = keyManager.sw5_D6.isOn();
  uint8_t sw6 = keyManager.sw6_G9.isOn();
  uint8_t sw7 = keyManager.sw7_G11.isOn();
  uint8_t sw8 = keyManager.sw8_G10.isOn();
  uint8_t sw9 = keyManager.sw9_G15.isOn();

  if (sw5 != last_sw5) {
    clear_area(45, 67, 35, 12);
    sprintf(dbg, "%-3s", sw5 ? "ON" : "OFF");
    PD_SetColor(sw5 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(45, 67, dbg);
    last_sw5 = sw5;
  }
  if (sw6 != last_sw6) {
    clear_area(115, 67, 35, 12);
    sprintf(dbg, "%-3s", sw6 ? "ON" : "OFF");
    PD_SetColor(sw6 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(115, 67, dbg);
    last_sw6 = sw6;
  }
  if (sw7 != last_sw7) {
    clear_area(185, 67, 35, 12);
    sprintf(dbg, "%-3s", sw7 ? "ON" : "OFF");
    PD_SetColor(sw7 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(185, 67, dbg);
    last_sw7 = sw7;
  }
  if (sw8 != last_sw8) {
    clear_area(45, 83, 35, 12);
    sprintf(dbg, "%-3s", sw8 ? "ON" : "OFF");
    PD_SetColor(sw8 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(45, 83, dbg);
    last_sw8 = sw8;
  }
  if (sw9 != last_sw9) {
    clear_area(115, 83, 35, 12);
    sprintf(dbg, "%-3s", sw9 ? "ON" : "OFF");
    PD_SetColor(sw9 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(115, 83, dbg);
    last_sw9 = sw9;
  }
}

// 更新组3显示
static void update_group3(void) {
  char dbg[16];

  uint8_t sw10 = keyManager.sw10_G3.isOn();
  uint8_t sw11 = keyManager.sw11_D15.isOn();
  uint8_t sw12 = keyManager.sw12_B12.isOn();
  uint8_t sw13 = keyManager.sw13_B14.isOn();

  if (sw10 != last_sw10) {
    clear_area(45, 104, 35, 12);
    sprintf(dbg, "%-3s", sw10 ? "ON" : "OFF");
    PD_SetColor(sw10 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(45, 104, dbg);
    last_sw10 = sw10;
  }
  if (sw11 != last_sw11) {
    clear_area(115, 104, 35, 12);
    sprintf(dbg, "%-3s", sw11 ? "ON" : "OFF");
    PD_SetColor(sw11 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(115, 104, dbg);
    last_sw11 = sw11;
  }
  if (sw12 != last_sw12) {
    clear_area(185, 104, 35, 12);
    sprintf(dbg, "%-3s", sw12 ? "ON" : "OFF");
    PD_SetColor(sw12 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(185, 104, dbg);
    last_sw12 = sw12;
  }
  if (sw13 != last_sw13) {
    clear_area(45, 120, 35, 12);
    sprintf(dbg, "%-3s", sw13 ? "ON" : "OFF");
    PD_SetColor(sw13 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(45, 120, dbg);
    last_sw13 = sw13;
  }
}

// 更新组4显示
static void update_group4(void) {
  char dbg[16];

  uint8_t sd = keyManager.isSdCardInserted();
  uint8_t buz = keyManager.isBuzzerEnabled();

  if (sd != last_sd) {
    clear_area(70, 141, 35, 12);
    sprintf(dbg, "%-3s", sd ? "IN" : "OUT");
    PD_SetColor(sd ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(70, 141, dbg);
    last_sd = sd;
  }
  if (buz != last_buz) {
    clear_area(130, 141, 35, 12);
    sprintf(dbg, "%-3s", buz ? "ON" : "OFF");
    PD_SetColor(buz ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(130, 141, dbg);
    last_buz = buz;
  }
}

// 更新碰撞开关显示
static void update_collision(void) {
  char dbg[16];

  uint8_t coll_a8 = keyManager.collision_A8.isPressed();
  uint8_t coll_d0 = keyManager.collision_D0.isPressed();

  if (coll_a8 != last_coll_a8) {
    clear_area(105, 162, 45, 12);
    sprintf(dbg, "%-5s", coll_a8 ? "PRE" : "REL");
    PD_SetColor(coll_a8 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(105, 162, dbg);
    last_coll_a8 = coll_a8;
  }
  if (coll_d0 != last_coll_d0) {
    clear_area(215, 162, 45, 12);
    sprintf(dbg, "%-5s", coll_d0 ? "PRE" : "REL");
    PD_SetColor(coll_d0 ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
    PD_DrawString(215, 162, dbg);
    last_coll_d0 = coll_d0;
  }
}

// 绘制底部提示栏
static void draw_bottom_bar(void) {
  // 底部栏背景
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 215, 240, 25);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(10, 219, "Enter: Exit");
  PD_DrawString(130, 219, "Auto Refresh");
}

void key_test_activity(void) {
  // 初始化 PD 图形库
  PD_Init();

  // 重置状态
  last_sw1 = 0xFF;
  last_sw2 = 0xFF;
  last_sw3 = 0xFF;
  last_sw4 = 0xFF;
  last_sw5 = 0xFF;
  last_sw6 = 0xFF;
  last_sw7 = 0xFF;
  last_sw8 = 0xFF;
  last_sw9 = 0xFF;
  last_sw10 = 0xFF;
  last_sw11 = 0xFF;
  last_sw12 = 0xFF;
  last_sw13 = 0xFF;
  last_coll_a8 = 0xFF;
  last_coll_d0 = 0xFF;
  last_btn = 0xFF;
  last_sd = 0xFF;
  last_buz = 0xFF;

  // 清屏并绘制静态内容
  PD_FillScreen(LCD_COLOR_BLACK);

  draw_status_bar();
  draw_group1_labels();
  draw_group2_labels();
  draw_group3_labels();
  draw_group4_labels();
  draw_collision_labels();
  draw_bottom_bar();

  // 初始刷新一次
  LCD_Flush();

  printf("\r\n========== Key Test GUI Started ==========\r\n");

  // 实时监测循环
  uint32_t last_update = HAL_GetTick();
  uint8_t last_btn_state = 0;

  while (1) {
    // 更新按键状态
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // 检查退出条件 - Enter 键
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      // 防抖处理
      if (last_btn_state == 0) {
        last_btn_state = 1;
        break;
      }
    } else {
      last_btn_state = 0;
    }

    // 每 50ms 更新一次显示（更快的响应）
    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      // 更新所有组的状态显示
      update_group1();
      update_group2();
      update_group3();
      update_group4();
      update_collision();

      // 刷新屏幕
      LCD_Flush();
    }

    HAL_Delay(10);
  }

  printf("Key Test Activity Exit\r\n");
}