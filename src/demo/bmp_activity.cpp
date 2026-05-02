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

extern USART boardSerial;
extern BMP180 boardBMP180;
extern KeyManager keyManager;
extern LCD boardLCD;

// BMP180 菜单项定义
#define BMP_MENU_ITEMS 6
static const char *bmp_menus[BMP_MENU_ITEMS] = {
    "1. Single Read", "2. Continuous Monitor", "3. Altitude Demo",
    "4. Debug Info",  "5. Calibrate",          "6. Back"};

static int bmp_menu_select = 0;
// static int continuous_running = 0;
static float reference_pressure = 1013.25f;
static float altitude_offset = 0.0f;

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
  PD_DrawString(10, 4, "BMP180 Sensor");
}

// 绘制菜单
static void draw_menu(void) {
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 140);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 38, "Select Function:");

  for (int i = 0; i < BMP_MENU_ITEMS; i++) {
    int y = 58 + i * 20;

    if (i == bmp_menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 16);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }

    PD_DrawString(20, y, bmp_menus[i]);
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

// 显示单次读数
static void show_single_read(void) {
  char dbg[64];
  ;
  char fstr[16];

  PD_FillScreen(LCD_COLOR_BLACK);
  draw_status_bar();

  // 检查传感器
  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized()) {
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_RED);
      PD_DrawString(20, 50, "BMP180 Init Failed!");
      PD_DrawString(20, 70, "Check I2C Connection");
      PD_DrawString(20, 90, "Press Enter to Exit");
      LCD_Flush();

      while (1) {
        keyManager.btn_enter.tick();
        if (keyManager.btn_enter.getState() == KEY_PRESSED) {
          break;
        }
        HAL_Delay(50);
      }
      return;
    }
  }

  // 读取数据
  BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
  float altitude = boardBMP180.calcAltitude(data.pressure, reference_pressure) -
                   altitude_offset;

  // 显示数据
  PD_SetFont(FONT_ASCII_12);

  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(20, 40, "Temperature:");
  float_to_str(data.temperature, fstr);
  sprintf(dbg, "%s C", fstr);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(120, 40, dbg);

  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(20, 65, "Pressure:");
  float_to_str(data.pressure, fstr);
  sprintf(dbg, "%s hPa", fstr);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(120, 65, dbg);

  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(20, 90, "Altitude:");
  float_to_str(altitude, fstr);
  sprintf(dbg, "%s m", fstr);
  PD_SetColor(LCD_COLOR_GREEN);
  PD_DrawString(120, 90, dbg);

  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(20, 130, "Ref Pressure:");
  float_to_str(reference_pressure, fstr);
  sprintf(dbg, "%s hPa", fstr);
  PD_DrawString(120, 130, dbg);

  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(20, 150, "Altitude Offset:");
  float_to_str(altitude_offset, fstr);
  sprintf(dbg, "%s m", fstr);
  PD_DrawString(120, 150, dbg);

  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(20, 190, "Press Enter to return");

  LCD_Flush();

  // 等待按键返回
  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// 连续监测模式
static void show_continuous_monitor(void) {
  char fstr[16];
  char dbg[64];
  ;
  uint32_t last_update = HAL_GetTick();

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== BMP180 Continuous Monitor ==========\r\n");
  printf("Press Enter to stop\r\n");

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized()) {
      return;
    }
  }

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      printf("\r\nMonitor stopped\r\n");
      break;
    }

    if (HAL_GetTick() - last_update > 500) {
      last_update = HAL_GetTick();

      BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
      float altitude =
          boardBMP180.calcAltitude(data.pressure, reference_pressure) -
          altitude_offset;

      // 清屏并重绘
      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();

      PD_SetFont(FONT_ASCII_12);

      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(20, 40, "Continuous Monitor");

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 65, "Temperature:");
      float_to_str(data.temperature, fstr);
      sprintf(dbg, "%s C", fstr);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(120, 65, dbg);

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 90, "Pressure:");
      float_to_str(data.pressure, fstr);
      sprintf(dbg, "%s hPa", fstr);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(120, 90, dbg);

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 115, "Altitude:");
      float_to_str(altitude, fstr);
      sprintf(dbg, "%s m", fstr);
      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawString(120, 115, dbg);

      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(20, 160, "Press Enter to stop");

      LCD_Flush();

      // 串口输出
      float_to_str(data.temperature, fstr);
      printf("Temp: ");
      printf(fstr);
      printf(" C, ");
      float_to_str(data.pressure, fstr);
      printf("Press: ");
      printf(fstr);
      printf(" hPa\r\n");
    }

    HAL_Delay(50);
  }
}

