#include "include/bmp_activity.hpp"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "main.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// CCMRAM 宏定义
#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern USART boardSerial;
extern BMP180 boardBMP180;
extern KeyManager keyManager;
extern LCD boardLCD;

// ==================== 菜单项（精简到4项）====================
#define BMP_MENU_ITEMS 4
static const char *bmp_menus[BMP_MENU_ITEMS] = {
    "1. Real-time Display", "2. Chart Mode", "3. Calibration", "4. Back"};

static int menu_select = 0;

// 全局校准参数
static float reference_pressure = 1013.25f;
static float altitude_offset = 0.0f;

// ==================== 图表数据缓冲区（CCMRAM）====================
#define CHART_HISTORY 120
static CCMRAM float chart_temp[CHART_HISTORY];
static CCMRAM float chart_press[CHART_HISTORY];
static CCMRAM int chart_index = 0;
static CCMRAM float temp_max = 50.0f, temp_min = -20.0f;
static CCMRAM float press_max = 1100.0f, press_min = 900.0f;
static CCMRAM int chart_mode = 0; // 0:温度, 1:气压

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
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 150);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 38, "Select Function:");

  for (int i = 0; i < BMP_MENU_ITEMS; i++) {
    int y = 58 + i * 22;

    if (i == menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 18);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }
    PD_DrawString(20, y, bmp_menus[i]);
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

// ==================== 图表绘制函数 ====================
static void draw_chart_axes(int x, int y, int width, int height, float max_val,
                            float min_val, const char *unit) {
  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawRect(x, y, width, height);

  // 水平网格线
  for (int i = 1; i <= 3; i++) {
    int line_y = y + (height * i / 4);
    PD_DrawLine(x, line_y, x + width, line_y);
  }

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);
  char label[16];
  snprintf(label, sizeof(label), "%.0f%s", max_val, unit);
  PD_DrawString(x - 30, y - 4, label);
  snprintf(label, sizeof(label), "%.0f%s", min_val, unit);
  PD_DrawString(x - 30, y + height - 4, label);
}

static void draw_chart_line(float *data, int x, int y, int width, int height,
                            float max_val, float min_val, uint32_t color) {
  PD_SetColor(color);
  float range = max_val - min_val;
  if (range < 0.01f)
    range = 1.0f;

  for (int i = 1; i < width && i < CHART_HISTORY; i++) {
    int idx_prev = (chart_index - 1 - i + CHART_HISTORY) % CHART_HISTORY;
    int idx_curr = (chart_index - i + CHART_HISTORY) % CHART_HISTORY;

    float val_prev = data[idx_prev];
    float val_curr = data[idx_curr];

    int y1 = y + height - (int)((val_prev - min_val) * height / range);
    int y2 = y + height - (int)((val_curr - min_val) * height / range);
    int x1 = x + width - i;
    int x2 = x + width - (i - 1);

    y1 = (y1 < y) ? y : (y1 > y + height) ? y + height : y1;
    y2 = (y2 < y) ? y : (y2 > y + height) ? y + height : y2;

    if (y1 >= y && y1 <= y + height && y2 >= y && y2 <= y + height) {
      PD_DrawLine(x1, y1, x2, y2);
    }
  }
}

static void update_chart_data(float temp, float press) {
  chart_temp[chart_index] = temp;
  chart_press[chart_index] = press;
  chart_index++;
  if (chart_index >= CHART_HISTORY)
    chart_index = 0;

  // 动态更新范围
  if (temp > temp_max)
    temp_max = temp + 2;
  if (temp < temp_min)
    temp_min = temp - 2;
  if (press > press_max)
    press_max = press + 20;
  if (press < press_min)
    press_min = press - 20;

  if (temp_max - temp_min > 100) {
    temp_max = temp_min + 100;
  }
  if (press_max - press_min > 500) {
    press_max = press_min + 500;
  }
}

static void reset_chart(void) {
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_temp[i] = 25.0f;
    chart_press[i] = 1013.25f;
  }
  chart_index = 0;
  temp_max = 50.0f;
  temp_min = -20.0f;
  press_max = 1100.0f;
  press_min = 900.0f;
}

