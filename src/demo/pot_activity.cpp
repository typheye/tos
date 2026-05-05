#include "include/pot_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/pot.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>


extern KeyManager keyManager;
extern LCD boardLCD;
extern Potentiometer boardPot;

static int menu_select = 0;

// 图表历史数据 (保存最近 240 个点，对应屏幕宽度)
#define CHART_WIDTH 240
#define CHART_HEIGHT 120
static uint16_t chart_data[CHART_WIDTH] = {0};
static int chart_index = 0;

// ==================== GUI 绘制函数 ====================

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
  const char *menus[] = {"1. Monitor Mode", "2. Chart Mode", "3. Calibrate",
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

// ==================== 绘制仪表盘 ====================

static void draw_gauge(float percentage, int x, int y, int radius) {
  // 绘制圆弧进度条
  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawCircle(x, y, radius);

  // 根据百分比填充
  int angle = (int)(percentage * 360 / 100);
  if (angle > 360)
    angle = 360;

  // 简单填充扇形 (用顶点越多越圆)
  for (int a = 0; a < angle; a += 10) {
    float rad = a * 3.14159f / 180.0f;
    int x1 = x + (int)((radius - 5) * cosf(rad));
    int y1 = y + (int)((radius - 5) * sinf(rad));
    PD_DrawLine(x, y, x1, y1);
  }

  // 显示百分比数字
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_GREEN);
  char dbg[16];
  snprintf(dbg, sizeof(dbg), "%.0f%%", percentage);
  PD_DrawString(x - 20, y - 8, dbg);
}

static void draw_bar(float percentage, int x, int y, int width, int height) {
  // 绘制进度条背景
  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawRect(x, y, width, height);

  // 绘制进度条填充
  int fill_width = (int)(width * percentage / 100);
  if (fill_width > 0) {
    PD_SetColor(LCD_COLOR_GREEN);
    PD_SetFill(true);
    PD_DrawRect(x + 1, y + 1, fill_width - 2, height - 2);
    PD_SetFill(false);
  }
}

static void draw_chart(uint16_t *data, int count, int x, int y, int width,
                       int height) {
  // 绘制坐标轴
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawLine(x, y, x + width, y);                   // 顶线
  PD_DrawLine(x, y + height, x + width, y + height); // 底线
  PD_DrawLine(x, y, x, y + height);                  // 左边线

  // 找到数据范围
  uint16_t max_val = 0;
  for (int i = 0; i < count && i < width; i++) {
    if (data[i] > max_val)
      max_val = data[i];
  }
  if (max_val == 0)
    max_val = 4095;

  // 绘制数据线
  PD_SetColor(LCD_COLOR_CYAN);
  for (int i = 1; i < width && i < count; i++) {
    int prev_x = x + i - 1;
    int prev_y = y + height - (int)((float)data[i - 1] * height / max_val);
    int curr_x = x + i;
    int curr_y = y + height - (int)((float)data[i] * height / max_val);
    PD_DrawLine(prev_x, prev_y, curr_x, curr_y);
  }
}

// ==================== 实时显示模式 ====================