// 海拔演示（包含校准功能）
static void show_altitude_demo(void) {
  char fstr[16];
  char dbg[64];
  ;
  float altitude_sum = 0;
  int altitude_count = 0;
  float current_altitude = 0;
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== BMP180 Altitude Demo ==========\r\n");
  printf("Press Enter to reset average\r\n");
  printf("Press A8+D0 to exit\r\n");

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized()) {
      return;
    }
  }

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // A8 + D0 同时按下退出
    if (keyManager.collision_A8.getState() == KEY_PRESSED &&
        keyManager.collision_D0.getState() == KEY_PRESSED) {
      printf("Altitude demo stopped\r\n");
      break;
    }

    // Enter 重置平均
    uint8_t current_enter =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter == 1 && last_enter_state == 0) {
      altitude_sum = 0;
      altitude_count = 0;
      printf("Average altitude reset\r\n");
    }
    last_enter_state = current_enter;

    if (HAL_GetTick() - last_update > 500) {
      last_update = HAL_GetTick();

      BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
      current_altitude =
          boardBMP180.calcAltitude(data.pressure, reference_pressure) -
          altitude_offset;

      altitude_sum += current_altitude;
      altitude_count++;
      float avg_altitude = altitude_sum / altitude_count;

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();

      PD_SetFont(FONT_ASCII_12);

      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(20, 40, "Altitude Demo");

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 65, "Current Alt:");
      float_to_str(current_altitude, fstr);
      sprintf(dbg, "%s m", fstr);
      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawString(120, 65, dbg);

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 90, "Average Alt:");
      float_to_str(avg_altitude, fstr);
      sprintf(dbg, "%s m", fstr);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(120, 90, dbg);

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 115, "Samples:");
      sprintf(dbg, "%d", altitude_count);
      PD_DrawString(120, 115, dbg);

      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(20, 140, "Ref Pressure:");
      float_to_str(reference_pressure, fstr);
      sprintf(dbg, "%s hPa", fstr);
      PD_DrawString(120, 140, dbg);

      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(20, 180, "Enter: Reset Avg");
      PD_DrawString(20, 195, "A8+D0: Exit");

      LCD_Flush();
    }

    HAL_Delay(50);
  }
}

// 显示调试信息
static void show_debug_info(void) {
  char dbg[64];
  ;
  int y = 40;

  PD_FillScreen(LCD_COLOR_BLACK);
  draw_status_bar();

  printf("\r\n========== BMP180 Debug Info ==========\r\n");

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized()) {
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_RED);
      PD_DrawString(20, 50, "BMP180 Init Failed!");
      LCD_Flush();

      while (1) {
        keyManager.btn_enter.tick();
        if (keyManager.btn_enter.getState() == KEY_PRESSED) {
          break;
        }
        HAL_Delay(50);
      }
      return;
    }
  }

  // 显示校准数据
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(20, y, "Calibration Data:");
  y += 16;

  sprintf(dbg, "AC1: %d", boardBMP180.getAC1());
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(25, y, dbg);
  y += 12;

  sprintf(dbg, "AC2: %d", boardBMP180.getAC2());
  PD_DrawString(25, y, dbg);
  y += 12;

  sprintf(dbg, "AC3: %d", boardBMP180.getAC3());
  PD_DrawString(25, y, dbg);
  y += 12;

  sprintf(dbg, "AC4: %u", boardBMP180.getAC4());
  PD_DrawString(25, y, dbg);
  y += 12;

  sprintf(dbg, "AC5: %u", boardBMP180.getAC5());
  PD_DrawString(25, y, dbg);
  y += 12;

  sprintf(dbg, "AC6: %u", boardBMP180.getAC6());
  PD_DrawString(25, y, dbg);
  y += 16;

  // 原始读数
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(20, y, "Raw Readings:");
  y += 16;

  int16_t raw_temp = boardBMP180.readRawTemp();
  sprintf(dbg, "Raw Temp: %d", raw_temp);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(25, y, dbg);
  y += 12;

  uint32_t raw_press = boardBMP180.readRawPressure(BMP180_MODE_STD);
  sprintf(dbg, "Raw Press: %lu", raw_press);
  PD_DrawString(25, y, dbg);

  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(20, 200, "Press Enter to exit");

  LCD_Flush();

  // 同时输出到串口
  boardBMP180.debugCalibration();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// 校准功能（设置参考气压和海拔偏移）