// ==================== 实时显示模式 ====================
void bmp180_display_activity(void) {
  char fstr[16];
  char dbg[64];
  uint32_t last_update = HAL_GetTick();

  printf("\r\n========== BMP180 Real-time Display ==========\r\n");
  printf("Press Enter to exit\n");

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized()) {
      PD_FillScreen(LCD_COLOR_BLACK);
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_RED);
      PD_DrawString(20, 50, "BMP180 Init Failed!");
      PD_DrawString(20, 70, "Check I2C Connection");
      LCD_Flush();
      HAL_Delay(2000);
      return;
    }
  }

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      break;

    if (HAL_GetTick() - last_update > 200) {
      last_update = HAL_GetTick();

      BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
      float altitude =
          boardBMP180.calcAltitude(data.pressure, reference_pressure) -
          altitude_offset;

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("BMP180 Monitor");

      PD_SetFont(FONT_ASCII_16);

      // 温度
      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(20, 40, "Temperature:");
      float_to_str(data.temperature, fstr);
      sprintf(dbg, "%s C", fstr);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(150, 40, dbg);

      // 气压
      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(20, 70, "Pressure:");
      float_to_str(data.pressure, fstr);
      sprintf(dbg, "%s hPa", fstr);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(150, 70, dbg);

      // 海拔
      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(20, 100, "Altitude:");
      float_to_str(altitude, fstr);
      sprintf(dbg, "%s m", fstr);
      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawString(150, 100, dbg);

      // 参考值
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_GRAY);
      float_to_str(reference_pressure, fstr);
      sprintf(dbg, "Ref: %s hPa", fstr);
      PD_DrawString(20, 150, dbg);

      float_to_str(altitude_offset, fstr);
      sprintf(dbg, "Offset: %s m", fstr);
      PD_DrawString(20, 170, dbg);

      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(20, 210, "Enter: Exit");

      LCD_Flush();
    }
    HAL_Delay(50);
  }
}

// ==================== 图表模式 ====================
void bmp180_chart_activity(void) {
  uint32_t last_update = HAL_GetTick();
  uint8_t last_a8 = 0, last_d0 = 0;
  uint32_t last_mode_switch = 0;

  printf("\r\n========== BMP180 Chart Mode ==========\n");
  printf("A8: Switch T/P, D0: Reset, Enter: Exit\n");

  reset_chart();
  chart_mode = 0;

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized())
      return;
  }

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      break;

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    // A8: 切换温度/气压
    uint8_t current_a8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (current_a8 && !last_a8 && (HAL_GetTick() - last_mode_switch > 300)) {
      chart_mode = !chart_mode;
      last_mode_switch = HAL_GetTick();
    }
    last_a8 = current_a8;

    // D0: 重置图表
    uint8_t current_d0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (current_d0 && !last_d0) {
      reset_chart();
      printf("[CHART] Reset\n");
    }
    last_d0 = current_d0;

    // 读取数据
    BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
    update_chart_data(data.temperature, data.pressure);

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);

      char title[32];
      snprintf(title, sizeof(title), "BMP180 - %s",
               chart_mode ? "Pressure" : "Temperature");
      draw_status_bar(title);

      // 绘制图表
      int chart_x = 15, chart_y = 50, chart_w = 210, chart_h = 110;

      if (chart_mode == 0) {
        draw_chart_axes(chart_x, chart_y, chart_w, chart_h, temp_max, temp_min,
                        "C");
        draw_chart_line(chart_temp, chart_x, chart_y, chart_w, chart_h,
                        temp_max, temp_min, LCD_COLOR_GREEN);
      } else {
        draw_chart_axes(chart_x, chart_y, chart_w, chart_h, press_max,
                        press_min, "hPa");
        draw_chart_line(chart_press, chart_x, chart_y, chart_w, chart_h,
                        press_max, press_min, LCD_COLOR_CYAN);
      }

      // 当前数值
      PD_SetFont(FONT_ASCII_12);
      char dbg[64], fstr[16];
      PD_SetColor(LCD_COLOR_WHITE);
      float_to_str(chart_mode ? data.pressure : data.temperature, fstr);
      snprintf(dbg, sizeof(dbg), "Current: %s %s", fstr,
               chart_mode ? "hPa" : "C");
      PD_DrawString(15, 175, dbg);

      // 范围
      PD_SetColor(LCD_COLOR_GRAY);
      snprintf(dbg, sizeof(dbg), "Range: %.0f-%.0f",
               chart_mode ? press_min : temp_min,
               chart_mode ? press_max : temp_max);
      PD_DrawString(15, 195, dbg);

      // 提示
      PD_SetColor(LCD_COLOR_DARK_BLUE);
      PD_SetFill(true);
      PD_DrawRect(0, 215, 240, 25);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(10, 219, "A8:Switch");
      PD_DrawString(80, 219, "D0:Reset");
      PD_DrawString(140, 219, "Enter:Exit");

      LCD_Flush();
    }
    HAL_Delay(30);
  }
}