void pot_monitor_activity(void) {
  uint32_t last_update = HAL_GetTick();
  char fstr[16];
  char dbg[64];

  printf("\r\n========== Potentiometer Monitor Mode ==========\r\n");
  printf("Rotate the potentiometer to see values change\n");
  printf("Press Enter to exit\n");

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      Pot_Data_t data = boardPot.readAll();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("Potentiometer Monitor");

      // 仪表盘 (左上)
      draw_gauge(data.percentage, 60, 70, 45);

      // 进度条 (右侧)
      draw_bar(data.percentage, 130, 45, 90, 20);

      // 数值显示
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_WHITE);

      // ADC 原始值
      snprintf(dbg, sizeof(dbg), "ADC: %4d", data.adc_raw);
      PD_DrawString(130, 80, dbg);

      // 电压
      float_to_str(data.voltage, fstr);
      snprintf(dbg, sizeof(dbg), "Voltage: %s V", fstr);
      PD_DrawString(130, 100, dbg);

      // 阻值
      float_to_str(data.resistance, fstr);
      snprintf(dbg, sizeof(dbg), "Resistance: %s kΩ", fstr);
      PD_DrawString(130, 120, dbg);

      // 百分比
      float_to_str(data.percentage, fstr);
      snprintf(dbg, sizeof(dbg), "Position: %s %%", fstr);
      PD_DrawString(130, 140, dbg);

      // 提示
      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 210, "Turn the potentiometer");
      PD_DrawString(150, 210, "Enter:Exit");

      LCD_Flush();

      // 串口输出
      static uint16_t last_raw = 0;
      if (abs(data.adc_raw - last_raw) > 10) {
        last_raw = data.adc_raw;
        printf("[POT] ADC=%4d, V=%.2fV, R=%.1fkΩ, %.0f%%\n", data.adc_raw,
               data.voltage, data.resistance, data.percentage);
      }
    }

    HAL_Delay(20);
  }

  printf("========== Monitor Exit ==========\n");
}

// ==================== 图表模式 ====================

void pot_chart_activity(void) {
  uint32_t last_update = HAL_GetTick();
  bool auto_scroll = true;
  uint8_t last_a8 = 0, last_d0 = 0;

  printf("\r\n========== Potentiometer Chart Mode ==========\n");
  printf("Linear chart showing ADC value over time\n");
  printf("A8: Reset chart, D0: Clear, Enter: Exit\n");

  // 重置图表数据
  for (int i = 0; i < CHART_WIDTH; i++) {
    chart_data[i] = 0;
  }
  chart_index = 0;

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    // A8: 重置图表
    uint8_t current_a8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (current_a8 && !last_a8) {
      for (int i = 0; i < CHART_WIDTH; i++) {
        chart_data[i] = 0;
      }
      chart_index = 0;
      printf("[CHART] Reset\n");
    }
    last_a8 = current_a8;

    // D0: 清屏但继续记录
    uint8_t current_d0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (current_d0 && !last_d0) {
      PD_FillScreen(LCD_COLOR_BLACK);
      printf("[CHART] Screen cleared\n");
    }
    last_d0 = current_d0;

    // 读取 ADC 值
    uint16_t adc_raw = boardPot.readRaw();

    // 更新图表数据
    chart_data[chart_index] = adc_raw;
    chart_index++;
    if (chart_index >= CHART_WIDTH) {
      chart_index = 0;
      // 滚动: 将数据左移
      if (auto_scroll) {
        for (int i = 0; i < CHART_WIDTH - 1; i++) {
          chart_data[i] = chart_data[i + 1];
        }
        chart_data[CHART_WIDTH - 1] = adc_raw;
        chart_index = CHART_WIDTH - 1;
      }
    }

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("Potentiometer Chart");

      // 绘制图表
      draw_chart(chart_data, CHART_WIDTH, 10, 40, 220, 100);

      // 当前值显示
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_YELLOW);
      char dbg[64];
      snprintf(dbg, sizeof(dbg), "Current ADC: %4d  (%.2fV)", adc_raw,
               (float)adc_raw * 3.3f / 4095.0f);
      PD_DrawString(10, 155, dbg);

      float percentage = (float)adc_raw * 100.0f / 4095.0f;
      snprintf(dbg, sizeof(dbg), "Position: %.1f%%", percentage);
      PD_DrawString(10, 175, dbg);

      // 进度条
      draw_bar(percentage, 10, 195, 220, 12);

      // 提示
      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 215, "A8:Reset D0:Clear");
      PD_DrawString(150, 215, "Enter:Exit");

      LCD_Flush();
    }

    HAL_Delay(20);
  }

  printf("========== Chart Mode Exit ==========\n");
}