static void show_calibrate(void) {
  char fstr[16];
  char dbg[64];
  ;
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();
  int calibrate_step = 0; // 0: 显示状态, 1: 等待确认

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== BMP180 Calibration ==========\r\n");

  if (!boardBMP180.isInitialized()) {
    boardBMP180.init();
    if (!boardBMP180.isInitialized()) {
      return;
    }
  }

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    uint8_t current_enter =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;

    if (calibrate_step == 0) {
      // 显示当前读数并询问是否校准
      if (HAL_GetTick() - last_update > 500) {
        last_update = HAL_GetTick();

        BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
        float current_altitude =
            boardBMP180.calcAltitude(data.pressure, reference_pressure);

        PD_FillScreen(LCD_COLOR_BLACK);
        draw_status_bar();

        PD_SetFont(FONT_ASCII_12);
        PD_SetColor(LCD_COLOR_CYAN);
        PD_DrawString(20, 40, "Calibration");

        PD_SetColor(LCD_COLOR_GRAY);
        PD_DrawString(20, 65, "Current Readings:");

        float_to_str(data.pressure, fstr);
        sprintf(dbg, "Pressure: %s hPa", fstr);
        PD_SetColor(LCD_COLOR_YELLOW);
        PD_DrawString(25, 85, dbg);

        float_to_str(current_altitude, fstr);
        sprintf(dbg, "Altitude: %s m", fstr);
        PD_DrawString(25, 105, dbg);

        PD_SetColor(LCD_COLOR_CYAN);
        PD_DrawString(20, 135, "Set current altitude to 0?");
        PD_DrawString(20, 155, "Press Enter to calibrate");
        PD_DrawString(20, 175, "Press D0 to skip");

        LCD_Flush();
      }

      if (current_enter == 1 && last_enter_state == 0) {
        calibrate_step = 1;
      }

      if (keyManager.collision_D0.getState() == KEY_PRESSED) {
        printf("Calibration skipped\r\n");
        break;
      }
    } else if (calibrate_step == 1) {
      // 执行校准
      BMP180_Data_t data = boardBMP180.readData(BMP180_MODE_STD);
      altitude_offset =
          boardBMP180.calcAltitude(data.pressure, reference_pressure);

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawString(20, 40, "Calibration Complete!");

      float_to_str(altitude_offset, fstr);
      sprintf(dbg, "Altitude Offset: %s m", fstr);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(20, 70, dbg);

      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(20, 110, "Press Enter to continue");

      LCD_Flush();

      printf("Calibration complete. Offset: ");
      printf(fstr);
      printf(" m\r\n");

      calibrate_step = 2;
    } else if (calibrate_step == 2) {
      if (current_enter == 1 && last_enter_state == 0) {
        break;
      }
    }

    last_enter_state = current_enter;
    HAL_Delay(50);
  }
}

// BMP180 GUI 主菜单
void bmp180_gui_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  bmp_menu_select = 0;

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== BMP180 GUI Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // 菜单导航
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      bmp_menu_select++;
      if (bmp_menu_select >= BMP_MENU_ITEMS) {
        bmp_menu_select = BMP_MENU_ITEMS - 1;
      }
      HAL_Delay(150);
    }

    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (bmp_menu_select > 0) {
        bmp_menu_select--;
      }
      HAL_Delay(150);
    }

    // 执行选中的功能
    uint8_t current_enter_state =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter_state == 1 && last_enter_state == 0) {
      printf("Executing: ");
      printf(bmp_menus[bmp_menu_select]);
      printf("\r\n");

      switch (bmp_menu_select) {
      case 0:
        show_single_read();
        break;
      case 1:
        show_continuous_monitor();
        break;
      case 2:
        show_altitude_demo();
        break;
      case 3:
        show_debug_info();
        break;
      case 4:
        show_calibrate();
        break;
      case 5:
        printf("Exit BMP180 GUI\r\n");
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

// 保留原有函数供兼容
void bmp180_test_activity(void) { bmp180_gui_activity(); }

void bmp180_continuous_activity(void) { show_continuous_monitor(); }

void bmp180_altitude_activity(void) { show_altitude_demo(); }

void bmp180_debug_activity(void) { show_debug_info(); }