// ==================== 校准模式 ====================
void bmp180_calibrate_activity(void) {
  int step = 0;
  uint32_t last_update = HAL_GetTick();
  uint8_t last_enter = 0;
  float new_ref_pressure = reference_pressure;
  float new_altitude_offset = altitude_offset;
  char input_str[16] = {0};
  int input_pos = 0;

  printf("\r\n========== BMP180 Calibration ==========\n");

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized())
      return;
  }

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    keyManager.collision_D0.tick();
    keyManager.collision_A8.tick();

    uint8_t current_enter = (keyManager.btn_enter.getState() == KEY_PRESSED);
    uint8_t current_d0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    uint8_t current_a8 = (keyManager.collision_A8.getState() == KEY_PRESSED);

    if (current_d0 && step == 0) {
      printf("Calibration cancelled\n");
      break;
    }

    if (current_enter && !last_enter) {
      if (step == 0) {
        // 读取当前气压作为参考
        BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
        new_ref_pressure = data.pressure;
        step = 1;
      } else if (step == 1) {
        reference_pressure = new_ref_pressure;
        altitude_offset = new_altitude_offset;
        printf("Calibration saved: Ref=%.2f hPa\n", reference_pressure);
        break;
      }
    }
    last_enter = current_enter;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
      float current_alt =
          boardBMP180.calcAltitude(data.pressure, new_ref_pressure);

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("Calibration");

      PD_SetFont(FONT_ASCII_12);

      if (step == 0) {
        PD_SetColor(LCD_COLOR_YELLOW);
        PD_DrawString(20, 40, "Step 1: Set Reference");
        PD_SetColor(LCD_COLOR_WHITE);
        PD_DrawString(20, 65, "Current Pressure:");
        char fstr[16];
        float_to_str(data.pressure, fstr);
        PD_DrawString(20, 85, fstr);
        PD_DrawString(20, 85, " hPa");

        PD_SetColor(LCD_COLOR_GREEN);
        PD_DrawString(20, 120, "Press Enter to set");
        PD_SetColor(LCD_COLOR_GRAY);
        PD_DrawString(20, 145, "as reference (0m altitude)");
        PD_DrawString(20, 180, "D0: Skip");
      } else {
        PD_SetColor(LCD_COLOR_GREEN);
        PD_DrawString(20, 40, "Calibration Complete!");
        PD_SetColor(LCD_COLOR_WHITE);
        char dbg[64];
        sprintf(dbg, "Reference: %.1f hPa", new_ref_pressure);
        PD_DrawString(20, 70, dbg);
        sprintf(dbg, "Altitude offset: %.1f m", new_altitude_offset);
        PD_DrawString(20, 95, dbg);
        PD_DrawString(20, 140, "Press Enter to save");
      }

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 210, "Current altitude: ");
      char fstr[16];
      float_to_str(current_alt, fstr);
      PD_DrawString(140, 210, fstr);
      PD_DrawString(190, 210, "m");

      LCD_Flush();
    }
    HAL_Delay(50);
  }
}

// ==================== 主菜单 ====================
void bmp180_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  menu_select = 0;

  boardBMP180.init();
  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== BMP180 Activity Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= BMP_MENU_ITEMS)
        menu_select = BMP_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0)
        menu_select--;
      HAL_Delay(150);
    }

    uint8_t current_enter = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (current_enter && !last_enter_state) {
      switch (menu_select) {
      case 0:
        bmp180_display_activity();
        break;
      case 1:
        bmp180_chart_activity();
        break;
      case 2:
        bmp180_calibrate_activity();
        break;
      case 3:
        printf("Exit BMP180 Activity\n");
        return;
      }
      PD_FillScreen(LCD_COLOR_BLACK);
    }
    last_enter_state = current_enter;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();
      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar("BMP180 Sensor");
      draw_menu();
      draw_bottom_bar();
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}