// ==================== 校准模式 ====================

void pot_calibrate_activity(void) {
  int cal_step = 0;
  uint32_t last_update = HAL_GetTick();
  uint8_t last_enter = 0;

  printf("\r\n========== Potentiometer Calibration ==========\n");
  printf("Step 1: Rotate to MIN position, press Enter\n");
  printf("Step 2: Rotate to MAX position, press Enter\n");

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    keyManager.collision_D0.tick();

    uint8_t current_enter = (keyManager.btn_enter.getState() == KEY_PRESSED);

    // D0 退出
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      printf("[CAL] Calibration cancelled\n");
      break;
    }

    if (current_enter && !last_enter) {
      if (cal_step == 0) {
        boardPot.calibrateMin();
        cal_step = 1;
        printf("Step 2: Rotate to MAX position, press Enter\n");
      } else if (cal_step == 1) {
        boardPot.calibrateMax();
        cal_step = 2;
        printf("Calibration complete!\n");
        break;
      }
    }
    last_enter = current_enter;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      uint16_t raw = boardPot.readRaw();
      float voltage = (float)raw * 3.3f / 4095.0f;

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("Potentiometer Calibration");

      PD_SetFont(FONT_ASCII_12);

      if (cal_step == 0) {
        PD_SetColor(LCD_COLOR_YELLOW);
        PD_DrawString(20, 40, "Step 1: Set to MIN position");
        PD_DrawString(20, 60, "Rotate fully counter-clockwise");
      } else {
        PD_SetColor(LCD_COLOR_YELLOW);
        PD_DrawString(20, 40, "Step 2: Set to MAX position");
        PD_DrawString(20, 60, "Rotate fully clockwise");
      }

      PD_SetColor(LCD_COLOR_WHITE);
      char dbg[64];
      snprintf(dbg, sizeof(dbg), "Current ADC: %4d", raw);
      PD_DrawString(20, 100, dbg);

      snprintf(dbg, sizeof(dbg), "Voltage: %.2f V", voltage);
      PD_DrawString(20, 120, dbg);

      // 进度条显示当前值
      float percentage = (float)raw * 100.0f / 4095.0f;
      draw_bar(percentage, 20, 150, 200, 15);

      if (cal_step == 0) {
        PD_SetColor(LCD_COLOR_GREEN);
        PD_DrawString(20, 190, "Press Enter to set MIN");
      } else {
        PD_SetColor(LCD_COLOR_GREEN);
        PD_DrawString(20, 190, "Press Enter to set MAX");
      }

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 215, "D0: Cancel  Enter: Set");

      LCD_Flush();
    }

    HAL_Delay(50);
  }

  printf("========== Calibration Exit ==========\n");
}

// ==================== 主菜单 ====================

void pot_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  menu_select = 0;

  boardPot.init();

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== Potentiometer Activity Started ==========\n");
  printf("ADC: PC0, 12-bit resolution\n");

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
        pot_monitor_activity();
        break;
      case 1:
        pot_chart_activity();
        break;
      case 2:
        pot_calibrate_activity();
        break;
      case 3:
        printf("Exit Potentiometer Activity\n");
        return;
      }
      PD_FillScreen(LCD_COLOR_BLACK);
    }
    last_enter_state = current_enter_state;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("Potentiometer");
      draw_menu();
      draw_bottom_bar();

      // 显示实时预览
      uint16_t preview = boardPot.readRaw();
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_GREEN);
      char dbg[48];
      snprintf(dbg, sizeof(dbg), "ADC: %4d  (%.2fV)", preview,
               (float)preview * 3.3f / 4095.0f);
      PD_DrawString(10, 195, dbg);

      // 实时预览进度条
      float pct = (float)preview * 100.0f / 4095.0f;
      draw_bar(pct, 10, 208, 220, 8);

      LCD_Flush();
    }

    HAL_Delay(20);
  }
}